// =================================
// INCLUDE FILES
// =================================

#include <Wire.h>
#include <TFT_eSPI.h>            // https://github.com/Xinyuan-LilyGO/T-Display-S3#quick-start
#include "EEPROM.h"
#include <SI4735.h>
#include "Rotary.h"              // Disabled half-step mode
#include "patch_init.h"          // SSB patch for whole SSBRX initialization string


// =================================
// PIN DEFINITIONS
// =================================

// SI4732/5 PINs
#define PIN_POWER_ON  15            // GPIO15   External LDO regulator enable (1 = Enable)
#define RESET_PIN     16            // GPIO16   SI4732/5 Reset
#define ESP32_I2C_SCL 17            // GPIO17   SI4732/5 Clock
#define ESP32_I2C_SDA 18            // GPIO18   SI4732/5 Data
#define AUDIO_MUTE     3            // GPIO3    Hardware L/R mute, controlled via SI4735 code (1 = Mute)
#define PIN_AMP_EN    10            // GPIO10   Hardware Audio Amplifer enable (1 = Enable)

// Display PINs
// All other pins are defined by the TFT_eSPI library
// Ref: User_Setup_Select.h
#define PIN_LCD_BL    38            // GPIO38   LCD backlight (PWM brightness control)

// Rotary Enconder PINs
#define ENCODER_PIN_A  2            // GPIO02
#define ENCODER_PIN_B  1            // GPIO01
#define ENCODER_PUSH_BUTTON 21      // GPIO21

// Battery Monitor PIN
#define VBAT_MON  4                 // GPIO04


// =================================
// COMPILE CONSTANTS
// =================================

// Compile options (0 = Disable, 1 = Enable)
// BFO Menu option
#define BFO_MENU_EN 0         // Allows BFO menu option for debug

// Serial.print control
#define DEBUG1_PRINT 0        // Highest level - Primary information
#define DEBUG2_PRINT 0        //               - Function call results
#define DEBUG3_PRINT 0        //               - Misc
#define DEBUG4_PRINT 0        // Lowest level  - EEPROM

// Remote Control
#define USE_REMOTE 1          // Allows basic serial control and monitoring

// Tune hold off enable (0 = Disable, 1 = Enable)
#define TUNE_HOLDOFF 1        // Whilst tuning holds off display update

// Display position control
// Added during development, code could be replaced with fixed values
#define menu_offset_x    0    // Menu horizontal offset
#define menu_offset_y   20    // Menu vertical offset
#define menu_delta_x    10    // Menu width delta
#define meter_offset_x   0    // Meter horizontal offset
#define meter_offset_y   0    // Meter vertical offset
#define save_offset_x   87    // EEPROM save icon horizontal offset
#define save_offset_y    0    // EEPROM save icon vertical offset
#define freq_offset_x  250    // Frequency horizontal offset
#define freq_offset_y   65    // Frequency vertical offset
#define funit_offset_x 255    // Frequency Unit horizontal offset
#define funit_offset_y  48    // Frequency Unit vertical offset
#define band_offset_x  150    // Band horizontal offset
#define band_offset_y    3    // Band vertical offset
// #define mode_offset_x   95    // Mode horizontal offset
// #define mode_offset_y  114    // Mode vertical offset
#define vol_offset_x   120    // Volume horizontal offset
#define vol_offset_y   150    // Volume vertical offset
#define rds_offset_x   165    // RDS horizontal offset
#define rds_offset_y    94    // RDS vertical offset
#define batt_offset_x  288    // Battery meter x offset
#define batt_offset_y    0    // Battery meter y offset
#define clock_datum    240    // Clock x offset

// Battery Monitoring
#define BATT_ADC_READS          10  // ADC reads for average calculation (Maximum value = 16 to avoid rollover in average calculation)
#define BATT_ADC_FACTOR      1.702  // ADC correction factor used for the battery monitor
#define BATT_SOC_LEVEL1      3.680  // Battery SOC voltage for 25%
#define BATT_SOC_LEVEL2      3.780  // Battery SOC voltage for 50%
#define BATT_SOC_LEVEL3      3.880  // Battery SOC voltage for 75%
#define BATT_SOC_HYST_2      0.020  // Battery SOC hyteresis voltage divided by 2

// SI473/5 and UI
#define MIN_ELAPSED_TIME         5  // 300
#define MIN_ELAPSED_RSSI_TIME  200  // RSSI check uses IN_ELAPSED_RSSI_TIME * 6 = 1.2s
#define ELAPSED_COMMAND      10000  // time to turn off the last command controlled by encoder. Time to goes back to the VFO control // G8PTN: Increased time and corrected comment
#define DEFAULT_VOLUME          35  // change it for your favorite sound volume
#define STRENGTH_CHECK_TIME   1500  // Not used
#define RDS_CHECK_TIME         250  // Increased from 90
#define CLICK_TIME              50
#define SHORT_PRESS_TIME       500
#define LONG_PRESS_TIME        2000

#define BACKGROUND_REFRESH_TIME 5000    // Background screen refresh time. Covers the situation where there are no other events causing a refresh
#define TUNE_HOLDOFF_TIME         90    // Timer to hold off display whilst tuning

// Band Types
#define FM_BAND_TYPE 0
#define MW_BAND_TYPE 1
#define SW_BAND_TYPE 2
#define LW_BAND_TYPE 3

// Modes
#define FM  0
#define LSB 1
#define USB 2
#define AM  3

#define MIN_CB_FREQUENCY 26060
#define MAX_CB_FREQUENCY 29665


// Menu Options
#define MENU_MODE         0
#define MENU_BAND         1
#define MENU_VOLUME       2
#define MENU_STEP         3
#define MENU_BW           4
#define MENU_MUTE         5
#define MENU_AGC_ATT      6
#define MENU_SOFTMUTE     7
#define MENU_AVC          8
#define MENU_CALIBRATION  9
#define MENU_SETTINGS    10
#if BFO_MENU_EN
#define MENU_BFO         11
#endif

// Settings Options
#define MENU_BRIGHTNESS   0
#define MENU_SLEEP        1
#define MENU_THEME        2
#define MENU_ABOUT        3

#define EEPROM_SIZE     512
#define STORE_TIME    10000                  // Time of inactivity to make the current receiver status writable (10s)

// =================================
// CONSTANTS AND VARIABLES
// =================================

// SI4732/5 patch
const uint16_t size_content = sizeof ssb_patch_content; // see patch_init.h


// EEPROM
// ====================================================================================================================================================
// Update F/W version comment as required   F/W VER    Function                                                           Locn (dec)            Bytes
// ====================================================================================================================================================
const uint8_t  app_id  = 67;          //               EEPROM ID.  If EEPROM read value mismatch, reset EEPROM            eeprom_address        1
const uint16_t app_ver = 108;         //     v1.08     EEPROM VER. If EEPROM read value mismatch (older), reset EEPROM    eeprom_ver_address    2
char app_date[] = "2025-03-25";
const int eeprom_address = 0;         //               EEPROM start address
const int eeprom_set_address = 256;   //               EEPROM setting base address
const int eeprom_setp_address = 272;  //               EEPROM setting (per band) base address
const int eeprom_ver_address = 496;   //               EEPROM version base address

long storeTime = millis();
bool itIsTimeToSave = false;

bool bfoOn = false;
bool ssbLoaded = false;
char bfo[18]="0000";
bool muted = false;
int8_t agcIdx = 0;
uint8_t disableAgc = 0;
int8_t agcNdx = 0;
int8_t softMuteMaxAttIdx = 4;

uint8_t seekDirection = 1;
bool seekStop = false;        // G8PTN: Added flag to abort seeking on rotary encoder detection
bool seekModePress = false;   // Seek happened during long press

bool cmdBand = false;
bool cmdVolume = false;
bool cmdAgc = false;
bool cmdBandwidth = false;
bool cmdStep = false;
bool cmdMode = false;
bool cmdMenu = false;
bool cmdSoftMuteMaxAtt = false;
bool cmdCal = false;
bool cmdAvc = false;
bool cmdSettings = false;
bool cmdBrt = false;
bool cmdSleep = false;
bool cmdTheme = false;
bool cmdAbout = false;

bool fmRDS = false;

int16_t currentBFO = 0;
long elapsedRSSI = millis();
long elapsedButton = millis();

long lastStrengthCheck = millis();
long lastRDSCheck = millis();

long elapsedCommand = millis();
volatile int encoderCount = 0;
uint16_t currentFrequency;

const uint16_t currentBFOStep = 10;

char sAgc[15];
char sAvc[15];

// G8PTN: Main additional variables
// BFO and Calibration limits (BFOMax + CALMax <= 16000)
const int BFOMax = 14000;               // Maximum range for currentBFO = +/- BFOMax
const int CALMax =  2000;               // Maximum range for currentCAL = +/- CALMax

// AGC/ATTN index per mode (FM/AM/SSB)
int8_t FmAgcIdx = 0;                    // Default FM  AGGON  : Range = 0 to 37, 0 = AGCON, 1 - 27 = ATTN 0 to 26
int8_t AmAgcIdx = 0;                    // Default AM  AGCON  : Range = 0 to 37, 0 = AGCON, 1 - 37 = ATTN 0 to 36
int8_t SsbAgcIdx = 0;                   // Default SSB AGCON  : Range = 0 to 1,  0 = AGCON,      1 = ATTN 0

// AVC index per mode (AM/SSB)
int8_t AmAvcIdx = 48;                   // Default AM  = 48 (as per AN332), range = 12 to 90 in steps of 2
int8_t SsbAvcIdx = 48;                  // Default SSB = 48, range = 12 to 90 in steps of 2

// SoftMute index per mode (AM/SSB)
int8_t AmSoftMuteIdx = 4;               // Default AM  = 4, range = 0 to 32
int8_t SsbSoftMuteIdx = 4;              // Default SSB = 4, range = 0 to 32

// Button checking
unsigned long pb1_time = 0;             // Push button timer
unsigned long pb1_edge_time = 0;        // Push button edge time
unsigned long pb1_pressed_time = 0;     // Push button pressed time
unsigned long pb1_short_pressed_time = 0; // Push button short pressed time
unsigned long pb1_long_pressed_time = 0;// Push button long pressed time
unsigned long pb1_released_time = 0;    // Push button released time
int pb1_current = HIGH;                 // Push button current state
int pb1_stable = HIGH;                  // Push button stable state
int pb1_last = HIGH;                    // Push button last state (after debounce)
bool pb1_pressed = false;               // Push button pressed
bool pb1_short_pressed = false;         // Push button short pressed
bool pb1_long_pressed = false;          // Push button long pressed
bool pb1_released = false;              // Push button released
bool pb1_short_released = false;        // Push button short released
bool pb1_long_released = false;         // Push button long released

bool display_on = true;                 // Display state

// Status bar icon flags
bool screen_toggle = false;             // Toggle when drawsprite is called
bool eeprom_wr_flag = false;            // Flag indicating EEPROM write request

// Firmware controlled mute
uint8_t mute_vol_val = 0;               // Volume level when mute is applied

// Menu options
int16_t currentCAL = 0;                 // Calibration offset, +/- 1000Hz in steps of 10Hz
uint16_t currentBrt = 128;              // Display brightness, range = 32 to 255 in steps of 32
int8_t currentAVC = 48;                 // Selected AVC, range = 12 to 90 in steps of 2
uint16_t currentSleep = 30;             // Display sleep timeout, range = 0 to 255 in steps of 5
long elapsedSleep = millis();           // Display sleep timer

// Background screen refresh
uint32_t background_timer = millis();   // Background screen refresh timer.
uint32_t tuning_timer = millis();       // Tuning hold off timer.
bool tuning_flag = false;               // Flag to indicate tuning

// Battery monitoring
uint16_t adc_read_total = 0;            // Total ADC count
uint16_t adc_read_avr;                  // Average ADC count = adc_read_total / BATT_ADC_READS
float adc_volt_avr;                     // Average ADC voltage with correction
uint8_t batt_soc_state = 255;           // State machine used for battery state of charge (SOC) detection with hysteresis (Default = Illegal state)

// Time
uint32_t clock_timer = 0;
uint8_t time_seconds = 0;
uint8_t time_minutes = 0;
uint8_t time_hours = 0;
char time_disp [16];
bool time_synchronized = false;  // Flag to indicate if time has been synchronized with RDS

// Remote serial
#if USE_REMOTE
uint32_t g_remote_timer = millis();
uint8_t g_remote_seqnum = 0;
bool g_remote_log = false;
#endif


// Tables

#include "themes.h"

// Menu Description
#if BFO_MENU_EN
// With BFO
const char *menu[] = {
  "Mode",
  "Band",
  "Volume",
  "Step",
  "Bandwidth",
  "Mute",
  "AGC/ATTN",
  "SoftMute",
  "AVC",
  "Calibration",
  "Settings",
  "BFO"
};
#else
// Without BFO
const char *menu[] = {
  "Mode",
  "Band",
  "Volume",
  "Step",
  "Bandwidth",
  "Mute",
  "AGC/ATTN",
  "SoftMute",
  "AVC",
  "Calibration",
  "Settings",
};
#endif

int8_t menuIdx = MENU_VOLUME;
const int lastMenu = (sizeof menu / sizeof(char *)) - 1;
int8_t currentMenuCmd = -1;

const char *settingsMenu[] = {
  "Brightness",
  "Sleep",
  "Theme",
  "About",
};

int8_t settingsMenuIdx = MENU_BRIGHTNESS;
const int lastSettingsMenu = (sizeof settingsMenu / sizeof(char *)) - 1;
int8_t currentSettingsMenuCmd = -1;

typedef struct
{
  uint8_t idx;      // SI473X device bandwidth index
  const char *desc; // bandwidth description
} Bandwidth;

int8_t bwIdxSSB = 4;
const int8_t maxSsbBw = 5;
Bandwidth bandwidthSSB[] = {
  {4, "0.5k"},
  {5, "1.0k"},
  {0, "1.2k"},
  {1, "2.2k"},
  {2, "3.0k"},
  {3, "4.0k"}
};
const int lastBandwidthSSB = (sizeof bandwidthSSB / sizeof(Bandwidth)) - 1;

int8_t bwIdxAM = 4;
const int8_t maxAmBw = 6;
Bandwidth bandwidthAM[] = {
  {4, "1.0k"},
  {5, "1.8k"},
  {3, "2.0k"},
  {6, "2.5k"},
  {2, "3.0k"},
  {1, "4.0k"},
  {0, "6.0k"}
};
const int lastBandwidthAM = (sizeof bandwidthAM / sizeof(Bandwidth)) - 1;

int8_t bwIdxFM = 0;
const int8_t maxFmBw = 4;
Bandwidth bandwidthFM[] = {
    {0, "Auto"}, // Automatic - default
    {1, "110k"}, // Force wide (110 kHz) channel filter.
    {2, "84k"},
    {3, "60k"},
    {4, "40k"}};
const int lastBandwidthFM = (sizeof bandwidthFM / sizeof(Bandwidth)) - 1;

int tabAmStep[] = {
  1,      // 0   AM/SSB   (kHz)
  5,      // 1   AM/SSB   (kHz)
  9,      // 2   AM/SSB   (kHz)
  10,     // 3   AM/SSB   (kHz)
  50,     // 4   AM       (kHz)
  100,    // 5   AM       (kHz)
  1000,   // 6   AM       (kHz)
  10,     // 7   SSB      (Hz)
  25,     // 8   SSB      (Hz)
  50,     // 9   SSB      (Hz)
  100,    // 10  SSB      (Hz)
  500     // 11  SSB      (Hz)
};

int tabSsbFastStep[] = {
  1,   // 0->1 (1kHz -> 5kHz)
  3,   // 1->3 (5kHz -> 10kHz)
  2,   // 2->2 (9kHz -> 9kHz)
  3,   // 3->3 (10kHz -> 10kHz)
  4,   // 4->4 (50kHz -> 50kHz) n/a
  5,   // 5->5 (100kHz -> 100kHz) n/a
  6,   // 6->6 (1MHz -> 1MHz) n/a
  10,  // 7->10 (10Hz -> 100Hz)
  10,  // 8->10 (25Hz -> 100Hz)
  11,  // 9->11 (50Hz -> 500Hz)
  0,   // 10->0 (100Hz -> 1kHz)
  1,   // 11->1 (500Hz -> 5kHz)
};

