#include "RadioDriver.h"
#include "Common.h"
// Note: SI4735-fixed.h is already pulled in via Common.h and has no include guard,
// so it must not be included here directly.

//
// Placeholder for future composite initialization.
// Currently no single rx.* method maps to a general init.
//
void radioInit(void)
{
  // Not yet implemented -- reserved for future bootstrap logic.
}

//
// Tune to an FM band
//
void radioTuneFM(uint16_t minFreq, uint16_t maxFreq, uint16_t freq, uint8_t step)
{
  rx.setFM(minFreq, maxFreq, freq, step);
  rx.setSeekFmRssiThreshold(5);
  rx.setSeekFmSNRThreshold(2);
  rx.setGpioCtl(1, 0, 0);
  rx.setGpio(0, 0, 0);
}

//
// Tune to an AM band
//
void radioTuneAM(uint16_t minFreq, uint16_t maxFreq, uint16_t freq, uint8_t step)
{
  rx.setAM(minFreq, maxFreq, freq, step);
  rx.setSeekAmRssiThreshold(10);
  rx.setSeekAmSNRThreshold(3);
}

//
// Tune to an SSB band
//
void radioTuneSSB(uint16_t minFreq, uint16_t maxFreq, uint16_t freq, uint16_t bfo, uint8_t mode)
{
  rx.setSSB(minFreq, maxFreq, freq, bfo, mode);
  rx.setSSBAutomaticVolumeControl(1);
}

// -------------------------------------------------------------------
// Direct 1:1 wrappers
// -------------------------------------------------------------------

void radioSetFrequency(uint16_t freq)           { rx.setFrequency(freq); }
uint16_t radioGetFrequency(void)                { return rx.getFrequency(); }

//
// BFO: the library expects the negated sum of BFO + calibration
//
void radioSetBfo(int16_t bfo, int16_t cal)      { rx.setSSBBfo(-(bfo + cal)); }

void radioSetVolume(uint8_t vol)                { rx.setVolume(vol); }
void radioSetFrequencyStep(uint8_t step)        { rx.setFrequencyStep(step); }

//
// Bandwidth: dispatch to the correct SI4735 API based on mode
//
void radioSetBandwidth(uint8_t mode, uint8_t idx)
{
  switch (mode)
  {
    case FM:
      rx.setFmBandwidth(idx);
      break;
    case AM:
      rx.setBandwidth(idx, 1);
      break;
    case LSB:
    case USB:
      rx.setSSBAudioBandwidth(idx);
      // When audio bandwidth is about 2 kHz or below, set sideband cutoff filter to 0
      rx.setSSBSidebandCutoffFilter((idx == 0 || idx == 4 || idx == 5) ? 0 : 1);
      break;
  }
}

void radioSetAgc(bool disable, uint8_t ndx)     { rx.setAutomaticGainControl(disable, ndx); }
void radioSetAvc(uint8_t gain)                  { rx.setAvcAmMaxGain(gain); }
void radioSetFmDeEmphasis(uint8_t value)        { rx.setFMDeEmphasis(value); }
void radioSetSeekFmLimits(uint16_t min, uint16_t max)  { rx.setSeekFmLimits(min, max); }
void radioSetSeekAmLimits(uint16_t min, uint16_t max)  { rx.setSeekAmLimits(min, max); }
void radioSetSeekFmRssiThreshold(uint8_t thresh) { rx.setSeekFmRssiThreshold(thresh); }
void radioSetSeekAmRssiThreshold(uint8_t thresh) { rx.setSeekAmRssiThreshold(thresh); }
void radioSetSeekFmSnrThreshold(uint8_t thresh)  { rx.setSeekFmSNRThreshold(thresh); }
void radioSetSeekAmSnrThreshold(uint8_t thresh)  { rx.setSeekAmSNRThreshold(thresh); }
void radioSetSeekFmSpacing(uint8_t spacing)      { rx.setSeekFmSpacing(spacing); }
void radioSetSeekAmSpacing(uint8_t spacing)      { rx.setSeekAmSpacing(spacing); }
void radioSetMaxSeekTime(uint8_t time)           { rx.setMaxSeekTime(time); }

