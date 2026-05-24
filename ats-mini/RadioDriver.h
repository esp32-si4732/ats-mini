#ifndef RADIO_DRIVER_H
#define RADIO_DRIVER_H

#include <stdint.h>
#include <stdbool.h>

void radioInit(void);
void radioTuneFM(uint16_t minFreq, uint16_t maxFreq, uint16_t freq, uint8_t step);
void radioTuneAM(uint16_t minFreq, uint16_t maxFreq, uint16_t freq, uint8_t step);
void radioTuneSSB(uint16_t minFreq, uint16_t maxFreq, uint16_t freq, uint16_t bfo, uint8_t mode);
void radioSetFrequency(uint16_t freq);
uint16_t radioGetFrequency(void);
void radioSetBfo(int16_t bfo, int16_t cal);
void radioSetVolume(uint8_t vol);
void radioSetFrequencyStep(uint8_t step);
void radioSetBandwidth(uint8_t mode, uint8_t idx);
void radioSetAgc(bool disable, uint8_t ndx);
void radioSetAvc(uint8_t gain);
void radioSetFmDeEmphasis(uint8_t value);
void radioSetSeekFmLimits(uint16_t min, uint16_t max);
void radioSetSeekAmLimits(uint16_t min, uint16_t max);
void radioSetSeekFmRssiThreshold(uint8_t thresh);
void radioSetSeekAmRssiThreshold(uint8_t thresh);
void radioSetSeekFmSnrThreshold(uint8_t thresh);
void radioSetSeekAmSnrThreshold(uint8_t thresh);
void radioSetSeekFmSpacing(uint8_t spacing);
void radioSetSeekAmSpacing(uint8_t spacing);
void radioSetMaxSeekTime(uint8_t time);
void radioSeekStation(bool up, void (*showFunc)(uint16_t), bool (*abortFunc)(void));
void radioGetSignalQuality(void);
uint8_t radioGetRssi(void);
uint8_t radioGetSnr(void);
bool radioGetPilot(void);
void radioRdsInit(void);
void radioRdsConfig(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t e);
void radioGetRdsStatus(void);
bool radioRdsReceived(void);
bool radioRdsSync(void);
bool radioRdsSyncFound(void);
const char *radioRdsStationName(void);
uint16_t radioRdsPiCode(void);
const char *radioRdsText2A(void);
const char *radioRdsText2B(void);
uint8_t radioRdsVersionCode(void);
uint8_t radioRdsProgramType(void);
const char *radioRdsTime(void);
void radioSetAudioMute(bool mute);
void radioSetGpioCtl(uint8_t a, uint8_t b, uint8_t c);
void radioSetGpio(uint8_t a, uint8_t b, uint8_t c);
void radioSetMaxDelay(uint8_t delay);
void radioLoadPatch(const uint8_t *data, uint16_t size, uint16_t bandwidth);
void radioSetup(uint8_t resetPin, uint8_t bandType);
void radioSetI2CFastMode(uint32_t speed);
int16_t radioGetI2CAddress(uint8_t resetPin);
void radioSetAudioMutePin(uint8_t pin);
void radioSetSsbAutoVolumeControl(bool on);
void radioSetSoftMuteMaxAtt(uint8_t att);
void radioStatus(uint8_t a, uint8_t b);
bool radioTuneComplete(void);

#endif
