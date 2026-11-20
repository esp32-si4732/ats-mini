#include "Common.h"
#include "Menu.h"
#include "Utils.h"
#include "RemoteKenwood.h"
#include "ParserKenwood.h"

void remoteKenwoodError(Stream& stream)
{
  stream.print("?;");
}

static uint8_t catMode()
{
  switch(currentMode)
  {
    case LSB: return 1;
    case USB: return 2;
    case FM:  return 4;
    default:  return 5;
  }
}

static void catStatus(Stream& stream)
{
  // TS-480 IF: no RIT/XIT, memory, transmit, split, scan or tone operation.
  stream.printf("IF%011llu     +0000000000%u%u00000 ;",
                (unsigned long long)currentFrequency, catMode(), vfoIdx);
}

static unsigned toCatLevel(unsigned level, unsigned maximum)
{
  return (level * 255 + maximum / 2) / maximum;
}

static int fromCatLevel(unsigned level, unsigned maximum)
{
  return (level * maximum + 127) / 255;
}

static int catSignalStrength()
{
  rx.getCurrentReceivedSignalQuality();
  // RSSI is in dBuV; nominal S9 is 34 dBuV on HF and 14 dBuV on VHF.
  const int aboveS9 = (int)rx.getCurrentRSSI() - (currentMode == FM ? 14 : 34);
  // Invert Hamlib's TS480_STR_CAL, rounding to the nearest meter bar:
  // https://github.com/Hamlib/Hamlib/blob/master/rigs/kenwood/ts480.c
  // 0=-60 dB, 3=-48 dB, 11=S9, 14=+20 dB, 17=+40 dB, 20=+60 dB.
  if(aboveS9 <= -60) return 0;
  if(aboveS9 < -48) return ((aboveS9 + 60) * 3 + 6) / 12;
  if(aboveS9 < 0) return 3 + (aboveS9 + 48 + 3) / 6;
  return min(20, 11 + (aboveS9 * 3 + 10) / 20);
}

void remoteKenwoodTick(Stream& stream, KenwoodState& state)
{
  if(!state.autoInformation) return;
  const uint32_t frequencyA = getVfo(0).freq;
  const uint32_t frequencyB = getVfo(1).freq;
  const uint8_t mode = catMode();
  const uint8_t squelch = currentSquelch[currentMode] & 0x7f;
  if(currentFrequency != (vfoIdx ? state.lastFrequencyB : state.lastFrequencyA) ||
     mode != state.lastMode || vfoIdx != state.lastVfo)
    state.statusPending = true;

  if(state.autoInformation & 2)
  {
    if(!state.lastMode) stream.print("PS1;");
    if(frequencyA != state.lastFrequencyA) stream.printf("FA%011llu;", (unsigned long long)frequencyA);
    if(frequencyB != state.lastFrequencyB) stream.printf("FB%011llu;", (unsigned long long)frequencyB);
    if(mode != state.lastMode) stream.printf("MD%u;", mode);
    if(vfoIdx != state.lastVfo) stream.printf("FR%u;FT%u;", vfoIdx, vfoIdx);
    if(volume != state.lastVolume) stream.printf("AG0%03u;", toCatLevel(volume, 63));
    if(squelch != state.lastSquelch) stream.printf("SQ0%03u;", toCatLevel(squelch, 127));
  }
  state.lastFrequencyA = frequencyA;
  state.lastFrequencyB = frequencyB;
  state.lastMode = mode;
  state.lastVfo = vfoIdx;
  state.lastVolume = volume;
  state.lastSquelch = squelch;

  // Extended reports must not consume changes waiting for the legacy interval.
  const uint32_t now = millis();
  if((state.autoInformation & 1) && state.statusPending && now - state.reportTimer >= 1500)
  {
    catStatus(stream);
    state.reportTimer = now;
    state.statusPending = false;
  }
}

