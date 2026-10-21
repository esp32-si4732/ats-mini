#include <SI4735.h>

class SI4735_fixed: public SI4735
{
  private:
    uint8_t nextStationNameSegment = 0;
    uint16_t radioTextSegments = 0;
    uint8_t radioTextVersion = 0;

    void resetRadioText(uint8_t flag = 0, uint8_t version = 0)
    {
      clearRdsBuffer2A();
      clearRdsBuffer2B();
      lastTextFlagAB = flag;
      radioTextVersion = version;
      radioTextSegments = 0;
    }

    char *completeRadioText(char *text, uint8_t address, uint8_t segmentSize)
    {
      radioTextSegments |= uint16_t(1) << address;
      for(uint8_t segment=0 ; segment<16 ; segment++)
      {
        if(!(radioTextSegments & (uint16_t(1) << segment))) return(NULL);
        for(uint8_t i=0 ; i<segmentSize ; i++)
        {
          if(text[segment * segmentSize + i]=='\r')
          {
            radioTextSegments = 0;
            return(text);
          }
        }
      }

      // A full-length message need not contain a terminator.
      radioTextSegments = 0;
      return(text);
    }

    // Assemble a replacement separately from the text already on screen.
    char *decodeRadioText(uint8_t version)
    {
      if(!getNumRdsFifoUsed() || getRdsGroupType()!=2 || getRdsVersionCode()!=version) return(NULL);

      uint8_t flag = getRdsFlagAB();
      if(lastTextFlagAB!=flag || radioTextVersion!=version)
        resetRadioText(flag, version);

      char *text = version? rds_buffer2B : rds_buffer2A;
      uint8_t segmentSize = version? 2 : 4;
      uint8_t address = currentRdsStatus.resp.BLOCKBL & 0x0F;
      if(version) getNext2Block(&text[address * segmentSize]);
      else getNext4Block(&text[address * segmentSize]);
      text[16 * segmentSize] = '\0';
      return completeRadioText(text, address, segmentSize);
    }

  public:
    void RdsInit(void)
    {
      SI4735::RdsInit();
      nextStationNameSegment = 0;
      resetRadioText();
    }

    void setRdsConfig(uint8_t RDSEN, uint8_t BLETHA, uint8_t BLETHB, uint8_t BLETHC, uint8_t BLETHD)
    {
      // The base method calls its own non-virtual RdsInit().
      SI4735::setRdsConfig(RDSEN, BLETHA, BLETHB, BLETHC, BLETHD);
      nextStationNameSegment = 0;
      resetRadioText();
    }

    // PU2CLR 2.1.8 leaves the five reserved command bits uninitialized.
    void getRdsStatus(uint8_t INTACK = 0, uint8_t MTFIFO = 0, uint8_t STATUSONLY = 0)
    {
      si47x_rds_command command = {};
      static uint16_t lastFreq;
      if(currentTune!=FM_TUNE_FREQ) return;

      if(lastFreq!=currentWorkFrequency)
      {
        lastFreq = currentWorkFrequency;
        RdsInit();
      }

      waitToSend();

      command.arg.INTACK = INTACK;
      command.arg.MTFIFO = MTFIFO;
      command.arg.STATUSONLY = STATUSONLY;

      Wire.beginTransmission(deviceAddress);
      Wire.write(FM_RDS_STATUS);
      Wire.write(command.raw);
      Wire.endTransmission();

      do
      {
        waitToSend();
        Wire.requestFrom(deviceAddress, 13);
        for(uint8_t i=0 ; i<13 ; i++)
          currentRdsStatus.raw[i] = Wire.read();
      } while(currentRdsStatus.resp.ERR);
      delayMicroseconds(550);
    }

    // Clear stale SSB sideband bits before the first FM tune
    void setFM(uint16_t fromFreq, uint16_t toFreq, uint16_t initialFreq, uint16_t step)
    {
      currentFrequencyParams.arg.USBLSB = 0;
      SI4735::setFM(fromFreq, toFreq, initialFreq, step);
    }

    // Clear stale SSB sideband bits before the first AM tune
    void setAM(uint16_t fromFreq, uint16_t toFreq, uint16_t initialFreq, uint16_t step)
    {
      currentFrequencyParams.arg.USBLSB = 0;
      SI4735::setAM(fromFreq, toFreq, initialFreq, step);
    }