uint8_t AmTotalSteps = 7;                          // Total AM steps
uint8_t AmTotalStepsSsb = 4;                       // G8PTN: Original : AM(LW/MW) 1k, 5k, 9k, 10k, 50k        : SSB 1k, 5k, 9k, 10k
//uint8_t AmTotalStepsSsb = 5;                     // G8PTN: Option 1 : AM(LW/MW) 1k, 5k, 9k, 10k, 100k       : SSB 1k, 5k, 9k, 10k, 50k
//uint8_t AmTotalStepsSsb = 6;                     // G8PTN: Option 2 : AM(LW/MW) 1k, 5k, 9k, 10k, 100k , 1M  : SSB 1k, 5k, 9k, 10k, 50k, 100k
//uint8_t AmTotalStepsSsb = 7;                     // G8PTN: Invalid option (Do not use)
uint8_t SsbTotalSteps = 5;                         // SSB sub 1kHz steps
volatile int8_t idxAmStep = 3;

const char *AmSsbStepDesc[] = {"1k", "5k", "9k", "10k", "50k", "100k", "1M", "10Hz", "25Hz", "50Hz", "0.1k", "0.5k"};

int tabFmStep[] = {5, 10, 20, 100};                             // G8PTN: Added 1MHz step
const int lastFmStep = (sizeof tabFmStep / sizeof(int)) - 1;
int idxFmStep = 1;

const char *FmStepDesc[] = {"50k", "100k", "200k", "1M"};

uint16_t currentStepIdx = 1;

const char *bandModeDesc[] = {"FM", "LSB", "USB", "AM"};
const int lastBandModeDesc = (sizeof bandModeDesc / sizeof(char *)) - 1;
uint8_t currentMode = FM;

/**
 *  Band data structure
 */
typedef struct
{
  const char *bandName;   // Band description
  uint8_t bandType;       // Band type (FM, MW or SW)
  uint16_t minimumFreq;   // Minimum frequency of the band
  uint16_t maximumFreq;   // maximum frequency of the band
  uint16_t currentFreq;   // Default frequency or current frequency
  int8_t currentStepIdx;  // Idex of tabStepAM:  Defeult frequency step (See tabStepAM)
  int8_t bandwidthIdx;    // Index of the table bandwidthFM, bandwidthAM or bandwidthSSB;
} Band;

/*
   Band table
   YOU CAN CONFIGURE YOUR OWN BAND PLAN. Be guided by the comments.
   To add a new band, all you have to do is insert a new line in the table below and adjust the bandCAL and bandMODE size.
   No extra code will be needed. You can remove a band by deleting a line if you do not want a given band.
   Also, you can change the parameters of the band.
   ATTENTION: You have to RESET the eeprom after adding or removing a line of this table.
              Turn your receiver on with the encoder push button pressed at first time to RESET the eeprom content.
*/

#define BAND_VHF 0
#define BAND_MW1 1
#define BAND_MW2 2
#define BAND_MW3 3
#define BAND_80M 4
#define BAND_SW1 5
#define BAND_SW2 6
#define BAND_40M 7
#define BAND_SW3 8
#define BAND_SW4 9
#define BAND_SW5 10
#define BAND_SW6 11
#define BAND_20M 12
#define BAND_SW7 13
#define BAND_SW8 14
#define BAND_15M 15
#define BAND_SW9 16
#define BAND_CB  17
#define BAND_10M 18
#define BAND_ALL 19

Band band[] = {
    {"VHF", FM_BAND_TYPE, 6400, 10800, 10390, 1, 0},
    {"MW1", MW_BAND_TYPE, 150, 1720, 810, 3, 4},
    {"MW2", MW_BAND_TYPE, 531, 1701, 783, 2, 4},
    {"MW3", MW_BAND_TYPE, 1700, 3500, 2500, 1, 4},
    {"80M", MW_BAND_TYPE, 3500, 4000, 3700, 0, 4},
    {"SW1", SW_BAND_TYPE, 4000, 5500, 4885, 1, 4},
    {"SW2", SW_BAND_TYPE, 5500, 6500, 6000, 1, 4},
    {"40M", SW_BAND_TYPE, 6500, 7300, 7100, 0, 4},
    {"SW3", SW_BAND_TYPE, 7200, 8000, 7200, 1, 4},
    {"SW4", SW_BAND_TYPE, 9000, 11000, 9500, 1, 4},
    {"SW5", SW_BAND_TYPE, 11100, 13000, 11900, 1, 4},
    {"SW6", SW_BAND_TYPE, 13000, 14000, 13500, 1, 4},
    {"20M", SW_BAND_TYPE, 14000, 15000, 14200, 0, 4},
    {"SW7", SW_BAND_TYPE, 15000, 17000, 15300, 1, 4},
    {"SW8", SW_BAND_TYPE, 17000, 18000, 17500, 1, 4},
    {"15M", SW_BAND_TYPE, 20000, 21400, 21100, 0, 4},
    {"SW9", SW_BAND_TYPE, 21400, 22800, 21500, 1, 4},
    {"CB ", SW_BAND_TYPE, 26000, 30000, 27135, 0, 4},
    {"10M", SW_BAND_TYPE, 28000, 30000, 28400, 0, 4},
    {"ALL", SW_BAND_TYPE, 150, 30000, 15000, 0, 4} // All band. LW, MW and SW (from 150kHz to 30MHz)
};

const int lastBand = (sizeof band / sizeof(Band)) - 1;
int bandIdx = 0;

//int tabStep[] = {1, 5, 10, 50, 100, 500, 1000};
//const int lastStep = (sizeof tabStep / sizeof(int)) - 1;

// Calibration (per band). Size needs to be the same as band[]
// Defaults
int16_t bandCAL[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

// Mode (per band). Size needs to be the same as band[] and mode needs to be appropriate for bandType
// Example bandType = FM_BAND_TYPE, bandMODE = FM. All other BAND_TYPE's, bandMODE = AM/LSB/USB
// Defaults
uint8_t bandMODE[] = {FM, AM, AM, AM, LSB, AM, AM, LSB, AM, AM, AM, AM, USB, AM, AM, USB, AM, AM, USB, AM};

const char *cbChannelNumber[] = {
    "1", "2", "3", "41",
    "4", "5", "6", "7", "42",
    "8", "9", "10", "11", "43",
    "12", "13", "14", "15", "44",
    "16", "17", "18", "19", "45",
    "20", "21", "22", "23",
    "24", "25", "26", "27",
    "28", "29", "30", "31",
    "32", "33", "34", "35",
    "36", "37", "38", "39",
    "40",
};

char *rdsMsg;
char *stationName;
char *rdsTime;
char bufferStationName[50];
char bufferRdsMsg[100];
char bufferRdsTime[32];

uint8_t rssi = 0;
uint8_t snr = 0;
uint8_t volume = DEFAULT_VOLUME;


// SSB Mode detection
bool isSSB() {
  return currentMode > FM && currentMode < AM;    // This allows for adding CW mode as well as LSB/USB if required
}

bool isCB() {
  return bandIdx == BAND_CB;
}


// Generation of step value
int getSteps(bool fast = false) {
  if (isSSB()) {
    int8_t idxAmStepEff = fast ? tabSsbFastStep[idxAmStep] : idxAmStep;
    if (idxAmStepEff >= AmTotalSteps)
      return tabAmStep[idxAmStepEff];             // SSB: Return in Hz used for VFO + BFO tuning

    return tabAmStep[idxAmStepEff] * 1000;        // SSB: Return in Hz used for VFO + BFO tuning
  }

  if (idxAmStep >= AmTotalSteps)                  // AM: Set to 0kHz if step is from the SSB Hz values
    idxAmStep = 0;

  return tabAmStep[idxAmStep];                    // AM: Return value in KHz for SI4732 step
}


// Generate last step index
int getLastStep()
{
  // Debug
  #if DEBUG2_PRINT
  Serial.print("Info: getLastStep() >>> AmTotalSteps = ");
  Serial.print(AmTotalSteps);
  Serial.print(", SsbTotalSteps = ");
  Serial.print(SsbTotalSteps);
  Serial.print(", isSSB = ");
  Serial.println(isSSB());
  #endif

  if (isSSB())
    return AmTotalSteps + SsbTotalSteps - 1;
  else if (band[bandIdx].bandType == LW_BAND_TYPE || band[bandIdx].bandType == MW_BAND_TYPE)    // G8PTN; Added in place of check in doStep() for LW/MW step limit
    return AmTotalStepsSsb;
  else
    return AmTotalSteps - 1;
}


// Devices class declarations
Rotary encoder = Rotary(ENCODER_PIN_B, ENCODER_PIN_A);      // G8PTN: Corrected mapping based on rotary library


TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);

SI4735 rx;

char fw_ver [25];

void get_fw_ver() {
    uint16_t ver_major = (app_ver / 100);
    uint16_t ver_minor = (app_ver % 100);
    sprintf(fw_ver, "F/W: v%1.1d.%2.2d %s", ver_major, ver_minor, app_date);
}

void setup()
{
  // Enable Serial. G8PTN: Added
  Serial.begin(115200);

  // Audio Amplifier Enable. G8PTN: Added
  // Initally disable the audio amplifier until the SI4732 has been setup
  pinMode(PIN_AMP_EN, OUTPUT);
  digitalWrite(PIN_AMP_EN, LOW);

  // SI4732 VDD Enable
  pinMode(PIN_POWER_ON, OUTPUT);
  digitalWrite(PIN_POWER_ON, HIGH);

  // Encoder pins. Enable internal pull-ups
  pinMode(ENCODER_PUSH_BUTTON, INPUT_PULLUP);
  pinMode(ENCODER_PIN_A, INPUT_PULLUP);
  pinMode(ENCODER_PIN_B, INPUT_PULLUP);

  // The line below may be necessary to setup I2C pins on ESP32
  Wire.begin(ESP32_I2C_SDA, ESP32_I2C_SCL);

  // TFT display setup
  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(theme[themeIdx].bg);
  spr.createSprite(320,170);
  spr.setTextDatum(MC_DATUM);
  spr.setSwapBytes(true);
  spr.setFreeFont(&Orbitron_Light_24);
  spr.setTextColor(theme[themeIdx].text, theme[themeIdx].bg);

  // TFT display brightness control (PWM)
  // Note: At brightness levels below 100%, switching from the PWM may cause power spikes and/or RFI
  ledcAttach(PIN_LCD_BL, 16000, 8);  // Pin assignment, 16kHz, 8-bit
  ledcWrite(PIN_LCD_BL, 255);        // Default value 255 = 100%)

  // EEPROM
  // Note: Use EEPROM.begin(EEPROM_SIZE) before use and EEPROM.begin.end after use to free up memory and avoid memory leaks
  EEPROM.begin(EEPROM_SIZE);

  // Press and hold Encoder button to force an EEPROM reset
  // Indirectly forces the reset by setting app_id = 0 (Detected in the subsequent check for app_id and app_ver)
  // Note: EEPROM reset is recommended after firmware updates
  if (digitalRead(ENCODER_PUSH_BUTTON) == LOW) {

    tft.setTextSize(2);
    tft.setTextColor(theme[themeIdx].text, theme[themeIdx].bg);
    get_fw_ver();
    tft.println(fw_ver);
    tft.println();
    EEPROM.write(eeprom_address, 0);
    EEPROM.commit();
    tft.setTextColor(theme[themeIdx].text_warn, theme[themeIdx].bg);
    tft.print("EEPROM Resetting");
    delay(2000);
  }

  EEPROM.end();

  // G8PTN: Moved this to later, to avoid interrupt action
  /*
  // ICACHE_RAM_ATTR void rotaryEncoder(); see rotaryEncoder implementation below.
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), rotaryEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_B), rotaryEncoder, CHANGE);
  */

  // Check for SI4732 connected on I2C interface
  // If the SI4732 is not detected, then halt with no further processing
  rx.setI2CFastModeCustom(100000);

  int16_t si4735Addr = rx.getDeviceI2CAddress(RESET_PIN); // Looks for the I2C bus address and set it.  Returns 0 if error

  if ( si4735Addr == 0 ) {
    tft.setTextSize(2);
    tft.setTextColor(theme[themeIdx].text_warn, theme[themeIdx].bg);
    tft.println("Si4735 not detected");
    while (1);
  }

  rx.setup(RESET_PIN, MW_BAND_TYPE);
  // Comment the line above and uncomment the three lines below if you are using external ref clock (active crystal or signal generator)
  // rx.setRefClock(32768);
  // rx.setRefClockPrescaler(1);   // will work with 32768
  // rx.setup(RESET_PIN, 0, MW_BAND_TYPE, SI473X_ANALOG_AUDIO, XOSCEN_RCLK);

  // Attached pin to allows SI4732 library to mute audio as required to minimise loud clicks
  rx.setAudioMuteMcuPin(AUDIO_MUTE);

  cleanBfoRdsInfo();

  delay(300);

  // Audio Amplifier Enable. G8PTN: Added
  // After the SI4732 has been setup, enable the audio amplifier
  digitalWrite(PIN_AMP_EN, HIGH);


  // Checking the EEPROM content
  // Checks app_id (which covers manual reset) and app_ver which allows for automatic reset
  // The app_ver is equivalent to a F/W version.

  // Debug
  // Read all EEPROM locations
  #if DEBUG4_PRINT
  EEPROM.begin(EEPROM_SIZE);
  Serial.println("**** EEPROM READ: Pre Check");
  for (int i = 0; i <= (EEPROM_SIZE - 1); i++){
    Serial.print(EEPROM.read(i));
    delay(10);
    Serial.print("\t");
    if (i%16 == 15) Serial.println();
  }
  Serial.println("****");
  EEPROM.end();
  #endif

  // Perform check against app_id and app_ver
  uint8_t  id_read;
  uint16_t ver_read;

  EEPROM.begin(EEPROM_SIZE);
  id_read = EEPROM.read(eeprom_address);
  ver_read  = EEPROM.read(eeprom_ver_address) << 8;
  ver_read |= EEPROM.read(eeprom_ver_address + 1);
  EEPROM.end();

  if (id_read == app_id) {
    readAllReceiverInformation();                        // Load EEPROM values
  }
  else {
    saveAllReceiverInformation();                        // Set EEPROM to defaults
    rx.setVolume(volume);                                // Set initial volume after EEPROM reset
    ledcWrite(PIN_LCD_BL, currentBrt);                   // Set initial brightness after EEPROM reset
  }

  sprintf(time_disp, "%02d:%02dZ", time_hours, time_minutes);

  // Debug
  // Read all EEPROM locations
  #if DEBUG4_PRINT
  EEPROM.begin(EEPROM_SIZE);
  Serial.println("**** START READ: Post check actions");
  for (int i = 0; i <= (EEPROM_SIZE - 1); i++){
    Serial.print(EEPROM.read(i));
    delay(10);
    Serial.print("\t");
    if (i%16 == 15) Serial.println();
  }
  Serial.println("****");
  EEPROM.end();
  #endif

  // ** SI4732 STARTUP **
  // Uses values from EEPROM (Last stored or defaults after EEPROM reset)
  useBand();

  showStatus();

  // Interrupt actions for Rotary encoder
  // Note: Moved to end of setup to avoid inital interrupt actions
  // ICACHE_RAM_ATTR void rotaryEncoder(); see rotaryEncoder implementation below.
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), rotaryEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_B), rotaryEncoder, CHANGE);
}


/**
 * Prints a given content on display
 */
void print(uint8_t col, uint8_t lin, const GFXfont *font, uint8_t textSize, const char *msg) {
  tft.setCursor(col,lin);
  tft.setTextSize(textSize);
  tft.setTextColor(theme[themeIdx].text_warn, theme[themeIdx].bg);
  tft.println(msg);
}

void printParam(const char *msg) {
  tft.fillScreen(theme[themeIdx].bg);
  print(0,10,NULL,2, msg);
}