static bool remoteKenwoodCommand(Stream& stream, KenwoodState& state, const KenwoodCommand& command)
{
  using Id = KenwoodCommand::Id;
  const bool read = command.action == KenwoodCommand::Action::Read;
  switch(command.id)
  {
    case Id::Identity:
      stream.print("ID020;");
      return false;
    case Id::Status:
      catStatus(stream);
      return false;
    case Id::AutoInformation:
      if(read) stream.printf("AI%u;", state.autoInformation);
      else
      {
        state = KenwoodState{};
        state.autoInformation = command.value;
      }
      return false;
    case Id::FilterWidth:
      if(read) stream.print("FW0000;"); // Normal filter; writes leave bandwidth unchanged.
      return false;
    case Id::Power:
      if(read) { stream.print("PS1;"); return false; }
      if(command.value == 1) return false; // Already powered on.
      break; // Power-off could disable the transport needed to wake the receiver.
    case Id::Receive:
      if(state.autoInformation & 2) stream.print("RX0;");
      return false;
    case Id::Volume:
    {
      if(read) { stream.printf("AG0%03u;", toCatLevel(volume, 63)); return false; }
      const int target = fromCatLevel(command.value, 63);
      if(target == volume) return false;
      doVolume(target - volume);
      return true;
    }
    case Id::Squelch:
    {
      const int level = currentSquelch[currentMode] & 0x7f;
      if(read) { stream.printf("SQ0%03u;", toCatLevel(level, 127)); return false; }
      const int target = fromCatLevel(command.value, 127);
      if(target == level) return false;
      doSquelch(target - level); // Preserve the RSSI/SNR selection bit.
      return true;
    }
    case Id::SignalStrength:
      stream.printf("SM0%04d;", catSignalStrength());
      return false;
    case Id::Frequency:
    case Id::FrequencyB:
    {
      const uint8_t index = command.id == Id::Frequency ? 0 : 1;
      if(read)
      {
        const uint32_t freq = getVfo(index).freq;
        stream.printf("F%c%011llu;", index ? 'B' : 'A', (unsigned long long)freq);
        return false;
      }
      if(command.value <= UINT32_MAX && setVfoFrequency(index, command.value))
        return index == vfoIdx;
      break;
    }
    case Id::ReceiveVfo:
      if(read) { stream.printf("FR%u;", vfoIdx); return false; }
      if(selectVfo(command.value)) return true;
      break;
    case Id::StepUp:
    case Id::StepDown:
    {
      if(!command.value) return false;
      const int64_t delta = getCurrentStep()->step * (int64_t)command.value;
      const int64_t freq = (int64_t)currentFrequency + (command.id == Id::StepUp ? delta : -delta);
      if(freq >= 0 && freq <= UINT32_MAX && setVfoFrequency(vfoIdx, freq)) return true;
      break;
    }
    case Id::CopyVfo:
      copyVfo();
      return false;
    case Id::TransmitVfo:
      if(read) { stream.printf("FT%u;", vfoIdx); return false; }
      if(command.value == vfoIdx) return false; // Simplex compatibility only.
      break;
    case Id::Mode:
    {
      if(read) { stream.printf("MD%u;", catMode()); return false; }
      uint8_t mode;
      switch(command.value)
      {
        case 1: mode = LSB; break;
        case 2: mode = USB; break;
        case 4: mode = FM; break;
        default: mode = AM; break; // The parser accepts only 1, 2, 4 and 5.
      }
      if(setMode(mode)) return true;
      break;
    }
    default: break;
  }
  remoteKenwoodError(stream);
  return false;
}

bool remoteKenwoodFrame(Stream& stream, KenwoodState& state, ByteSpan frame)
{
  KenwoodCommand command;
  const ParseResult result = parseKenwood(frame, command);
  if(result == ParseResult::Ready) return remoteKenwoodCommand(stream, state, command);
  if(result != ParseResult::None) remoteKenwoodError(stream);
  return false;
}