//
// Seek: pass callbacks directly to the SI4735 library
//
void radioSeekStation(bool up, void (*showFunc)(uint16_t), bool (*abortFunc)(void))
{
  rx.seekStationProgress(showFunc, abortFunc, up ? 1 : 0);
}

void radioGetSignalQuality(void)                { rx.getCurrentReceivedSignalQuality(); }
uint8_t radioGetRssi(void)                      { return rx.getCurrentRSSI(); }
uint8_t radioGetSnr(void)                       { return rx.getCurrentSNR(); }
bool   radioGetPilot(void)                      { return rx.getCurrentPilot(); }

// -------------------------------------------------------------------
// RDS wrappers
// -------------------------------------------------------------------

void      radioRdsInit(void)                    { rx.RdsInit(); }
void      radioRdsConfig(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t e)
                                                { rx.setRdsConfig(a, b, c, d, e); }
void      radioGetRdsStatus(void)               { rx.getRdsStatus(); }
bool      radioRdsReceived(void)                { return rx.getRdsReceived(); }
bool      radioRdsSync(void)                    { return rx.getRdsSync(); }
bool      radioRdsSyncFound(void)               { return rx.getRdsSyncFound(); }
const char *radioRdsStationName(void)           { return rx.getRdsStationName(); }
uint16_t  radioRdsPiCode(void)                  { return rx.getRdsPI(); }
const char *radioRdsText2A(void)                { return rx.getRdsText2A(); }
const char *radioRdsText2B(void)                { return rx.getRdsText2B(); }
uint8_t   radioRdsVersionCode(void)             { return rx.getRdsVersionCode(); }
uint8_t   radioRdsProgramType(void)             { return rx.getRdsProgramTypeX(); }
const char *radioRdsTime(void)                  { return rx.getRdsTime(); }

// -------------------------------------------------------------------
// GPIO, audio, and hardware control wrappers
// -------------------------------------------------------------------

void radioSetAudioMute(bool mute)               { rx.setAudioMute(mute); }
void radioSetGpioCtl(uint8_t a, uint8_t b, uint8_t c) { rx.setGpioCtl(a, b, c); }
void radioSetGpio(uint8_t a, uint8_t b, uint8_t c)    { rx.setGpio(a, b, c); }

//
// Tuning delay: used by Scan and other timing-sensitive operations
//
void radioSetMaxDelay(uint8_t delay)            { rx.setMaxDelaySetFrequency(delay); }

//
// SSB patch loading
// Note: the underlying library's third parameter is uint8_t bandwidth
//
void radioLoadPatch(const uint8_t *data, uint16_t size, uint16_t bandwidth)
{
  rx.loadPatch(data, size, (uint8_t)bandwidth);
}

//
// Radio chip setup and I2C configuration
//
void    radioSetup(uint8_t resetPin, uint8_t bandType) { rx.setup(resetPin, bandType); }
void    radioSetI2CFastMode(uint32_t speed)            { rx.setI2CFastModeCustom(speed); }
int16_t radioGetI2CAddress(uint8_t resetPin)           { return rx.getDeviceI2CAddress(resetPin); }
void    radioSetAudioMutePin(uint8_t pin)              { rx.setAudioMuteMcuPin(pin); }

//
// SSB-specific configuration
//
void radioSetSsbAutoVolumeControl(bool on)      { rx.setSSBAutomaticVolumeControl(on); }

//
// Soft-mute max attenuation (AM and SSB)
//
void radioSetSoftMuteMaxAtt(uint8_t att)        { rx.setAmSoftMuteMaxAttenuation(att); }

//
// Status polling and tune-complete detection
//
void radioStatus(uint8_t a, uint8_t b)          { rx.getStatus(a, b); }
bool radioTuneComplete(void)                    { return rx.getTuneCompleteTriggered(); }