/*
   writes the conrrent receiver information into the eeprom.
   The EEPROM.update avoid write the same data in the same memory position. It will save unnecessary recording.
*/
void saveAllReceiverInformation()
{
  eeprom_wr_flag = true;
  int addr_offset;
  int16_t currentBFOs = (currentBFO % 1000);            // G8PTN: For SSB ensures BFO value is valid wrt band[bandIdx].currentFreq = currentFrequency;

  EEPROM.begin(EEPROM_SIZE);

  EEPROM.write(eeprom_address, app_id);                 // Stores the app id;
  EEPROM.write(eeprom_address + 1, rx.getVolume());     // Stores the current Volume
  EEPROM.write(eeprom_address + 2, bandIdx);            // Stores the current band
  EEPROM.write(eeprom_address + 3, fmRDS);              // G8PTN: Not used
  EEPROM.write(eeprom_address + 4, currentMode);        // Stores the current Mode (FM / AM / LSB / USB). Now per mode, leave for compatibility
  EEPROM.write(eeprom_address + 5, currentBFOs >> 8);   // G8PTN: Stores the current BFO % 1000 (HIGH byte)
  EEPROM.write(eeprom_address + 6, currentBFOs & 0XFF); // G8PTN: Stores the current BFO % 1000 (LOW byte)
  EEPROM.commit();

  addr_offset = 7;

  // G8PTN: Commented out the assignment
  // - The line appears to be required to ensure the band[bandIdx].currentFreq = currentFrequency
  // - Updated main code to ensure that this should occur as required with frequency, band or mode changes
  // - The EEPROM reset code now calls saveAllReceiverInformation(), which is the correct action, this line
  //   must be disabled otherwise band[bandIdx].currentFreq = 0 (where bandIdx = 0; by default) on EEPROM reset
  //band[bandIdx].currentFreq = currentFrequency;

  for (int i = 0; i <= lastBand; i++)
  {
    EEPROM.write(addr_offset++, (band[i].currentFreq >> 8));   // Stores the current Frequency HIGH byte for the band
    EEPROM.write(addr_offset++, (band[i].currentFreq & 0xFF)); // Stores the current Frequency LOW byte for the band
    EEPROM.write(addr_offset++, band[i].currentStepIdx);       // Stores current step of the band
    EEPROM.write(addr_offset++, band[i].bandwidthIdx);         // table index (direct position) of bandwidth
    EEPROM.commit();
  }

  // G8PTN: Added
  addr_offset = eeprom_set_address;
  EEPROM.write(addr_offset++, currentBrt >> 8);         // Stores the current Brightness value (HIGH byte)
  EEPROM.write(addr_offset++, currentBrt & 0XFF);       // Stores the current Brightness value (LOW byte)
  EEPROM.write(addr_offset++, FmAgcIdx);                // Stores the current FM AGC/ATTN index value
  EEPROM.write(addr_offset++, AmAgcIdx);                // Stores the current AM AGC/ATTN index value
  EEPROM.write(addr_offset++, SsbAgcIdx);               // Stores the current SSB AGC/ATTN index value
  EEPROM.write(addr_offset++, AmAvcIdx);                // Stores the current AM AVC index value
  EEPROM.write(addr_offset++, SsbAvcIdx);               // Stores the current SSB AVC index value
  EEPROM.write(addr_offset++, AmSoftMuteIdx);           // Stores the current AM SoftMute index value
  EEPROM.write(addr_offset++, SsbSoftMuteIdx);          // Stores the current SSB SoftMute index value
  EEPROM.write(addr_offset++, currentSleep >> 8);       // Stores the current Sleep value (HIGH byte)
  EEPROM.write(addr_offset++, currentSleep & 0XFF);     // Stores the current Sleep value (LOW byte)
  EEPROM.write(addr_offset++, themeIdx);                // Stores the current Theme index value
  EEPROM.commit();

  addr_offset = eeprom_setp_address;
  for (int i = 0; i <= lastBand; i++)
  {
    EEPROM.write(addr_offset++, (bandCAL[i] >> 8));     // Stores the current Calibration value (HIGH byte) for the band
    EEPROM.write(addr_offset++, (bandCAL[i] & 0XFF));   // Stores the current Calibration value (LOW byte) for the band
    EEPROM.write(addr_offset++,  bandMODE[i]);          // Stores the current Mode value for the band
    EEPROM.commit();
  }

  addr_offset = eeprom_ver_address;
  EEPROM.write(addr_offset++, app_ver >> 8);            // Stores app_ver (HIGH byte)
  EEPROM.write(addr_offset++, app_ver & 0XFF);          // Stores app_ver (LOW byte)
  EEPROM.commit();

  EEPROM.end();
}

/**
 * reads the last receiver status from eeprom.
 */
void readAllReceiverInformation()
{
  uint8_t volume;
  int addr_offset;
  int bwIdx;
  EEPROM.begin(EEPROM_SIZE);

  volume = EEPROM.read(eeprom_address + 1); // Gets the stored volume;
  bandIdx = EEPROM.read(eeprom_address + 2);
  fmRDS = EEPROM.read(eeprom_address + 3);                // G8PTN: Not used
  currentMode = EEPROM.read(eeprom_address + 4);          // G8PTM: Reads stored Mode. Now per mode, leave for compatibility
  currentBFO = EEPROM.read(eeprom_address + 5) << 8;      // G8PTN: Reads stored BFO value (HIGH byte)
  currentBFO |= EEPROM.read(eeprom_address + 6);          // G8PTN: Reads stored BFO value (HIGH byte)

  addr_offset = 7;
  for (int i = 0; i <= lastBand; i++)
  {
    band[i].currentFreq = EEPROM.read(addr_offset++) << 8;
    band[i].currentFreq |= EEPROM.read(addr_offset++);
    band[i].currentStepIdx = EEPROM.read(addr_offset++);
    band[i].bandwidthIdx = EEPROM.read(addr_offset++);
  }

  // G8PTN: Added
  addr_offset = eeprom_set_address;
  currentBrt      = EEPROM.read(addr_offset++) << 8;      // Reads stored Brightness value (HIGH byte)
  currentBrt     |= EEPROM.read(addr_offset++);           // Reads stored Brightness value (LOW byte)
  FmAgcIdx        = EEPROM.read(addr_offset++);           // Reads stored FM AGC/ATTN index value
  AmAgcIdx        = EEPROM.read(addr_offset++);           // Reads stored AM AGC/ATTN index value
  SsbAgcIdx       = EEPROM.read(addr_offset++);           // Reads stored SSB AGC/ATTN index value
  AmAvcIdx        = EEPROM.read(addr_offset++);           // Reads stored AM AVC index value
  SsbAvcIdx       = EEPROM.read(addr_offset++);           // Reads stored SSB AVC index value
  AmSoftMuteIdx   = EEPROM.read(addr_offset++);           // Reads stored AM SoftMute index value
  SsbSoftMuteIdx  = EEPROM.read(addr_offset++);           // Reads stored SSB SoftMute index value
  currentSleep    = EEPROM.read(addr_offset++) << 8;      // Reads stored Sleep value (HIGH byte)
  currentSleep   |= EEPROM.read(addr_offset++);           // Reads stored Sleep value (LOW byte)
  themeIdx        = EEPROM.read(addr_offset++);           // Reads stored Theme index value

  addr_offset = eeprom_setp_address;
  for (int i = 0; i <= lastBand; i++)
  {
    bandCAL[i]    = EEPROM.read(addr_offset++) << 8;      // Reads stored Calibration value (HIGH byte) per band
    bandCAL[i]   |= EEPROM.read(addr_offset++);           // Reads stored Calibration value (LOW byte) per band
    bandMODE[i]   = EEPROM.read(addr_offset++);           // Reads stored Mode value per band
  }

  EEPROM.end();

  // G8PTN: Added
  ledcWrite(PIN_LCD_BL, currentBrt);

  currentFrequency = band[bandIdx].currentFreq;
  currentMode = bandMODE[bandIdx];                       // G8PTN: Added to support mode per band

  if (band[bandIdx].bandType == FM_BAND_TYPE)
  {
    currentStepIdx = idxFmStep = band[bandIdx].currentStepIdx;
    rx.setFrequencyStep(tabFmStep[currentStepIdx]);
  }
  else
  {
    currentStepIdx = idxAmStep = band[bandIdx].currentStepIdx;
    rx.setFrequencyStep(tabAmStep[currentStepIdx]);
  }

  bwIdx = band[bandIdx].bandwidthIdx;

  if (isSSB())
  {
    loadSSB();
    bwIdxSSB = (bwIdx > 5) ? 5 : bwIdx;
    rx.setSSBAudioBandwidth(bandwidthSSB[bwIdxSSB].idx);
    correctCutoffFilter();
    updateBFO();
  }
  else if (currentMode == AM)
  {
    bwIdxAM = bwIdx;
    rx.setBandwidth(bandwidthAM[bwIdxAM].idx, 1);
  }
  else
  {
    bwIdxFM = bwIdx;
    rx.setFmBandwidth(bandwidthFM[bwIdxFM].idx);
  }

  if (currentBFO > 0)
    sprintf(bfo, "+%4.4d", currentBFO);
  else
    sprintf(bfo, "%4.4d", currentBFO);

  delay(50);
  rx.setVolume(volume);
}

/*
 * To store any change into the EEPROM, it is needed at least STORE_TIME  milliseconds of inactivity.
 */
void resetEepromDelay()
{
  storeTime = millis();
  itIsTimeToSave = true;
}

/**
    Set all command flags to false
    When all flags are disabled (false), the encoder controls the frequency
*/
void disableCommands()
{
  cmdBand = false;
  bfoOn = false;
  cmdVolume = false;
  cmdAgc = false;
  cmdBandwidth = false;
  cmdStep = false;
  cmdMode = false;
  cmdMenu = false;
  cmdSoftMuteMaxAtt = false;
  cmdCal = false;
  cmdAvc = false;
  cmdSettings = false;
  cmdBrt = false;
  cmdSleep = false;
  cmdTheme = false;
  cmdAbout = false;
}


bool isModalMode() {
  return (
          isMenuMode() |
          isSettingsMode() |
          cmdAbout
          );
}

bool isMenuMode() {
  return (
          cmdMenu |
          cmdStep |
          cmdBandwidth |
          cmdAgc |
          cmdVolume |
          cmdSoftMuteMaxAtt |
          cmdMode |
          cmdBand |
          cmdCal |
          cmdAvc
          );
}

bool isSettingsMode() {
  return (
          cmdSettings |
          cmdBrt |
          cmdSleep |
          cmdTheme
          );
}

/**
 * Reads encoder via interrupt
 * Use Rotary.h and  Rotary.cpp implementation to process encoder via interrupt
 * if you do not add ICACHE_RAM_ATTR declaration, the system will reboot during attachInterrupt call.
 * With ICACHE_RAM_ATTR macro you put the function on the RAM.
 */
ICACHE_RAM_ATTR void rotaryEncoder()
{ // rotary encoder events
  uint8_t encoderStatus = encoder.process();
  if (encoderStatus) {
    encoderCount = (encoderStatus == DIR_CW) ? 1 : -1;
    seekStop = true;  // G8PTN: Added flag
  }
}

/**
 * Shows frequency information on Display
 */
void showFrequency() {
  drawSprite();
}

/**
 * Shows the current mode
 */
void showMode() {
  drawSprite();
}

/**
 * Shows some basic information on display
 */
void showStatus() {
  drawSprite();
}

/**
 *  Shows the current Bandwidth status
 */
void showBandwidth()
{
  drawSprite();
}

/**
 *   Shows the current RSSI and SNR status
 */
void showRSSI()
{
  drawSprite();
}

/**
 *    Shows the current AGC and Attenuation status
 */
void showAgcAtt()
{
  // lcd.clear();
  //rx.getAutomaticGainControl();             // G8PTN: Read back value is not used
  if (agcNdx == 0 && agcIdx == 0)
    strcpy(sAgc, "AGC ON");
  else
    sprintf(sAgc, "ATT: %2.2d", agcNdx);

  drawSprite();
}

/**
 *   Shows the current step
 */
void showStep()
{
  drawSprite();
}

/**
 *  Shows the current BFO value
 */
void showBFO()
{
  if (currentBFO > 0)
    sprintf(bfo, "+%4.4d", currentBFO);
  else
    sprintf(bfo, "%4.4d", currentBFO);
  drawSprite();
}

/*
 *  Shows the volume level on LCD
 */
void showVolume()
{
drawSprite();
}

/**
 * Show Soft Mute
 */
void showSoftMute()
{
  drawSprite();
}

/**
 * Show message "Loading SSB"
 */

void showLoadingSSB()
{
  if (display_on) {
    spr.setTextDatum(MC_DATUM);
    spr.fillSmoothRoundRect(80,40,160,40,4,theme[themeIdx].text);
    spr.fillSmoothRoundRect(81,41,158,38,4,theme[themeIdx].menu_bg);
    spr.drawString("Loading SSB",160,62,4);
    spr.pushSprite(0,0);
  }
}

/**
 *   Sets Band up (1) or down (!1)
 */
void setBand(int8_t up_down)
{
  // G8PTN: Reset BFO when changing band and store frequency
  band[bandIdx].currentFreq = currentFrequency + (currentBFO / 1000);
  currentBFO = 0;

  band[bandIdx].currentStepIdx = currentStepIdx;
  if (up_down == 1)                                            // G8PTN: Corrected direction
    bandIdx = (bandIdx < lastBand) ? (bandIdx + 1) : 0;
  else
    bandIdx = (bandIdx > 0) ? (bandIdx - 1) : lastBand;

  // G8PTN: Added to support mode per band
  currentMode = bandMODE[bandIdx];
  if (isSSB())
  {
    if (ssbLoaded == false)
    {
      // Only loadSSB if not already loaded
      showLoadingSSB();
      loadSSB();
      ssbLoaded = true;
    }
  }
  else {
    // If not SSB
    ssbLoaded = false;
  }

  useBand();
}

/**
 * Switch the radio to current band
 */
void useBand() {
  currentMode = bandMODE[bandIdx];                  // G8PTN: Added to support mode per band
  if (band[bandIdx].bandType == FM_BAND_TYPE) {
    currentMode = FM;
    // rx.setTuneFrequencyAntennaCapacitor(0);
    rx.setFM(band[bandIdx].minimumFreq, band[bandIdx].maximumFreq, band[bandIdx].currentFreq, tabFmStep[band[bandIdx].currentStepIdx]);
    rx.setSeekFmLimits(band[bandIdx].minimumFreq, band[bandIdx].maximumFreq);
    bfoOn = ssbLoaded = false;
    bwIdxFM = band[bandIdx].bandwidthIdx;
    rx.setFmBandwidth(bandwidthFM[bwIdxFM].idx);
    rx.setFMDeEmphasis(1);
    rx.RdsInit();
    rx.setRdsConfig(1, 2, 2, 2, 2);
    rx.setGpioCtl(1,0,0);   // G8PTN: Enable GPIO1 as output
    rx.setGpio(0,0,0);      // G8PTN: Set GPIO1 = 0
  } else {
    // set the tuning capacitor for SW or MW/LW
    // rx.setTuneFrequencyAntennaCapacitor((band[bandIdx].bandType == MW_BAND_TYPE || band[bandIdx].bandType == LW_BAND_TYPE) ? 0 : 1);
    if (ssbLoaded) {
      // Configure SI4732 for SSB
      rx.setSSB(
        band[bandIdx].minimumFreq,
        band[bandIdx].maximumFreq,
        band[bandIdx].currentFreq,
        0,                                                  // SI4732 step is not used for SSB!
        currentMode);

      rx.setSSBAutomaticVolumeControl(1);                   // G8PTN: Always enabled
      //rx.setSsbSoftMuteMaxAttenuation(softMuteMaxAttIdx); // G8PTN: Commented out
      if   (band[bandIdx].bandwidthIdx > 5) bwIdxSSB = 5;   // G8PTN: Limit value
      else bwIdxSSB = band[bandIdx].bandwidthIdx;
      rx.setSSBAudioBandwidth(bandwidthSSB[bwIdxSSB].idx);
      updateBFO();                                          // G8PTN: If SSB is loaded update BFO
    } else {
      currentMode = AM;
      rx.setAM(
        band[bandIdx].minimumFreq,
        band[bandIdx].maximumFreq,
        band[bandIdx].currentFreq,
        band[bandIdx].currentStepIdx >= AmTotalSteps ? 1 : tabAmStep[band[bandIdx].currentStepIdx]);   // Set to 1kHz

      bfoOn = false;
      bwIdxAM = band[bandIdx].bandwidthIdx;
      rx.setBandwidth(bandwidthAM[bwIdxAM].idx, 1);
      //rx.setAmSoftMuteMaxAttenuation(softMuteMaxAttIdx); //Soft Mute for AM or SSB
    }
    rx.setGpioCtl(1,0,0);   // G8PTN: Enable GPIO1 as output
    rx.setGpio(1,0,0);      // G8PTN: Set GPIO1 = 1
    rx.setSeekAmLimits(band[bandIdx].minimumFreq, band[bandIdx].maximumFreq); // Consider the range all defined current band
    rx.setSeekAmSpacing(5); // Max 10kHz for spacing
  }

  // G8PTN: Added
  // Call doSoftMute(0), 0 = No incr/decr action (eqivalent to getSoftMute)
  // This gets softMuteMaxAttIdx based on mode (AM, SSB)
  doSoftMute(0);

  // Call doAgc(0), 0 = No incr/decr action (eqivalent to getAgc)
  // This gets disableAgc and agcNdx values based on mode (FM, AM , SSB)
  doAgc(0);

  // Call doAvc(0), 0 = No incr/decr action (eqivalent to getAvc)
  // This gets currentAVC values based on mode (AM, SSB)
  doAvc(0);

  delay(100);

  // Default
  currentFrequency = band[bandIdx].currentFreq;
  currentStepIdx = band[bandIdx].currentStepIdx;    // Default. Need to modify for AM/SSB as required


  if (currentMode == FM)
      idxFmStep = band[bandIdx].currentStepIdx;
  else
  {
    // Default for AM/SSB
    idxAmStep = band[bandIdx].currentStepIdx;


    // Update depending on currentMode and currentStepIdx
    // If outside SSB step ranges
    if (isSSB() && currentStepIdx >= AmTotalStepsSsb && currentStepIdx <AmTotalSteps)
    {
      currentStepIdx = 0;;
      idxAmStep = 0;
      band[bandIdx].currentStepIdx = 0;
    }

    // If outside AM step ranges
    if (currentMode == AM && currentStepIdx >= AmTotalSteps)
    {
      currentStepIdx = 0;;
      idxAmStep = 0;
      band[bandIdx].currentStepIdx = 0;
    }

  }

  // Debug
  #if DEBUG2_PRINT
  Serial.print("Info: useBand() >>> currentStepIdx = ");
  Serial.print(currentStepIdx);
  Serial.print(", idxAmStep = ");
  Serial.print(idxAmStep);
  Serial.print(", band[bandIdx].currentStepIdx = ");
  Serial.print(band[bandIdx].currentStepIdx);
  Serial.print(", currentMode = ");
  Serial.println(currentMode);
  #endif

  // Store mode
  bandMODE[bandIdx] = currentMode;               // G8PTN: Added to support mode per band

  rssi = 0;
  snr = 0;
  cleanBfoRdsInfo();
  showStatus();
}