    // AM patch loading without SSB-specific properties. A null content
    // pointer restores stock AM when setAM() would otherwise skip power-up.
    void loadAMPatch(const uint8_t *content, uint16_t size)
    {
      if(content)
      {
        queryLibraryId();
        setPowerUp(ctsIntEnable, 0, 1, currentClockType, AM_CURRENT_MODE, currentAudioMode);
        radioPowerUp();
        delay(50);
        downloadPatch(content, size);
        delay(25);
      }
      else
      {
        powerDown();
        setPowerUp(ctsIntEnable, 0, 0, currentClockType, AM_CURRENT_MODE, currentAudioMode);
        radioPowerUp();
      }
      setAvcAmMaxGain(currentAvcAmMaxGain);
      setVolume(volume);
      currentSsbStatus = 0;
      currentFrequencyParams.arg.USBLSB = 0;
      lastMode = AM_CURRENT_MODE;
    }

    // Fixing SI4735::getRdsPI() bug where it only returns BLOCKAL
    uint16_t getRdsPI(void)
    {
      if(getNumRdsFifoUsed())
        return (currentRdsStatus.resp.BLOCKAH << 8) + currentRdsStatus.resp.BLOCKAL;
      else
        return 0x0000;
    }

    // Fixing SI4735::getRdsProgramType() bug where it only returns three lower bits
    uint8_t getRdsProgramTypeX(void)
    {
      uint16_t blockB = (currentRdsStatus.resp.BLOCKBH << 8) + currentRdsStatus.resp.BLOCKBL;
      return (blockB >> 5) & 0x1F;
    }

    // Group 0 bit 4 is TA, not the RadioText A/B flag.
    char *getRdsText0A(void)
    {
      if(!getNumRdsFifoUsed() || getRdsGroupType()!=0) return(NULL);

      uint8_t address = currentRdsStatus.resp.BLOCKBL & 0x03;
      // Segment zero starts a new cycle; gaps invalidate the current one.
      if(address==0) nextStationNameSegment = 0;
      if(address!=nextStationNameSegment)
      {
        // Repeating the last segment unchanged does not break the sequence.
        if(address + 1==nextStationNameSegment &&
           uint8_t(rds_buffer0A[address * 2])==currentRdsStatus.resp.BLOCKDH &&
           uint8_t(rds_buffer0A[address * 2 + 1])==currentRdsStatus.resp.BLOCKDL)
          return(NULL);

        nextStationNameSegment = 0;
        return(NULL);
      }

      getNext2Block(&rds_buffer0A[address * 2]);
      rds_buffer0A[8] = '\0';
      if(++nextStationNameSegment!=4) return(NULL);

      // Keep the displayed name until a complete ordered cycle arrives.
      nextStationNameSegment = 0;
      return(rds_buffer0A);
    }

    inline char *getRdsStationName(void) { return getRdsText0A(); }

    inline char *getRdsText2A(void) { return decodeRadioText(0); }
    inline char *getRdsText2B(void) { return decodeRadioText(1); }

    // Only one kind of text is available
    inline char *getRdsProgramInformation(void)
    {
      return decodeRadioText(getRdsVersionCode());
    }

    // Only one kind of text is available
    inline char *getRdsStationInformation(void)
    {
      return decodeRadioText(getRdsVersionCode());
    }

    // Implementing the empty SI4735::setFmStereoOff() placeholder by
    // moving every blend threshold beyond reach, which pins the audio to mono
    void setFmStereoOff()
    {
      setFmBlendRssiStereoThreshold(127);
      setFmBLendRssiMonoThreshold(127);
      setFmBlendSnrStereoThreshold(127);
      setFmBLendSnrMonoThreshold(127);
      setFmBlendMultiPathStereoThreshold(0);
      setFmBlendMultiPathMonoThreshold(0);
    }

    // Implementing the empty SI4735::setFmStereoOn() placeholder by restoring
    // the blend thresholds the chip starts up with, see AN332
    void setFmStereoOn()
    {
      setFmBlendRssiStereoThreshold(49);
      setFmBLendRssiMonoThreshold(30);
      setFmBlendSnrStereoThreshold(27);
      setFmBLendSnrMonoThreshold(14);
      setFmBlendMultiPathStereoThreshold(20);
      setFmBlendMultiPathMonoThreshold(60);
    }