void loadSSB()
{
  rx.setI2CFastModeCustom(400000); // You can try rx.setI2CFastModeCustom(700000); or greater value
  rx.loadPatch(ssb_patch_content, size_content, bandwidthSSB[bwIdxSSB].idx);
  rx.setI2CFastModeCustom(100000);
  ssbLoaded = true;
}

void correctCutoffFilter()
{
  // If audio bandwidth selected is about 2 kHz or below, it is recommended to set Sideband Cutoff Filter to 0.
  if (bandwidthSSB[bwIdxSSB].idx == 0 || bandwidthSSB[bwIdxSSB].idx == 4 || bandwidthSSB[bwIdxSSB].idx == 5)
    rx.setSSBSidebandCutoffFilter(0);
  else
    rx.setSSBSidebandCutoffFilter(1);
}

/**
 *  Switches the Bandwidth
 */
void doBandwidth(int8_t v)
{
    if (isSSB())
    {
      bwIdxSSB = (v == 1) ? bwIdxSSB + 1 : bwIdxSSB - 1;

      if (bwIdxSSB > maxSsbBw)
        bwIdxSSB = 0;
      else if (bwIdxSSB < 0)
        bwIdxSSB = maxSsbBw;

      rx.setSSBAudioBandwidth(bandwidthSSB[bwIdxSSB].idx);
      correctCutoffFilter();

      band[bandIdx].bandwidthIdx = bwIdxSSB;
    }
    else if (currentMode == AM)
    {
      bwIdxAM = (v == 1) ? bwIdxAM + 1 : bwIdxAM - 1;

      if (bwIdxAM > maxAmBw)
        bwIdxAM = 0;
      else if (bwIdxAM < 0)
        bwIdxAM = maxAmBw;

      rx.setBandwidth(bandwidthAM[bwIdxAM].idx, 1);
      band[bandIdx].bandwidthIdx = bwIdxAM;

    } else {
    bwIdxFM = (v == 1) ? bwIdxFM + 1 : bwIdxFM - 1;
    if (bwIdxFM > maxFmBw)
      bwIdxFM = 0;
    else if (bwIdxFM < 0)
      bwIdxFM = maxFmBw;

    rx.setFmBandwidth(bandwidthFM[bwIdxFM].idx);
    band[bandIdx].bandwidthIdx = bwIdxFM;
  }
  showBandwidth();
}

/**
 * Show cmd on display. It means you are setting up something.
 */
void showCommandStatus(char * currentCmd)
{
  if (display_on) {
    spr.drawString(currentCmd,38,14,2);
  }
  drawSprite();
}

/**
 *  AGC and attenuattion setup
 */
void doAgc(int8_t v) {

  // G8PTN: Modified to have separate AGC/ATTN per mode (FM, AM, SSB)
  if (currentMode == FM) {
    if      (v == 1)   FmAgcIdx ++;
    else if (v == -1)  FmAgcIdx --;

    // Limit range
    if (FmAgcIdx < 0)
      FmAgcIdx = 27;
    else if (FmAgcIdx > 27)
      FmAgcIdx = 0;

    // Select
    agcIdx = FmAgcIdx;
  }

  else if (isSSB()) {
    if      (v == 1)   SsbAgcIdx ++;
    else if (v == -1)  SsbAgcIdx --;

    // Limit range
    if (SsbAgcIdx < 0)
      SsbAgcIdx = 1;
    else if (SsbAgcIdx > 1)
      SsbAgcIdx = 0;

    // Select
    agcIdx = SsbAgcIdx;
  }

  else {
    if      (v == 1)   AmAgcIdx ++;
    else if (v == -1)  AmAgcIdx --;

    // Limit range
    if (AmAgcIdx < 0)
      AmAgcIdx = 37;
    else if (AmAgcIdx > 37)
      AmAgcIdx = 0;

    // Select
    agcIdx = AmAgcIdx;
  }

  // Process agcIdx to generate disableAgc and agcIdx
  // agcIdx     0 1 2 3 4 5 6  ..... n    (n:    FM = 27, AM = 37, SSB = 1)
  // agcNdx     0 0 1 2 3 4 5  ..... n -1 (n -1: FM = 26, AM = 36, SSB = 0)
  // disableAgc 0 1 1 1 1 1 1  ..... 1
  disableAgc = (agcIdx > 0);     // if true, disable AGC; else, AGC is enabled
  if (agcIdx > 1)
    agcNdx = agcIdx - 1;
  else
    agcNdx = 0;

  // Configure SI4732/5
  rx.setAutomaticGainControl(disableAgc, agcNdx); // if agcNdx = 0, no attenuation

  // Only call showAgcAtt() if incr/decr action (allows the doAgc(0) to act as getAgc)
  if (v != 0) showAgcAtt();
}


/**
 * Switches the current step
 */
void doStep(int8_t v)
{
    if ( currentMode == FM ) {
      idxFmStep = (v == 1) ? idxFmStep + 1 : idxFmStep - 1;
      if (idxFmStep > lastFmStep)
        idxFmStep = 0;
      else if (idxFmStep < 0)
        idxFmStep = lastFmStep;

      currentStepIdx = idxFmStep;
      rx.setFrequencyStep(tabFmStep[currentStepIdx]);
    }

    else {
      idxAmStep = (v == 1) ? idxAmStep + 1 : idxAmStep - 1;
      if (idxAmStep > getLastStep())
        idxAmStep = 0;
      else if (idxAmStep < 0)
        idxAmStep = getLastStep();

      //SSB Step limit
      else if (isSSB() && idxAmStep >= AmTotalStepsSsb && idxAmStep < AmTotalSteps)
          idxAmStep = v == 1 ? AmTotalSteps : AmTotalStepsSsb - 1;

      if (!isSSB() || (isSSB() && idxAmStep < AmTotalSteps)) {
          currentStepIdx = idxAmStep;
          rx.setFrequencyStep(tabAmStep[idxAmStep]);
      }

      /*
      if (!isSSB())
          rx.setSeekAmSpacing((band[bandIdx].currentStepIdx >= AmTotalSteps) ? 1 : tabStep[band[bandIdx].currentStepIdx]);
      */

      //showStep();

      currentStepIdx = idxAmStep;
      //rx.setFrequencyStep(tabAmStep[currentStepIdx]);
      rx.setSeekAmSpacing(5); // Max 10kHz for spacing
    }

    // Debug
    #if DEBUG2_PRINT
    int temp_LastStep = getLastStep();
    Serial.print("Info: doStep() >>> currentStepIdx = ");
    Serial.print(currentStepIdx);
    Serial.print(", getLastStep() = ");
    Serial.println(temp_LastStep);
    #endif

    band[bandIdx].currentStepIdx = currentStepIdx;
    showStep();
}

/**
 * Switches to the AM, LSB or USB modes
 */
void doMode(int8_t v)
{
  currentMode = bandMODE[bandIdx];               // G8PTN: Added to support mode per band

  if (currentMode != FM)                         // Nothing to do if FM mode
  {
    if (v == 1)  { // clockwise
      if (currentMode == AM)
      {
        // If you were in AM mode, it is necessary to load SSB patch (every time)

        showLoadingSSB();
        loadSSB();
        ssbLoaded = true;
        currentMode = LSB;
      }
      else if (currentMode == LSB)
        currentMode = USB;
      else if (currentMode == USB)
      {
        currentMode = AM;
        bfoOn = ssbLoaded = false;

        // G8PTN: When exiting SSB mode update the current frequency and BFO
        currentFrequency = currentFrequency + (currentBFO / 1000);
        currentBFO = 0;
      }
    } else { // and counterclockwise
      if (currentMode == AM)
      {
        // If you were in AM mode, it is necessary to load SSB patch (every time)
        showLoadingSSB();
        loadSSB();
        ssbLoaded = true;
        currentMode = USB;
      }
      else if (currentMode == USB)
        currentMode = LSB;
      else if (currentMode == LSB)
      {
        currentMode = AM;
        bfoOn = ssbLoaded = false;

        // G8PTN: When exiting SSB mode update the current frequency and BFO
        currentFrequency = currentFrequency + (currentBFO / 1000);
        currentBFO = 0;
      }
    }

    band[bandIdx].currentFreq = currentFrequency;
    band[bandIdx].currentStepIdx = currentStepIdx;
    bandMODE[bandIdx] = currentMode;                      // G8PTN: Added to support mode per band
    useBand();
  }
}

/**
 * Sets the audio volume
 */
void doVolume( int8_t v ) {
  if ( v == 1)
    rx.volumeUp();
  else
    rx.volumeDown();

  showVolume();
}

/**
 *  This function is called by the seek function process.  G8PTN: Added
 */
bool checkStopSeeking() {
  // Checks the seekStop flag
  return seekStop;  // returns true if the user rotates the encoder
}

/**
 *  This function is called by the seek function process.
 */
void showFrequencySeek(uint16_t freq)
{
  currentFrequency = freq;
  showFrequency();
}

/**
 *  Find a station. The direction is based on the last encoder move clockwise or counterclockwise
 */
void doSeek()
{
  if (isSSB()) return; // It does not work for SSB mode

  rx.seekStationProgress(showFrequencySeek, checkStopSeeking, seekDirection);   // G8PTN: Added checkStopSeeking
  currentFrequency = rx.getFrequency();

}

/**
 * Sets the Soft Mute Parameter
 */
void doSoftMute(int8_t v)
{
  // G8PTN: Modified to have separate SoftMute per mode (AM, SSB)
  // Only allow for AM and SSB modes
  if (currentMode != FM) {

    if (isSSB()) {
      if      (v == 1)   SsbSoftMuteIdx ++;
      else if (v == -1)  SsbSoftMuteIdx --;

      // Limit range
      if (SsbSoftMuteIdx < 0)
        SsbSoftMuteIdx = 32;
      else if (SsbSoftMuteIdx > 32)
        SsbSoftMuteIdx = 0;

      // Select
      softMuteMaxAttIdx = SsbSoftMuteIdx;
    }

    else {
      if      (v == 1)   AmSoftMuteIdx ++;
      else if (v == -1)  AmSoftMuteIdx --;

      // Limit range
      if (AmSoftMuteIdx < 0)
        AmSoftMuteIdx = 32;
      else if (AmSoftMuteIdx > 32)
        AmSoftMuteIdx = 0;

      // Select
      softMuteMaxAttIdx = AmSoftMuteIdx;
    }

  rx.setAmSoftMuteMaxAttenuation(softMuteMaxAttIdx);

  // Only call showSoftMute() if incr/decr action (allows the doSoftMute(0) to act as getSoftMute)
  if (v != 0) showSoftMute();
  }
}

/**
 *  Menu options selection
 */
void doMenu( int8_t v) {
  menuIdx = (v == 1) ? menuIdx + 1 : menuIdx - 1;               // G8PTN: Corrected direction

  if (menuIdx > lastMenu)
    menuIdx = 0;
  else if (menuIdx < 0)
    menuIdx = lastMenu;

  showMenu();
}

/**
 * Show menu options
 */
void showMenu() {
  drawSprite();
}


void doSettings( uint8_t v ) {
  settingsMenuIdx = (v == 1) ? settingsMenuIdx + 1 : settingsMenuIdx - 1;

  if (settingsMenuIdx > lastSettingsMenu)
    settingsMenuIdx = 0;
  else if (settingsMenuIdx < 0)
    settingsMenuIdx = lastSettingsMenu;

  showSettings();
}

void showSettings() {
  drawSprite();
}

/**
 * Starts the MENU action process
 */
void doCurrentMenuCmd() {
  disableCommands();
  switch (currentMenuCmd) {
     case MENU_VOLUME:
      if(muted) {
        rx.setVolume(mute_vol_val);
        muted = false;
      }
      cmdVolume = true;
      showVolume();
      break;
    case MENU_STEP:
      cmdStep = true;
      showStep();
      break;
    case MENU_MODE:
      cmdMode = true;
      showMode();
      break;
    #if BFO_MENU_EN
    case MENU_BFO:
      if (isSSB()) {
        bfoOn = true;
        showBFO();
      }
      showFrequency();
      break;
    #endif
    case MENU_BW:
      cmdBandwidth = true;
      showBandwidth();
      break;
    case MENU_AGC_ATT:
      cmdAgc = true;
      showAgcAtt();
      break;
    case MENU_SOFTMUTE:
      if (currentMode != FM) {
        cmdSoftMuteMaxAtt = true;
      }
      showSoftMute();
      break;
    case MENU_BAND:
      cmdBand = true;
      drawSprite();
      break;
    case MENU_MUTE:
      muted=!muted;
      if (muted)
      {
        mute_vol_val = rx.getVolume();
        rx.setVolume(0);
      }
      else rx.setVolume(mute_vol_val);
      drawSprite();
      break;

    // G8PTN: Added
    case MENU_CALIBRATION:
      if (isSSB()) {
        cmdCal = true;
        currentCAL = bandCAL[bandIdx];
      }
      showCal();
      break;

    // G8PTN: Added
    case MENU_AVC:
      if (currentMode != FM) {
        cmdAvc = true;
      }
      showAvc();
      break;

    case MENU_SETTINGS:
      cmdSettings = true;
      drawSprite();
      break;

  default:
      showStatus();
      break;
  }
  currentMenuCmd = -1;
}


/**
 * Starts the SETTINGS action process
 */
void doCurrentSettingsMenuCmd() {
  disableCommands();
  switch (currentSettingsMenuCmd) {
  case MENU_BRIGHTNESS:
      cmdBrt = true;
      showBrt();
      break;

  case MENU_SLEEP:
      cmdSleep = true;
      showSleep();
      break;

  case MENU_THEME:
      cmdTheme = true;
      showTheme();
      break;

  case MENU_ABOUT:
      cmdAbout = true;
      showAbout();
      break;

  default:
      showStatus();
      break;
  }
  currentSettingsMenuCmd = -1;
}


uint8_t getStrength() {
#if THEME_EDITOR
  return 17;
#endif
  if (currentMode != FM) {
    //dBuV to S point conversion HF
    if ((rssi <=  1)) return  1;                  // S0
    if ((rssi >  1) and (rssi <=  2)) return  2;  // S1         // G8PTN: Corrected table
    if ((rssi >  2) and (rssi <=  3)) return  3;  // S2
    if ((rssi >  3) and (rssi <=  4)) return  4;  // S3
    if ((rssi >  4) and (rssi <= 10)) return  5;  // S4
    if ((rssi > 10) and (rssi <= 16)) return  6;  // S5
    if ((rssi > 16) and (rssi <= 22)) return  7;  // S6
    if ((rssi > 22) and (rssi <= 28)) return  8;  // S7
    if ((rssi > 28) and (rssi <= 34)) return  9;  // S8
    if ((rssi > 34) and (rssi <= 44)) return 10;  // S9
    if ((rssi > 44) and (rssi <= 54)) return 11;  // S9 +10
    if ((rssi > 54) and (rssi <= 64)) return 12;  // S9 +20
    if ((rssi > 64) and (rssi <= 74)) return 13;  // S9 +30
    if ((rssi > 74) and (rssi <= 84)) return 14;  // S9 +40
    if ((rssi > 84) and (rssi <= 94)) return 15;  // S9 +50
    if  (rssi > 94)                   return 16;  // S9 +60
    if  (rssi > 95)                   return 17;  //>S9 +60
  }
  else
  {
    //dBuV to S point conversion FM
    if  ((rssi <=  1)) return  1;                 // G8PTN: Corrected table
    if ((rssi >  1) and (rssi <=  2)) return  7;  // S6
    if ((rssi >  2) and (rssi <=  8)) return  8;  // S7
    if ((rssi >  8) and (rssi <= 14)) return  9;  // S8
    if ((rssi > 14) and (rssi <= 24)) return 10;  // S9
    if ((rssi > 24) and (rssi <= 34)) return 11;  // S9 +10
    if ((rssi > 34) and (rssi <= 44)) return 12;  // S9 +20
    if ((rssi > 44) and (rssi <= 54)) return 13;  // S9 +30
    if ((rssi > 54) and (rssi <= 64)) return 14;  // S9 +40
    if ((rssi > 64) and (rssi <= 74)) return 15;  // S9 +50
    if  (rssi > 74)                   return 16;  // S9 +60
    if  (rssi > 76)                   return 17;  //>S9 +60
    // newStereoPilot=si4735.getCurrentPilot();
  }
  return 1;
}

// G8PTN: Alternative layout
void drawMenu() {
  if (!display_on) return;

  spr.setTextDatum(MC_DATUM);
  if (cmdMenu) {
    spr.fillSmoothRoundRect(1+menu_offset_x,1+menu_offset_y,76+menu_delta_x,110,4,theme[themeIdx].menu_border);
    spr.fillSmoothRoundRect(2+menu_offset_x,2+menu_offset_y,74+menu_delta_x,108,4,theme[themeIdx].menu_bg);
    spr.setTextColor(theme[themeIdx].menu_hdr, theme[themeIdx].menu_bg);

    char label_menu [16];
    sprintf(label_menu, "Menu %2.2d/%2.2d", (menuIdx + 1), (lastMenu + 1));
    //spr.drawString("Menu",38+menu_offset_x+(menu_delta_x/2),14+menu_offset_y,2);
    spr.drawString(label_menu,40+menu_offset_x+(menu_delta_x/2),12+menu_offset_y,2);
    spr.drawLine(1+menu_offset_x, 23+menu_offset_y, 76+menu_delta_x, 23+menu_offset_y, theme[themeIdx].menu_border);

    spr.setTextFont(0);
    spr.setTextColor(theme[themeIdx].menu_item, theme[themeIdx].menu_bg);
    spr.fillRoundRect(6+menu_offset_x,24+menu_offset_y+(2*16),66+menu_delta_x,16,2,theme[themeIdx].menu_hl_bg);
    for(int i=-2; i<3; i++){
      if (i==0) spr.setTextColor(theme[themeIdx].menu_hl_text,theme[themeIdx].menu_hl_bg);
      else spr.setTextColor(theme[themeIdx].menu_item,theme[themeIdx].menu_bg);
      spr.drawString(menu[abs((menuIdx+lastMenu+1+i)%(lastMenu+1))],40+menu_offset_x+(menu_delta_x/2),64+menu_offset_y+(i*16),2);
    }
  } else if (cmdSettings) {
    spr.fillSmoothRoundRect(1+menu_offset_x,1+menu_offset_y,76+menu_delta_x,110,4,theme[themeIdx].menu_border);
    spr.fillSmoothRoundRect(2+menu_offset_x,2+menu_offset_y,74+menu_delta_x,108,4,theme[themeIdx].menu_bg);
    spr.setTextColor(theme[themeIdx].menu_hdr, theme[themeIdx].menu_bg);
    spr.drawString("Settings",40+menu_offset_x+(menu_delta_x/2),12+menu_offset_y,2);
    spr.drawLine(1+menu_offset_x, 23+menu_offset_y, 76+menu_delta_x, 23+menu_offset_y, theme[themeIdx].menu_border);

    spr.setTextFont(0);
    spr.setTextColor(theme[themeIdx].menu_item,theme[themeIdx].menu_bg);
    spr.fillRoundRect(6+menu_offset_x,24+menu_offset_y+(2*16),66+menu_delta_x,16,2,theme[themeIdx].menu_hl_bg);
    for(int i=-2; i<3; i++) {
      if (i==0) spr.setTextColor(theme[themeIdx].menu_hl_text,theme[themeIdx].menu_hl_bg);
      else spr.setTextColor(theme[themeIdx].menu_item,theme[themeIdx].menu_bg);
      spr.drawString(settingsMenu[abs((settingsMenuIdx+lastSettingsMenu+1+i)%(lastSettingsMenu+1))],40+menu_offset_x+(menu_delta_x/2),64+menu_offset_y+(i*16),2);
    }
  } else {
    spr.setTextColor(theme[themeIdx].menu_hdr,theme[themeIdx].menu_bg);
    spr.fillSmoothRoundRect(1+menu_offset_x,1+menu_offset_y,76+menu_delta_x,110,4,theme[themeIdx].menu_border);
    spr.fillSmoothRoundRect(2+menu_offset_x,2+menu_offset_y,74+menu_delta_x,108,4,theme[themeIdx].menu_bg);
    if (isSettingsMode()) {
      spr.drawString(settingsMenu[settingsMenuIdx],40+menu_offset_x+(menu_delta_x/2),12+menu_offset_y,2);
    } else {
      spr.drawString(menu[menuIdx],40+menu_offset_x+(menu_delta_x/2),12+menu_offset_y,2);
    }
    spr.drawLine(1+menu_offset_x, 23+menu_offset_y, 76+menu_delta_x, 23+menu_offset_y, theme[themeIdx].menu_border);

    spr.setTextFont(0);
    spr.setTextColor(theme[themeIdx].menu_item,theme[themeIdx].menu_bg);
    spr.fillRoundRect(6+menu_offset_x,24+menu_offset_y+(2*16),66+menu_delta_x,16,2,theme[themeIdx].menu_hl_bg);

    //G8PTN: Added to reduce calls to getLastStep()
    int temp_LastStep = getLastStep();

    for(int i=-2;i<3;i++){
      if (i==0) spr.setTextColor(theme[themeIdx].menu_hl_text,theme[themeIdx].menu_hl_bg);
      else spr.setTextColor(theme[themeIdx].menu_item,theme[themeIdx].menu_bg);

      if (cmdMode) {
        if (currentMode == FM) {
          if (i==0) spr.drawString(bandModeDesc[abs((currentMode+lastBandModeDesc+1+i)%(lastBandModeDesc+1))],40+menu_offset_x+(menu_delta_x/2),64+menu_offset_y+(i*16),2);
        } else {
          spr.drawString(bandModeDesc[abs((currentMode+lastBandModeDesc+1+i)%(lastBandModeDesc+1))],40+menu_offset_x+(menu_delta_x/2),64+menu_offset_y+(i*16),2);
        }
      }
      if (cmdStep) {
        if (currentMode == FM) {
          spr.drawString(FmStepDesc[abs((currentStepIdx+lastFmStep+1+i)%(lastFmStep+1))],40+menu_offset_x+(menu_delta_x/2),64+menu_offset_y+(i*16),2);
        } else {
          spr.drawString(AmSsbStepDesc[abs((currentStepIdx+temp_LastStep+1+i)%(temp_LastStep+1))],40+menu_offset_x+(menu_delta_x/2),64+menu_offset_y+(i*16),2);
        }
      }
      if (cmdBand) {
        spr.drawString(band[abs((bandIdx+lastBand+1+i)%(lastBand+1))].bandName,40+menu_offset_x+(menu_delta_x/2),64+menu_offset_y+(i*16),2);
      }

      if (cmdBandwidth) {
        if (isSSB())
        {
          spr.drawString(bandwidthSSB[abs((bwIdxSSB+lastBandwidthSSB+1+i)%(lastBandwidthSSB+1))].desc,40+menu_offset_x+(menu_delta_x/2),64+menu_offset_y+(i*16),2);
          // bw = (char *)bandwidthSSB[bwIdxSSB].desc;
          // showBFO();
        }
        else if (currentMode == AM)
        {
          spr.drawString(bandwidthAM[abs((bwIdxAM+lastBandwidthAM+1+i)%(lastBandwidthAM+1))].desc,40+menu_offset_x+(menu_delta_x/2),64+menu_offset_y+(i*16),2);
        }
        else
        {
          spr.drawString(bandwidthFM[abs((bwIdxFM+lastBandwidthFM+1+i)%(lastBandwidthFM+1))].desc,40+menu_offset_x+(menu_delta_x/2),64+menu_offset_y+(i*16),2);
        }
      }

      if (cmdTheme) {
        spr.drawString(theme[abs((themeIdx+lastTheme+1+i)%(lastTheme+1))].name, 40+menu_offset_x+(menu_delta_x/2), 64+menu_offset_y+(i*16), 2);
      }
    }
    if (cmdVolume) {
      spr.setTextColor(theme[themeIdx].menu_param,theme[themeIdx].menu_bg);
      spr.fillRoundRect(6+menu_offset_x,24+menu_offset_y+(2*16),66+menu_delta_x,16,2,theme[themeIdx].menu_bg);
      spr.drawNumber(rx.getVolume(),40+menu_offset_x+(menu_delta_x/2),66+menu_offset_y,7);
    }
    if (cmdAgc) {
      spr.setTextColor(theme[themeIdx].menu_param,theme[themeIdx].menu_bg);
      spr.fillRoundRect(6+menu_offset_x,24+menu_offset_y+(2*16),66+menu_delta_x,16,2,theme[themeIdx].menu_bg);
      // rx.getAutomaticGainControl();             // G8PTN: Read back value is not used
      if (agcNdx == 0 && agcIdx == 0) {
        spr.setFreeFont(&Orbitron_Light_24);
        spr.drawString("AGC",40+menu_offset_x+(menu_delta_x/2),48+menu_offset_y);
        spr.drawString("On",40+menu_offset_x+(menu_delta_x/2),72+menu_offset_y);
        spr.setTextFont(0);
      } else {
        sprintf(sAgc, "%2.2d", agcNdx);
        spr.drawString(sAgc,40+menu_offset_x+(menu_delta_x/2),60+menu_offset_y,7);
      }
    }
    if (cmdSoftMuteMaxAtt) {
      spr.setTextColor(theme[themeIdx].menu_param,theme[themeIdx].menu_bg);
      spr.fillRoundRect(6+menu_offset_x,24+menu_offset_y+(2*16),66+menu_delta_x,16,2,theme[themeIdx].menu_bg);
      spr.drawString("Max Attn",40+menu_offset_x+(menu_delta_x/2),32+menu_offset_y,2);
      spr.drawNumber(softMuteMaxAttIdx,40+menu_offset_x+(menu_delta_x/2),60+menu_offset_y,4);
      spr.drawString("dB",40+menu_offset_x+(menu_delta_x/2),90+menu_offset_y,4);
    }

    // G8PTN: Added
    if (cmdCal) {
      spr.setTextColor(theme[themeIdx].menu_param,theme[themeIdx].menu_bg);
      spr.fillRoundRect(6+menu_offset_x,24+menu_offset_y+(2*16),66+menu_delta_x,16,2,theme[themeIdx].menu_bg);
      spr.drawNumber(currentCAL,40+menu_offset_x+(menu_delta_x/2),60+menu_offset_y,4);
      spr.drawString("Hz",40+menu_offset_x+(menu_delta_x/2),90+menu_offset_y,4);
    }

    // G8PTN: Added
    if (cmdAvc) {
      spr.setTextColor(theme[themeIdx].menu_param,theme[themeIdx].menu_bg);
      spr.fillRoundRect(6+menu_offset_x,24+menu_offset_y+(2*16),66+menu_delta_x,16,2,theme[themeIdx].menu_bg);
      spr.drawString("Max Gain",40+menu_offset_x+(menu_delta_x/2),32+menu_offset_y,2);
      spr.drawNumber(currentAVC,40+menu_offset_x+(menu_delta_x/2),60+menu_offset_y,4);
      spr.drawString("dB",40+menu_offset_x+(menu_delta_x/2),90+menu_offset_y,4);
    }

    // G8PTN: Added
    if (cmdBrt) {
      spr.setTextColor(theme[themeIdx].menu_param,theme[themeIdx].menu_bg);
      spr.fillRoundRect(6+menu_offset_x,24+menu_offset_y+(2*16),66+menu_delta_x,16,2,theme[themeIdx].menu_bg);
      spr.drawNumber(currentBrt,40+menu_offset_x+(menu_delta_x/2),60+menu_offset_y,4);
    }

    if (cmdSleep) {
      spr.setTextColor(theme[themeIdx].menu_param,theme[themeIdx].menu_bg);
      spr.fillRoundRect(6+menu_offset_x,24+menu_offset_y+(2*16),66+menu_delta_x,16,2,theme[themeIdx].menu_bg);
      spr.drawNumber(currentSleep,40+menu_offset_x+(menu_delta_x/2),60+menu_offset_y,4);
    }
  }
}