    // Decode UTC time directly from the RDS data blocks.
    // SI4735::getRdsDateTime() converts it to the broadcaster's local time,
    // which cannot be converted back reliably from clock-face times alone.
    bool getRdsUTCEpoch(uint32_t *epoch)
    {
      if(!epoch || getRdsGroupType() != 4 || getRdsVersionCode()) return(false);

      uint16_t blockB = (currentRdsStatus.resp.BLOCKBH << 8) | currentRdsStatus.resp.BLOCKBL;
      uint16_t blockC = (currentRdsStatus.resp.BLOCKCH << 8) | currentRdsStatus.resp.BLOCKCL;
      uint16_t blockD = (currentRdsStatus.resp.BLOCKDH << 8) | currentRdsStatus.resp.BLOCKDL;
      uint32_t mjd = ((uint32_t)(blockB & 0x0003) << 15) | (blockC >> 1);
      uint8_t hour = ((blockC & 0x0001) << 4) | (blockD >> 12);
      uint8_t minute = (blockD >> 6) & 0x003F;

      if(hour > 23 || minute > 59)
        return(false);

      const uint32_t unixEpochMJD = 40587;
      uint32_t seconds = (hour * 60 + minute) * 60;

      // Preserve the time of day when the transmitted date cannot be
      // represented by the 32-bit epoch used by NTPClient.
      if(mjd < unixEpochMJD || mjd - unixEpochMJD > (UINT32_MAX - seconds) / 86400)
        *epoch = seconds;
      else
        *epoch = (mjd - unixEpochMJD) * 86400 + seconds;

      return(true);
    }

  void seekStationProgress(void (*showFunc)(uint16_t f), bool (*stopSeeking)(), uint8_t up_down)
  {
    si47x_frequency freq;
    long elapsed_seek = millis();

    // seek command does not work for SSB
    if (lastMode == SSB_CURRENT_MODE)
      return;

    seekStation(up_down, 0);
    do
    {
      delay(maxDelaySetFrequency);
      getStatus(0, 0);
      delay(maxDelaySetFrequency);
      freq.raw.FREQH = currentStatus.resp.READFREQH;
      freq.raw.FREQL = currentStatus.resp.READFREQL;
      currentWorkFrequency = freq.value;
      if (showFunc != NULL)
        showFunc(freq.value);
      if (stopSeeking != NULL)
        if (stopSeeking())
          return;

    } while (!currentStatus.resp.VALID && !currentStatus.resp.BLTF && (millis() - elapsed_seek) < maxSeekTime);
  }

#if 0
    // Speeding up SI4735::downloadPatch() function
    bool downloadPatch(const uint8_t *ssb_patch_content, const uint16_t ssb_patch_content_size)
    {
      for(uint16_t offset=0 ; offset<ssb_patch_content_size ; offset+=8)
      {
        Wire.beginTransmission(deviceAddress);

        for(uint16_t i=0 ; i<8 ; i++)
          Wire.write(pgm_read_byte_near(ssb_patch_content + (i + offset)));

        Wire.endTransmission();
        waitToSend();
      }

      delayMicroseconds(250);
      return true;
    }

    // Using the new downloadPatch() function here
    void loadPatch(const uint8_t *ssb_patch_content, const uint16_t ssb_patch_content_size, uint8_t ssb_audiobw)
    {
      queryLibraryId();                                                                                                       patchPowerUp();
      delay(50);                                                                                                              downloadPatch(ssb_patch_content, ssb_patch_content_size);                                                               // Parameters                                                                                                           // AUDIOBW - SSB Audio bandwidth; 0 = 1.2kHz (default); 1=2.2kHz; 2=3kHz; 3=4kHz; 4=500Hz; 5=1kHz;
      // SBCUTFLT SSB - side band cutoff filter for band passand low pass filter ( 0 or 1)                                    // AVC_DIVIDER  - set 0 for SSB mode; set 3 for SYNC mode.
      // AVCEN - SSB Automatic Volume Control (AVC) enable; 0=disable; 1=enable (default).
      // SMUTESEL - SSB Soft-mute Based on RSSI or SNR (0 or 1).
      // DSP_AFCDIS - DSP AFC Disable or enable; 0=SYNC MODE, AFC enable; 1=SSB MODE, AFC disable.
      setSSBConfig(ssb_audiobw, 1, 0, 0, 0, 1);
      delay(25);
    }
#endif
};