void drawSprite()
{
  if (!display_on) return;
  spr.fillSprite(theme[themeIdx].bg);

  // Time
  if (time_synchronized) {
    spr.setTextColor(theme[themeIdx].text, theme[themeIdx].bg);
    spr.setTextDatum(ML_DATUM);
    spr.drawString(time_disp, batt_offset_x - 8, batt_offset_y + 24, 2); // Position below battery icon
    spr.setTextColor(theme[themeIdx].text, theme[themeIdx].bg);
  }

  /* // Screen activity icon */
  /* screen_toggle = !screen_toggle; */
  /* spr.drawCircle(clock_datum+50,11,6,theme[themeIdx].text); */
  /* if (screen_toggle) spr.fillCircle(clock_datum+50,11,5,theme[themeIdx].bg); */
  /* else               spr.fillCircle(clock_datum+50,11,5,TFT_GREEN); */

  // EEPROM write request icon
#if THEME_EDITOR
  eeprom_wr_flag = true;
#endif
  if (eeprom_wr_flag){
    spr.fillRect(save_offset_x+3, save_offset_y+2, 3, 5, theme[themeIdx].save_icon);
    spr.fillTriangle(save_offset_x+1, save_offset_y+7, save_offset_x+7, save_offset_y+7, save_offset_x+4, save_offset_y+10, theme[themeIdx].save_icon);
    spr.drawLine(save_offset_x, save_offset_y+12, save_offset_x, save_offset_y+13, theme[themeIdx].save_icon);
    spr.drawLine(save_offset_x, save_offset_y+13, save_offset_x+8, save_offset_y+13, theme[themeIdx].save_icon);
    spr.drawLine(save_offset_x+8, save_offset_y+13, save_offset_x+8, save_offset_y+12, theme[themeIdx].save_icon);
    eeprom_wr_flag = false;
  }

  if (cmdAbout) {
    // About screen
    spr.setTextDatum(TL_DATUM);
    spr.setTextColor(theme[themeIdx].text_muted, theme[themeIdx].bg);
    spr.drawString("ESP32-SI4732 Receiver", 0, 0, 4);
    spr.setTextColor(theme[themeIdx].text, theme[themeIdx].bg);
    get_fw_ver();
    spr.drawString(fw_ver, 2, 33, 2);
    spr.drawString("https://github.com/esp32-si4732/ats-mini", 2, 33 + 16, 2);
    spr.drawString("Authors: PU2CLR (Ricardo Caratti),", 2, 33 + 16 * 3, 2);
    spr.drawString("Volos Projects, Ralph Xavier, Sunnygold,", 2, 33 + 16 * 4, 2);
    spr.drawString("Goshante, G8PTN (Dave), R9UCL (Max Arnold)", 2, 33 + 16 * 5, 2);
  } else {
    // Band and mode
    spr.setFreeFont(&Orbitron_Light_24);
    spr.setTextDatum(TC_DATUM);
    spr.setTextColor(theme[themeIdx].band_text, theme[themeIdx].bg);
    uint16_t band_width = spr.drawString(band[bandIdx].bandName, band_offset_x, band_offset_y);
    spr.setTextDatum(TL_DATUM);
    spr.setTextColor(theme[themeIdx].mode_text, theme[themeIdx].bg);
    uint16_t mode_width = spr.drawString(bandModeDesc[currentMode], band_offset_x + band_width / 2 + 12, band_offset_y + 8, 2);
    spr.drawSmoothRoundRect(band_offset_x + band_width / 2 + 7, band_offset_y + 7, 4, 4, mode_width + 8, 17, theme[themeIdx].mode_border, theme[themeIdx].bg);

#if THEME_EDITOR
    spr.setTextDatum(TR_DATUM);
    spr.setTextColor(theme[themeIdx].text_warn, theme[themeIdx].bg);
    spr.drawString("WARN", 319, rds_offset_y, 4);
#endif

    if (currentMode == FM) {
      // FM frequency
      spr.setTextDatum(MR_DATUM);
      spr.setTextColor(theme[themeIdx].freq_text, theme[themeIdx].bg);
      spr.drawFloat(currentFrequency/100.00, 2, freq_offset_x, freq_offset_y, 7);
      spr.setTextDatum(ML_DATUM);
      spr.setTextColor(theme[themeIdx].funit_text, theme[themeIdx].bg);
      spr.drawString("MHz", funit_offset_x, funit_offset_y);
    } else {
      // Other modes frequency
      spr.setTextDatum(MR_DATUM);
      spr.setTextColor(theme[themeIdx].freq_text, theme[themeIdx].bg);
      if (isSSB()) {
        uint32_t freq  = (uint32_t(currentFrequency) * 1000) + currentBFO;
        uint16_t khz   = freq / 1000;
        uint16_t tail  = (freq % 1000);
        char skhz [32];
        char stail [32];
        sprintf(skhz, "%3.3u", khz);
        sprintf(stail, ".%3.3d", tail);
        spr.drawString(skhz, freq_offset_x, freq_offset_y, 7);
        spr.setTextDatum(ML_DATUM);
        spr.drawString(stail, 5+freq_offset_x, 15+freq_offset_y, 4);
      } else {
        spr.drawNumber(currentFrequency, freq_offset_x, freq_offset_y, 7);
        spr.setTextDatum(ML_DATUM);
        spr.drawString(".000", 5+freq_offset_x, 15+freq_offset_y, 4);
      }
      spr.setTextColor(theme[themeIdx].funit_text, theme[themeIdx].bg);
      spr.drawString("kHz", funit_offset_x, funit_offset_y);
    }

    if (isModalMode()) {
      drawMenu();
    } else {
      // Info box
      spr.setTextDatum(ML_DATUM);
      spr.setTextColor(theme[themeIdx].box_text, theme[themeIdx].box_bg);
      spr.fillSmoothRoundRect(1+menu_offset_x, 1+menu_offset_y, 76+menu_delta_x, 110, 4, theme[themeIdx].box_border);
      spr.fillSmoothRoundRect(2+menu_offset_x, 2+menu_offset_y, 74+menu_delta_x, 108, 4, theme[themeIdx].box_bg);
      spr.drawString("Step:",6+menu_offset_x,64+menu_offset_y+(-3*16),2);
      if (currentMode == FM) spr.drawString(FmStepDesc[currentStepIdx],48+menu_offset_x,64+menu_offset_y+(-3*16),2);
      else spr.drawString(AmSsbStepDesc[currentStepIdx],48+menu_offset_x,64+menu_offset_y+(-3*16),2);
      spr.drawString("BW:",6+menu_offset_x,64+menu_offset_y+(-2*16),2);
      if (isSSB())
        {
          spr.drawString(bandwidthSSB[bwIdxSSB].desc,48+menu_offset_x,64+menu_offset_y+(-2*16),2);
        }
      else if (currentMode == AM)
        {
          spr.drawString(bandwidthAM[bwIdxAM].desc,48+menu_offset_x,64+menu_offset_y+(-2*16),2);
        }
      else
        {
          spr.drawString(bandwidthFM[bwIdxFM].desc,48+menu_offset_x,64+menu_offset_y+(-2*16),2);
        }
      if (agcNdx == 0 && agcIdx == 0) {
        spr.drawString("AGC:",6+menu_offset_x,64+menu_offset_y+(-1*16),2);
        spr.drawString("On",48+menu_offset_x,64+menu_offset_y+(-1*16),2);
      } else {
        sprintf(sAgc, "%2.2d", agcNdx);
        spr.drawString("Att:",6+menu_offset_x,64+menu_offset_y+(-1*16),2);
        spr.drawString(sAgc,48+menu_offset_x,64+menu_offset_y+(-1*16),2);
      }
      spr.drawString("AVC:", 6+menu_offset_x, 64+menu_offset_y + (0*16), 2);
      if (currentMode !=FM) {
        if (isSSB()) {
          sprintf(sAvc, "%2.2ddB", SsbAvcIdx);
        } else {
          sprintf(sAvc, "%2.2ddB", AmAvcIdx);
        }
      } else {
        sprintf(sAvc, "n/a");
      }
      spr.drawString(sAvc, 48+menu_offset_x, 64+menu_offset_y + (0*16), 2);
      /*
        spr.drawString("BFO:",6+menu_offset_x,64+menu_offset_y+(2*16),2);
        if (isSSB()) {
        spr.setTextDatum(MR_DATUM);
        spr.drawString(bfo,74+menu_offset_x,64+menu_offset_y+(2*16),2);
        }
        else spr.drawString("Off",48+menu_offset_x,64+menu_offset_y+(2*16),2);
        spr.setTextDatum(MC_DATUM);
      */

      spr.drawString("Vol:",6+menu_offset_x,64+menu_offset_y+(1*16),2);
      if (muted) {
        //spr.setTextDatum(MR_DATUM);
        spr.setTextColor(theme[themeIdx].box_off_text, theme[themeIdx].box_off_bg);
        spr.drawString("Muted",48+menu_offset_x,64+menu_offset_y+(1*16),2);
      } else {
        spr.setTextColor(theme[themeIdx].box_text, theme[themeIdx].box_bg);
        spr.drawNumber(rx.getVolume(),48+menu_offset_x,64+menu_offset_y+(1*16),2);
      }
    }

    if (bfoOn) {
      spr.setTextDatum(ML_DATUM);
      spr.setTextColor(theme[themeIdx].text, theme[themeIdx].bg);
      spr.drawString("BFO:",10,158,4);
      spr.drawString(bfo,80,158,4);
    }

    // S-Meter
    spr.drawTriangle(meter_offset_x + 1, meter_offset_y + 1, meter_offset_x + 11, meter_offset_y + 1, meter_offset_x + 6, meter_offset_y + 6, theme[themeIdx].smeter_icon);
    spr.drawLine(meter_offset_x + 6, meter_offset_y + 1, meter_offset_x + 6, meter_offset_y + 14, theme[themeIdx].smeter_icon);
    for(int i=0; i<getStrength(); i++) {
      if (i<10) {
        spr.fillRect(15+meter_offset_x + (i*4), 2+meter_offset_y, 2, 12, theme[themeIdx].smeter_bar);
      } else {
        spr.fillRect(15+meter_offset_x + (i*4), 2+meter_offset_y, 2, 12, theme[themeIdx].smeter_bar_plus);
      }
    }

    // RDS info
    if (currentMode == FM) {
      if (rx.getCurrentPilot()) {
        spr.fillRect(15 + meter_offset_x, 7+meter_offset_y, 4*17, 2, theme[themeIdx].bg);
      }

      spr.setTextDatum(TC_DATUM);
      spr.setTextColor(theme[themeIdx].rds_text, theme[themeIdx].bg);
#if THEME_EDITOR
      spr.drawString("*STATION*", rds_offset_x, rds_offset_y, 4);
#else
      spr.drawString(bufferStationName, rds_offset_x, rds_offset_y, 4);
#endif
    }

    if (isCB()) {
      spr.setTextDatum(TC_DATUM);
      spr.setTextColor(theme[themeIdx].rds_text, theme[themeIdx].bg);
      spr.drawString(bufferStationName, rds_offset_x, rds_offset_y, 4);
    }

    // Tuner scale
    spr.fillTriangle(156, 122, 160, 132, 164, 122, theme[themeIdx].scale_pointer);
    spr.drawLine(160, 124, 160, 169, theme[themeIdx].scale_pointer);

    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(theme[themeIdx].scale_text, theme[themeIdx].bg);
    int temp;
    if (isSSB()) {
      temp = (currentFrequency + currentBFO/1000)/10.00 - 20;
    } else {
      temp = currentFrequency/10.00 - 20;
    }
    uint16_t lineColor;
    for(int i=0; i<40; i++) {
      if (i==20) lineColor=theme[themeIdx].scale_pointer;
      else lineColor=theme[themeIdx].scale_line;
      if (!(temp<band[bandIdx].minimumFreq/10.00 or temp>band[bandIdx].maximumFreq/10.00)) {
        if((temp%10)==0){
          spr.drawLine(i*8, 169, i*8, 150, lineColor);
          spr.drawLine((i*8)+1, 169, (i*8)+1, 150, lineColor);
          if (currentMode == FM) spr.drawFloat(temp/10.0, 1, i*8, 140, 2);
          else if (temp >= 100) spr.drawFloat(temp/100.0, 3, i*8, 140, 2);
          else spr.drawNumber(temp*10, i*8, 140, 2);
        } else if((temp%5)==0 && (temp%10)!=0) {
          spr.drawLine(i*8, 169, i*8, 155, lineColor);
          spr.drawLine((i*8)+1, 169, (i*8)+1, 155, lineColor);
        } else {
          spr.drawLine(i*8, 169, i*8, 160, lineColor);
        }
      }
      temp += 1;
    }
  }

#if TUNE_HOLDOFF
  // Update if not tuning
  if (tuning_flag == false) {
    if (!cmdAbout) batteryMonitor();
    spr.pushSprite(0,0);
  }
#else
  // No hold off
  if (!cmdAbout) batteryMonitor();
  spr.pushSprite(0,0);
#endif

}


void cleanBfoRdsInfo()
{
  bufferStationName[0]='\0';
}

void showRDSMsg()
{
  rdsMsg[35] = bufferRdsMsg[35] = '\0';
  if (strcmp(bufferRdsMsg, rdsMsg) == 0)
    return;
}

void showRDSStation()
{
  if (strcmp(bufferStationName, stationName) == 0 ) return;
  cleanBfoRdsInfo();
  strcpy(bufferStationName, stationName);
  drawSprite();
}

void showRDSTime()
{
  if (strcmp(bufferRdsTime, rdsTime) == 0)
    return;

  if (snr < 12) return; // Do not synchronize if the signal is weak

  // Copy new RDS time to buffer
  strcpy(bufferRdsTime, rdsTime);
  
  // Synchronize internal time with RDS time
  syncTimeFromRDS(rdsTime);
  
  // Display updated time (optional)
  drawSprite();
}

void checkRDS()
{
  rx.getRdsStatus();
  if (rx.getRdsReceived())
  {
    if (rx.getRdsSync() && rx.getRdsSyncFound())
    {
      rdsMsg = rx.getRdsText2A();
      stationName = rx.getRdsText0A();
      rdsTime = rx.getRdsTime();
      
      // if ( rdsMsg != NULL )   showRDSMsg();
      if (stationName != NULL)
          showRDSStation();
      if (rdsTime != NULL) showRDSTime();
    }
  }
}

void checkCBChannel()
{
  const int column_step = 450;  // In kHz
  const int row_step = 10;
  const int max_columns = 8; // A-H
  const int max_rows = 45;

  if (currentFrequency < MIN_CB_FREQUENCY || currentFrequency > MAX_CB_FREQUENCY) {
    bufferStationName[0] = '\0';
    return;
  }

  int offset = currentFrequency - MIN_CB_FREQUENCY;
  char type = 'R';

  if (offset % 10 == 5) {
    type = 'E';
    offset -= 5;
  }

  int column_index = offset / column_step;

  if (column_index >= max_columns) {
    bufferStationName[0] = '\0';
    return;
  }

  int remainder = offset % column_step;

  if (remainder % row_step != 0) {
    bufferStationName[0] = '\0';
    return;
  }

  int row_number = remainder / row_step;

  if (row_number >= max_rows || row_number < 0) {
    bufferStationName[0] = '\0';
    return;
  }

  sprintf(bufferStationName, "%c%s%c", 'A' + column_index, cbChannelNumber[row_number], type);
}

/***************************************************************************************
** Function name:           batteryMonitor
** Description:             Check Battery Level and Draw to level icon
***************************************************************************************/
// Check Battery Level
void batteryMonitor() {

  // Read ADC and calculate average
  adc_read_total = 0;                 // Reset to 0 at start of calculation
  for (int i = 0; i < 10; i++) {
    adc_read_total += analogRead(VBAT_MON);
  }
  adc_read_avr = (adc_read_total / BATT_ADC_READS);

  // Calculated average voltage with correction factor
  adc_volt_avr = adc_read_avr * BATT_ADC_FACTOR / 1000;

  // State machine
  // SOC (%)      batt_soc_state
  //  0 to 25           0
  // 25 to 50           1
  // 50 to 75           2
  // 75 to 100          3

  switch (batt_soc_state) {
  case 0:
    if      (adc_volt_avr > (BATT_SOC_LEVEL1 + BATT_SOC_HYST_2)) batt_soc_state = 1;   // State 0 > 1
    break;

  case 1:
    if      (adc_volt_avr > (BATT_SOC_LEVEL2 + BATT_SOC_HYST_2)) batt_soc_state = 2;   // State 1 > 2
    else if (adc_volt_avr < (BATT_SOC_LEVEL1 - BATT_SOC_HYST_2)) batt_soc_state = 0;   // State 1 > 0
    break;

  case 2:
    if      (adc_volt_avr > (BATT_SOC_LEVEL3 + BATT_SOC_HYST_2)) batt_soc_state = 3;   // State 2 > 3
    else if (adc_volt_avr < (BATT_SOC_LEVEL2 - BATT_SOC_HYST_2)) batt_soc_state = 1;   // State 2 > 1
    break;

  case 3:
    if      (adc_volt_avr < (BATT_SOC_LEVEL3 - BATT_SOC_HYST_2)) batt_soc_state = 2;   // State 3 > 2
    break;

  default:
    if      (batt_soc_state > 3) batt_soc_state = 0;                                   // State (Illegal) > 0
    else    batt_soc_state = batt_soc_state;                                           // Keep current state
    break;
  }

  // Debug
#if DEBUG3_PRINT
  Serial.print("Info: batteryMonitor() >>> batt_soc_state = "); Serial.print(batt_soc_state); Serial.print(", ");
  Serial.print("ADC count (average) = "); Serial.print(adc_read_avr); Serial.print(", ");
  Serial.print("ADC voltage (average) = "); Serial.println(adc_volt_avr);
#endif

  if (!display_on) return;

  // SOC display information
  // Variable: chargeLevel = pixel width, batteryLevelColor = Colour of level
  int chargeLevel = 24;
  uint16_t batteryLevelColor = theme[themeIdx].batt_full;

  if (batt_soc_state == 0 ) {
    chargeLevel=6;
    batteryLevelColor=theme[themeIdx].batt_low;
  }
  if (batt_soc_state == 1 ) {
    chargeLevel=12;
    batteryLevelColor=theme[themeIdx].batt_full;
  }
  if (batt_soc_state == 2 ) {
    chargeLevel=18;
    batteryLevelColor=theme[themeIdx].batt_full;
  }
  if (batt_soc_state == 3 ) {
    chargeLevel=24;
    batteryLevelColor=theme[themeIdx].batt_full;
  }

  // Set display information
  spr.drawRoundRect(batt_offset_x, batt_offset_y + 1, 28, 14, 3, theme[themeIdx].batt_border);
  spr.drawLine(batt_offset_x + 29, batt_offset_y + 5, batt_offset_x + 29, batt_offset_y + 10, theme[themeIdx].batt_border);
  spr.drawLine(batt_offset_x + 30, batt_offset_y + 6, batt_offset_x + 30, batt_offset_y + 9, theme[themeIdx].batt_border);

  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(theme[themeIdx].batt_voltage, theme[themeIdx].bg);

#if THEME_EDITOR
  spr.drawRoundRect(batt_offset_x - 31, batt_offset_y + 1, 28, 14, 3, theme[themeIdx].batt_border);
  spr.drawLine(batt_offset_x - 31 + 29, batt_offset_y + 5, batt_offset_x - 31 + 29, batt_offset_y + 10, theme[themeIdx].batt_border);
  spr.drawLine(batt_offset_x - 31 + 30, batt_offset_y + 6, batt_offset_x - 31 + 30, batt_offset_y + 9, theme[themeIdx].batt_border);

  spr.fillRoundRect(batt_offset_x - 31 + 2, batt_offset_y + 3, 18, 10, 2, theme[themeIdx].batt_full);
  spr.fillRoundRect(batt_offset_x - 31 + 2, batt_offset_y + 3, 12, 10, 2, theme[themeIdx].batt_low);
  spr.drawString("4.0V", batt_offset_x - 31 - 3, batt_offset_y, 2);

  adc_volt_avr = 4.5;
#endif
  // The hardware has a load sharing circuit to allow simultaneous charge and power
  // With USB(5V) connected the voltage reading will be approx. VBUS - Diode Drop = 4.65V
  // If the average voltage is greater than 4.3V, show ligtning on the display
  if (adc_volt_avr > 4.3) {
    spr.fillRoundRect(batt_offset_x + 2, batt_offset_y + 3, 24, 10, 2, theme[themeIdx].batt_charge);
    spr.drawLine(batt_offset_x + 9 + 8, batt_offset_y + 1, batt_offset_x + 9 + 6, batt_offset_y + 1 + 5, theme[themeIdx].bg);
    spr.drawLine(batt_offset_x + 9 + 6, batt_offset_y + 1 + 5, batt_offset_x + 9 + 10, batt_offset_y + 1 + 5, theme[themeIdx].bg);
    spr.drawLine(batt_offset_x + 9 + 11, batt_offset_y + 1 + 6, batt_offset_x + 9 + 4, batt_offset_y + 1 + 13, theme[themeIdx].bg);
    spr.drawLine(batt_offset_x + 9 + 2, batt_offset_y + 1 + 13, batt_offset_x + 9 + 4, batt_offset_y + 1 + 8, theme[themeIdx].bg);
    spr.drawLine(batt_offset_x + 9 + 4, batt_offset_y + 1 + 8, batt_offset_x + 9 + 0, batt_offset_y + 1 + 8, theme[themeIdx].bg);
    spr.drawLine(batt_offset_x + 9 - 1, batt_offset_y + 1 + 7, batt_offset_x + 9 + 6, batt_offset_y + 1 + 0, theme[themeIdx].bg);
    spr.fillTriangle(batt_offset_x + 9 + 7, batt_offset_y + 1, batt_offset_x + 9 + 4, batt_offset_y + 1 + 6, batt_offset_x + 9, batt_offset_y + 1 + 7, theme[themeIdx].batt_icon);
    spr.fillTriangle(batt_offset_x + 9 + 5, batt_offset_y + 1 + 6, batt_offset_x + 9 + 10, batt_offset_y + 1 + 6, batt_offset_x + 9 + 3, batt_offset_y + 1 + 13, theme[themeIdx].batt_icon);
    spr.fillRect(batt_offset_x + 9 + 1, batt_offset_y + 1 + 6, 9, 2, theme[themeIdx].batt_icon);
    spr.drawPixel(batt_offset_x + 9 + 3, batt_offset_y + 1 + 12, theme[themeIdx].batt_icon);
  } else {
    char voltage[8];
    spr.fillRoundRect(batt_offset_x + 2, batt_offset_y + 3, chargeLevel, 10, 2, batteryLevelColor);
    sprintf(voltage, "%.02fV", adc_volt_avr);
    spr.drawString(voltage, batt_offset_x - 3, batt_offset_y, 2);
  }
}


/***************************************************************************************
** Description:   In SSB mode tuning uses VFO and BFO
**                - Algorithm from ATS-20_EX Goshante firmware
***************************************************************************************/
// Tuning algorithm
void doFrequencyTuneSSB(bool fast = false) {
    int step = encoderCount == 1 ? getSteps(fast) : getSteps(fast) * -1;
    int newBFO = currentBFO + step;
    int redundant = 0;

    if (newBFO > BFOMax) {
        redundant = (newBFO / BFOMax) * BFOMax;
        currentFrequency += redundant / 1000;
        newBFO -= redundant;
    } else if (newBFO < -BFOMax) {
        redundant = ((abs(newBFO) / BFOMax) * BFOMax);
        currentFrequency -= redundant / 1000;
        newBFO += redundant;
    }

    currentBFO = newBFO;
    updateBFO();

    if (redundant != 0) {
        clampSSBBand();                                   // G8PTN: Added
        rx.setFrequency(currentFrequency);
        //agcSetFunc(); //Re-apply to remove noize        // G8PTN: Commented out
        currentFrequency = rx.getFrequency();
    }

    band[bandIdx].currentFreq = currentFrequency + (currentBFO / 1000);     // Update band table currentFreq

    if (clampSSBBand()) {
      // Debug
      #if DEBUG1_PRINT
      Serial.println("Info: clampSSBBand() >>> SSB Band Clamp !");
      #endif
    }
}

// Clamp SSB tuning to band limits
bool clampSSBBand()
{
    uint16_t freq = currentFrequency + (currentBFO / 1000);

    // Special case to cover SSB frequency negative!
    bool SsbFreqNeg = false;
    if (currentFrequency & 0x8000)
      SsbFreqNeg = true;

    // Priority to minimum check to cover SSB frequency negative
    bool upd = false;
    if (freq < band[bandIdx].minimumFreq || SsbFreqNeg)
    {
        currentFrequency = band[bandIdx].maximumFreq;
        upd = true;
    }
    else if (freq > band[bandIdx].maximumFreq)
    {
        currentFrequency = band[bandIdx].minimumFreq;
        upd = true;
    }

    if (upd)
    {
        band[bandIdx].currentFreq = currentFrequency;    // Update band table currentFreq
        rx.setFrequency(currentFrequency);
        currentBFO = 0;
        updateBFO();
        return true;
    }

    return false;
}


void updateBFO()
{
    // To move frequency forward, need to move the BFO backwards, so multiply by -1
    currentCAL = bandCAL[bandIdx];    // Select from table
    rx.setSSBBfo((currentBFO + currentCAL) * -1);

    // Debug
    #if DEBUG2_PRINT
    Serial.print("Info: updateBFO() >>> ");
    Serial.print("currentBFO = ");
    Serial.print(currentBFO);
    Serial.print(", currentCAL = ");
    Serial.print(currentCAL);
    Serial.print(", rx.setSSBbfo() = ");
    Serial.println((currentBFO + currentCAL) * -1);
    #endif
}


void doCal( int16_t v ) {
  currentCAL = bandCAL[bandIdx];    // Select from table
  if ( v == 1) {
    currentCAL = currentCAL + 10;
    if (currentCAL > CALMax) currentCAL = CALMax;
  }

  else {
    currentCAL = currentCAL - 10;
    if (currentCAL < -CALMax) currentCAL = -CALMax;
  }
  bandCAL[bandIdx] = currentCAL;    // Store to table

  // If in SSB mode set the SI4732/5 BFO value
  // This adjustments the BFO whilst in the calibration menu
  if (isSSB()) updateBFO();

  showCal();
}

void showCal()
{
drawSprite();
}


void doBrt( uint16_t v ) {
  if ( v == 1) {
    currentBrt = currentBrt + 32;
    if (currentBrt > 255) currentBrt = 255;
  } else {
    if (currentBrt == 255) currentBrt = currentBrt - 31;
    else currentBrt = currentBrt - 32;
    if (currentBrt < 32) currentBrt = 32;
  }

  if (display_on)
    ledcWrite(PIN_LCD_BL, currentBrt);
  showBrt();
}

void showBrt()
{
drawSprite();
}

void showAbout() {
  drawSprite();
}

void doSleep( uint16_t v ) {
  if ( v == 1) {
    currentSleep = currentSleep + 5;
    if (currentSleep > 255) currentSleep = 255;
  } else {
    if (currentSleep >= 5) currentSleep = currentSleep - 5;
    else currentSleep = 0;
  }

  showSleep();
}

void showSleep() {
  drawSprite();
}

void doTheme( uint16_t v ) {
  if (v == 1)
    themeIdx = (themeIdx < lastTheme) ? (themeIdx + 1) : 0;
  else
    themeIdx = (themeIdx > 0) ? (themeIdx - 1) : lastTheme;

  showTheme();
}

void showTheme() {
  drawSprite();
}

void doAvc(int16_t v) {
  // Only allow for AM and SSB modes
  if (currentMode != FM) {

    if (isSSB()) {
      if      (v == 1)   SsbAvcIdx += 2;
      else if (v == -1)  SsbAvcIdx -= 2;

      // Limit range
      if (SsbAvcIdx < 12)
        SsbAvcIdx = 90;
      else if (SsbAvcIdx > 90)
        SsbAvcIdx = 12;

      // Select
      currentAVC = SsbAvcIdx;
    }

    else {
      if      (v == 1)   AmAvcIdx += 2;
      else if (v == -1)  AmAvcIdx -= 2;

      // Limit range
      if (AmAvcIdx < 12)
        AmAvcIdx = 90;
      else if (AmAvcIdx > 90)
        AmAvcIdx = 12;

      // Select
      currentAVC = AmAvcIdx;
    }

  // Configure SI4732/5
  rx.setAvcAmMaxGain(currentAVC);

  // Only call showAvc() if incr/decr action (allows the doAvc(0) to act as getAvc)
  if (v != 0) showAvc();
  }
}

void showAvc()
{
drawSprite();
}


void buttonCheck() {
  // G8PTN: Added
  // Push button detection
  // Only execute every 10 ms
  if ((millis() - pb1_time) > 10) {
    pb1_time = millis();
    pb1_current = digitalRead(ENCODER_PUSH_BUTTON);        // Read pin value
    if (pb1_last != pb1_current) {                         // Start debounce timer
      pb1_edge_time = millis();
      pb1_last = pb1_current;
    }

    if ((millis() - pb1_edge_time) > CLICK_TIME) {         // Debounced
      if (pb1_stable == HIGH && pb1_last == LOW) {         // button is pressed
        // Debug
        #if DEBUG2_PRINT
        Serial.println("Info: button_check() >>> Button Pressed");
        #endif
        pb1_pressed_time = pb1_edge_time;
        pb1_short_pressed_time = pb1_long_pressed_time = 0;
        pb1_stable = pb1_last;
        pb1_pressed = true;                                // Set flags
        pb1_short_pressed = false;
        pb1_long_pressed = false;
        pb1_released = false;
        pb1_short_released = false;
        pb1_long_released = false;
      } else if (pb1_stable == LOW && pb1_last == LOW) {   // button is still pressed
        long pb1_press_duration = millis() - pb1_pressed_time;
        if (pb1_press_duration > SHORT_PRESS_TIME && (pb1_short_pressed_time - pb1_pressed_time) != SHORT_PRESS_TIME) {
          pb1_short_pressed = true;
          pb1_short_pressed_time = pb1_pressed_time + SHORT_PRESS_TIME;
          #if DEBUG2_PRINT
          Serial.println("Info: button_check() >>> Short Press triggered");
          #endif
        }
        if (pb1_press_duration > LONG_PRESS_TIME && (pb1_long_pressed_time - pb1_pressed_time) != LONG_PRESS_TIME) {
          pb1_short_pressed = false;
          pb1_long_pressed = true;
          pb1_long_pressed_time = pb1_pressed_time + LONG_PRESS_TIME;
          #if DEBUG2_PRINT
          Serial.println("Info: button_check() >>> Long Press triggered");
          #endif
        }
      } else if (pb1_stable == LOW && pb1_last == HIGH) {  // button is released
        // Debug
        #if DEBUG2_PRINT
        Serial.println("Info: button_check() >>> Button Released");
        #endif
        pb1_released_time = pb1_edge_time;
        pb1_stable = pb1_last;
        pb1_released = true;
        pb1_pressed = pb1_short_pressed = pb1_long_pressed = false;
        long pb1_press_duration = pb1_released_time - pb1_pressed_time;
        if (pb1_press_duration > LONG_PRESS_TIME) {
          pb1_short_released = false;
          pb1_long_released = true;
          #if DEBUG2_PRINT
          Serial.println("Info: button_check() >>> Long Release triggered");
          #endif
        } else if (pb1_press_duration > SHORT_PRESS_TIME) {
          pb1_short_released = true;
          pb1_long_released = false;
          #if DEBUG2_PRINT
          Serial.println("Info: button_check() >>> Short Release triggered");
          #endif
        }
      }
    }
  }
}

void clock_time()
{
  if ((micros() - clock_timer) >= 1000000) {
    clock_timer = micros();
    time_seconds++;
    if (time_seconds >= 60) {
      time_seconds = 0;
      time_minutes ++;

      if (time_minutes >= 60) {
        time_minutes = 0;
        time_hours++;

        if (time_hours >= 24) {
          time_hours = 0;
        }
      }
    }

    // Format for display HH:MM (24 hour format)
    sprintf(time_disp, "%2.2d:%2.2dZ", time_hours, time_minutes);
  }
}

void toggleRemoteLog() {
  g_remote_log = !g_remote_log;
}

void displayOff() {
  display_on = false;
  ledcWrite(PIN_LCD_BL, 0);
  tft.writecommand(ST7789_DISPOFF);
  tft.writecommand(ST7789_SLPIN);
  delay(120);
}

void displayOn() {
  display_on = true;
  tft.writecommand(ST7789_SLPOUT);
  delay(120);
  tft.writecommand(ST7789_DISPON);
  ledcWrite(PIN_LCD_BL, currentBrt);
  drawSprite();
}

void captureScreen() {
  uint16_t width = spr.width(), height = spr.height();
  char sb[9];
  Serial.println("");
  // 14 bytes of BMP header
  Serial.print("424d"); // BM
  sprintf(sb, "%08x", (unsigned int)htonl(14 + 40 + 12 + width * height * 2)); // Image size
  Serial.print(sb);
  Serial.print("00000000");
  sprintf(sb, "%08x", (unsigned int)htonl(14 + 40 + 12)); // Offset to image data
  Serial.print(sb);

  //Image header
  Serial.print("28000000"); // Header size
  sprintf(sb, "%08x", (unsigned int)htonl(width));
  Serial.print(sb);
  sprintf(sb, "%08x", (unsigned int)htonl(height));
  Serial.print(sb);
  Serial.print("01001000"); // 1 plane, 16 bpp
  Serial.print("00000000"); // Compression
  Serial.print("00000000"); // Compressed image size
  Serial.print("00000000"); // X res
  Serial.print("00000000"); // Y res
  Serial.print("00000000"); // Color map
  Serial.print("00000000"); // Colors
  Serial.print("00f80000"); // Red mask
  Serial.print("e0070000"); // Green mask
  Serial.println("1f000000"); // Blue mask

  // Image data
  for (int y=height-1; y>=0; y--) {
    for (int x=0; x<width; x++) {
      sprintf(sb, "%04x", htons(spr.readPixel(x, y)));
      Serial.print(sb);
    }
    Serial.println("");
  }
}

#if THEME_EDITOR
char readSerialWithEcho() {
  char key;
  while (Serial.available() == 0) {};
  key = Serial.read();
  Serial.print(key);
  return key;
}

uint8_t char2nibble(char key) {
  if (key < '0') return 0;
  if (key <= '9') return key - '0';
  if (key < 'A') return 0;
  if (key <= 'F') return key - 'A' + 10;
  if (key < 'a') return 0;
  if (key <= 'f') return key - 'a' + 10;
}

void setColorTheme() {
  Serial.print("Enter a string of hex colors (x0001x0002...): ");
  int i = 0;
  char key;
  while(true) {
    if (i >= (sizeof(ColorTheme) - offsetof(ColorTheme, bg))) {
      Serial.println(" Ok");
      break;
    }
    key = readSerialWithEcho();
    if (key != 'x') {
      Serial.println(" Err");
      break;
    }

    key = readSerialWithEcho();
    ((char *) &theme[themeIdx])[offsetof(ColorTheme, bg) + i + 1] = char2nibble(key) * 16;
    key = readSerialWithEcho();
    ((char *) &theme[themeIdx])[offsetof(ColorTheme, bg) + i + 1] |= char2nibble(key);

    key = readSerialWithEcho();
    ((char *) &theme[themeIdx])[offsetof(ColorTheme, bg) + i] = char2nibble(key) * 16;
    key = readSerialWithEcho();
    ((char *) &theme[themeIdx])[offsetof(ColorTheme, bg) + i] |= char2nibble(key);

    i += sizeof(uint16_t);
  }
  drawSprite();
}


void getColorTheme() {
  char sb[6];
  Serial.print("Color theme ");
  Serial.print(theme[themeIdx].name);
  Serial.print(": ");
  for (int i=0; i<(sizeof(ColorTheme) - offsetof(ColorTheme, bg)); i += sizeof(uint16_t)) {
    sprintf(sb, "x%02X%02X", ((char *) &theme[themeIdx])[offsetof(ColorTheme, bg) + i + 1], ((char *) &theme[themeIdx])[offsetof(ColorTheme, bg) + i]);
    Serial.print(sb);
  }
  Serial.println();
}
#endif

// Function to synchronize internal clock with RDS data
void syncTimeFromRDS(char *rdsTimeStr)
{
  if (!rdsTimeStr) return;
  
  // The standard RDS time format is “HH:MM”.
  // or sometimes more complex like “DD.MM.YY,HH:MM”.
  char *timeField = strstr(rdsTimeStr, ":");
  
  // If we find a valid time format
  if (timeField && (timeField >= rdsTimeStr + 2)) {
    char hourStr[3] = {0};
    char minStr[3] = {0};
    
    // Extract hours and minutes
    hourStr[0] = *(timeField - 2);
    hourStr[1] = *(timeField - 1);
    minStr[0] = *(timeField + 1);
    minStr[1] = *(timeField + 2);
    
    // Convert to numbers
    int hours = atoi(hourStr);
    int mins = atoi(minStr);
    
    // Check validity of values
    if (hours >= 0 && hours < 24 && mins >= 0 && mins < 60) {
      // Update internal clock
      time_hours = hours;
      time_minutes = mins;
      time_seconds = 0; // Reset seconds for greater precision
      
      // Update display
      sprintf(time_disp, "%02d:%02dZ", time_hours, time_minutes);
      
      time_synchronized = true;

      #if DEBUG1_PRINT
      Serial.print("Info: syncTimeFromRDS() >>> Synchronized clock: ");
      Serial.println(time_disp);
      #endif
    }
  }
}

/**
 * Main loop
 */
void loop() {
  // Check if the encoder has moved.
  if (encoderCount != 0 && !display_on) {
    encoderCount = 0;
  } else if (encoderCount != 0 && pb1_pressed  && !isModalMode()) {
    if (isSSB()) {
#if TUNE_HOLDOFF
      // Tuning timer to hold off (FM/AM) display updates
      tuning_flag = true;
      tuning_timer = millis();
      #if DEBUG3_PRINT
      Serial.print("Info: TUNE_HOLDOFF SSB (Set) >>> ");
      Serial.print("tuning_flag = ");
      Serial.print(tuning_flag);
      Serial.print(", millis = ");
      Serial.println(millis());
      #endif
#endif
      doFrequencyTuneSSB(true);
    } else {
      seekDirection = (encoderCount == 1) ? 1 : 0;
      seekStop = false; // G8PTN: Flag is set by rotary encoder and cleared on seek entry
      doSeek();
      band[bandIdx].currentFreq = currentFrequency;            // G8PTN: Added to ensure update of currentFreq in table for AM/FM
    }
    encoderCount = 0;
    seekModePress = true;
    resetEepromDelay();
    elapsedSleep = elapsedCommand = millis();
  } else if (encoderCount != 0) {
    // G8PTN: The manual BFO adjusment is not required with the doFrequencyTuneSSB method, but leave for debug
    if (bfoOn & isSSB()) {
      currentBFO = (encoderCount == 1) ? (currentBFO + currentBFOStep) : (currentBFO - currentBFOStep);
      // G8PTN: Clamp range to +/- BFOMax (as per doFrequencyTuneSSB)
      if (currentBFO >  BFOMax) currentBFO =  BFOMax;
      if (currentBFO < -BFOMax) currentBFO = -BFOMax;
      band[bandIdx].currentFreq = currentFrequency + (currentBFO / 1000);     // G8PTN; Calculate frequency value to store in EEPROM
      updateBFO();
      showBFO();
    }
    else if (cmdMenu)
      doMenu(encoderCount);
    else if (cmdMode)
      doMode(encoderCount);
    else if (cmdStep)
      doStep(encoderCount);
    else if (cmdAgc)
      doAgc(encoderCount);
    else if (cmdBandwidth)
      doBandwidth(encoderCount);
    else if (cmdVolume)
      doVolume(encoderCount);
    else if (cmdSoftMuteMaxAtt)
      doSoftMute(encoderCount);
    else if (cmdBand)
      setBand(encoderCount);
    else if (cmdCal)
      doCal(encoderCount);
    else if (cmdAvc)
      doAvc(encoderCount);
    else if (cmdSettings)
      doSettings(encoderCount);
    else if (cmdBrt)
      doBrt(encoderCount);
    else if (cmdSleep)
      doSleep(encoderCount);
    else if (cmdTheme)
      doTheme(encoderCount);
    else if (cmdAbout) {}
    // G8PTN: Added SSB tuning
    else if (isSSB()) {

#if TUNE_HOLDOFF
      // Tuning timer to hold off (SSB) display updates
      tuning_flag = true;
      tuning_timer = millis();
      #if DEBUG3_PRINT
      Serial.print("Info: TUNE_HOLDOFF SSB (Set) >>> ");
      Serial.print("tuning_flag = ");
      Serial.print(tuning_flag);
      Serial.print(", millis = ");
      Serial.println(millis());
      #endif
#endif

      doFrequencyTuneSSB();

      // Debug
      #if DEBUG1_PRINT
      Serial.print("Info: SSB >>> ");
      Serial.print("currentFrequency = ");
      Serial.print(currentFrequency);
      Serial.print(", currentBFO = ");
      Serial.print(currentBFO);
      Serial.print(", rx.setSSBbfo() = ");
      Serial.println((currentBFO + currentCAL) * -1);
      #endif
    } else {

#if TUNE_HOLDOFF
      // Tuning timer to hold off (FM/AM) display updates
      tuning_flag = true;
      tuning_timer = millis();
      #if DEBUG3_PRINT
      Serial.print("Info: TUNE_HOLDOFF FM/AM (Set) >>> ");
      Serial.print("tuning_flag = ");
      Serial.print(tuning_flag);
      Serial.print(", millis = ");
      Serial.println(millis());
      #endif
#endif

      // G8PTN: Used in place of rx.frequencyUp() and rx.frequencyDown()
      uint16_t step = currentMode == FM ? tabFmStep[currentStepIdx] : tabAmStep[currentStepIdx]; 
      uint16_t stepAdjust = currentFrequency % step;
      step = !stepAdjust? step : encoderCount>0? step - stepAdjust : stepAdjust;
      currentFrequency += step * encoderCount;

      // Band limit checking
      uint16_t bMin = band[bandIdx].minimumFreq;                            // Assign lower band limit
      uint16_t bMax = band[bandIdx].maximumFreq;                            // Assign upper band limit

      // Special case to cover AM frequency negative!
      bool AmFreqNeg = false;
      if ((currentMode == AM) && (currentFrequency & 0x8000))
        AmFreqNeg = true;

      // Priority to minimum check to cover AM frequency negative
      if ((currentFrequency < bMin) || AmFreqNeg)
        currentFrequency = bMax;                                           // Lower band limit or AM frequency negative
      else if (currentFrequency > bMax)
        currentFrequency = bMin;                                           // Upper band limit

      rx.setFrequency(currentFrequency);                                   // Set new frequency

      if (currentMode == FM) cleanBfoRdsInfo();

      if (isCB()) checkCBChannel();

      // Show the current frequency only if it has changed
      currentFrequency = rx.getFrequency();
      band[bandIdx].currentFreq = currentFrequency;            // G8PTN: Added to ensure update of currentFreq in table for AM/FM

      // Debug
      #if DEBUG1_PRINT
      Serial.print("Info: AM/FM >>> currentFrequency = ");
      Serial.print(currentFrequency);
      Serial.print(", currentBFO = ");
      Serial.println(currentBFO);                              // Print to check the currentBFO value
      //Serial.print(", rx.setSSBbfo() = ");                   // rx.setSSBbfo() will not have been written
      //Serial.println((currentBFO + currentCAL) * -1);        // rx.setSSBbfo() will not have been written
      #endif
    }

    encoderCount = 0;
    resetEepromDelay();
    elapsedSleep = elapsedCommand = millis();
  } else {
    if (pb1_long_pressed && !seekModePress) {
      pb1_long_pressed = pb1_short_pressed = pb1_pressed = false;
      if (display_on) {
        displayOff();
      } else {
        displayOn();
      }
      elapsedSleep = millis();
    } else if (pb1_short_released && display_on && !seekModePress) {
      pb1_released = pb1_short_released = pb1_long_released = false;
      if (muted) {
        rx.setVolume(mute_vol_val);
        muted = false;
      }
      disableCommands();
      cmdVolume = true;
      menuIdx = MENU_VOLUME;
      showVolume();
      delay(MIN_ELAPSED_TIME); // waits a little more for releasing the button.
      elapsedSleep = elapsedCommand = millis();
   } else if (pb1_released && !pb1_long_released && !seekModePress) {
      pb1_released = pb1_short_released = pb1_long_released = false;
      if (!display_on) {
        if (currentSleep) {
          displayOn();
        }
      } else if (cmdMenu) {
        currentMenuCmd = menuIdx;
        doCurrentMenuCmd();
      } else if (cmdSettings) {
        currentSettingsMenuCmd = settingsMenuIdx;
        doCurrentSettingsMenuCmd();
      } else {
        if (isModalMode()) {
          disableCommands();
          showStatus();
          showCommandStatus((char *)"VFO ");
        } else if (bfoOn) {
          bfoOn = false;
          showStatus();
        } else {
          cmdMenu = !cmdMenu;
          // menuIdx = MENU_VOLUME;
          currentMenuCmd = menuIdx;
          // settingsMenuIdx = MENU_BRIGHTNESS;
          currentSettingsMenuCmd = settingsMenuIdx;
          drawSprite();
        }
      }
      delay(MIN_ELAPSED_TIME); // waits a little more for releasing the button.
      elapsedSleep = elapsedCommand = millis();
    }
  }

  // Display sleep timeout
  if (currentSleep && display_on) {
    if ((millis() - elapsedSleep) > currentSleep * 1000) {
      displayOff();
    }
  }

  // Show RSSI status only if this condition has changed
  if ((millis() - elapsedRSSI) > MIN_ELAPSED_RSSI_TIME * 6)
  {
    // Debug
    #if DEBUG3_PRINT
    Serial.println("Info: loop() >>> Checking signal information");
    #endif

    rx.getCurrentReceivedSignalQuality();
    snr= rx.getCurrentSNR();
    int aux = rx.getCurrentRSSI();

    // Debug
    #if DEBUG3_PRINT
    Serial.print("Info: loop() >>> RSSI = ");
    Serial.println(rssi);
    #endif

    if (rssi != aux)                            // G8PTN: Based on 1.2s update, always allow S-Meter
    {
      // Debug
      #if DEBUG3_PRINT
      Serial.println("Info: loop() >>> RSI diff detected");
      #endif

      rssi = aux;
      showRSSI();
    }
    elapsedRSSI = millis();
  }

  // Disable commands control
  if ((millis() - elapsedCommand) > ELAPSED_COMMAND)
  {
    if (isSSB())
    {
      bfoOn = false;
      // showBFO();
      disableCommands();
      showStatus();
    }
    else if (isModalMode()) {
      disableCommands();
      showStatus();
    }
    elapsedCommand = millis();
  }

  if ((millis() - lastRDSCheck) > RDS_CHECK_TIME) {
    if ((currentMode == FM) and (snr >= 12)) checkRDS();
    lastRDSCheck = millis();
  }

  // Save the current frequency only if it has changed
  if (itIsTimeToSave) {
    if ((millis() - storeTime) > STORE_TIME) {
      saveAllReceiverInformation();
      storeTime = millis();
      itIsTimeToSave = false;
    }
  }

  // Check for button activity
  buttonCheck();
  if (!pb1_pressed && seekModePress) {
    seekModePress = false;
    pb1_released = pb1_short_released = pb1_long_released = false;
  }

  // Periodically refresh the main screen
  // This covers the case where there is nothing else triggering a refresh
  if ((millis() - background_timer) > BACKGROUND_REFRESH_TIME) {
    background_timer = millis();
    if (!isModalMode()) showStatus();
  }

#if TUNE_HOLDOFF
  // Check if tuning flag is set
  if (tuning_flag == true) {
    if ((millis() - tuning_timer) > TUNE_HOLDOFF_TIME) {
      tuning_flag = false;
      showFrequency();
      #if DEBUG3_PRINT
      Serial.print("Info: TUNE_HOLDOFF (Reset) >>> ");
      Serial.print("tuning_flag = ");
      Serial.print(tuning_flag);
      Serial.print(", millis = ");
      Serial.println(millis());
      #endif
    }
  }
#endif

  // Run clock
  clock_time();


#if USE_REMOTE
  // REMOTE Serial - Experimental

  if (millis() - g_remote_timer >= 500 && g_remote_log)
  {
    g_remote_timer = millis();

    // Increment diagnostic sequence number
    g_remote_seqnum ++;

    // Prepare information ready to be sent
    int remote_volume  = rx.getVolume();

    // S-Meter conditional on compile option
    rx.getCurrentReceivedSignalQuality();
    uint8_t remote_rssi = rx.getCurrentRSSI();

    // Use rx.getFrequency to force read of capacitor value from SI4732/5
    rx.getFrequency();
    uint16_t tuning_capacitor = rx.getAntennaTuningCapacitor();

    // Remote serial
    Serial.print(app_ver);                      // Firmware version
    Serial.print(",");

    Serial.print(currentFrequency);             // Frequency (KHz)
    Serial.print(",");
    Serial.print(currentBFO);                   // Frequency (Hz x 1000)
    Serial.print(",");

    Serial.print(band[bandIdx].bandName);      // Band
    Serial.print(",");
    Serial.print(currentMode);                  // Mode
    Serial.print(",");
    Serial.print(currentStepIdx);               // Step (FM/AM/SSB)
    Serial.print(",");
    Serial.print(bwIdxFM);                      // Bandwidth (FM)
    Serial.print(",");
    Serial.print(bwIdxAM);                      // Bandwidth (AM)
    Serial.print(",");
    Serial.print(bwIdxSSB);                     // Bandwidth (SSB)
    Serial.print(",");
    Serial.print(agcIdx);                       // AGC/ATTN (FM/AM/SSB)
    Serial.print(",");

    Serial.print(remote_volume);                // Volume
    Serial.print(",");
    Serial.print(remote_rssi);                  // RSSI
    Serial.print(",");
    Serial.print(tuning_capacitor);             // Tuning cappacitor
    Serial.print(",");
    Serial.print(adc_read_avr);                 // V_BAT/2 (ADC average value)
    Serial.print(",");
    Serial.println(g_remote_seqnum);            // Sequence number
  }

  if (Serial.available() > 0)
  {
    char key = Serial.read();
    switch (key)
    {
        case 'U':                              // Encoder Up
          encoderCount ++;
          break;
        case 'D':                              // Encoder Down
          encoderCount --;
          break;
        case 'P':                              // Encoder Push Button
          pb1_released = true;
          break;

        case 'B':                              // Band Up
          setBand(1);
          break;
        case 'b':                              // Band Down
          setBand(-1);
          break;

        case 'M':                              // Mode Up
          doMode(1);
          break;
        case 'm':                              // Mode Down
          doMode(-1);
          break;

        case 'S':                              // Step Up
          doStep(1);
          break;
        case 's':                              // Step Down
          doStep(-1);
          break;

        case 'W':                              // Bandwidth Up
          doBandwidth(1);
          break;
        case 'w':                              // Bandwidth Down
          doBandwidth(-1);
          break;

        case 'A':                              // AGC/ATTN Up
          doAgc(1);
          break;
        case 'a':                              // AGC/ATTN Down
          doAgc(-1);
          break;

        case 'V':                              // Volume Up
          doVolume(1);
          break;
        case 'v':                              // Volume Down
          doVolume(-1);
          break;

        case 'L':                              // Backlight Up
          doBrt(1);
          break;
        case 'l':                              // Backlight Down
          doBrt(-1);
          break;

        case 'O':
          displayOff();
          break;

        case 'o':
          displayOn();
          break;

        case 'C':
          captureScreen();
          break;

        case 't':
          toggleRemoteLog();
          break;

#if THEME_EDITOR
        case '!':
          setColorTheme();
          break;

        case '@':
          getColorTheme();
          break;
#endif

        default:
          break;
    }
  }
#endif

  // Add a small default delay in the main loop
  delay(5);
}
