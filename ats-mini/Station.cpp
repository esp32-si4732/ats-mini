#include "Common.h"

#define MIN_CB_FREQUENCY 26060
#define MAX_CB_FREQUENCY 29665

static const char *cbChannelNumber[] =
{
  "1",  "2",  "3",  "41",
  "4",  "5",  "6",  "7",  "42",
  "8",  "9",  "10", "11", "43",
  "12", "13", "14", "15", "44",
  "16", "17", "18", "19", "45",
  "20", "21", "22", "23",
  "24", "25", "26", "27",
  "28", "29", "30", "31",
  "32", "33", "34", "35",
  "36", "37", "38", "39",
  "40",
};

static char bufStationName[50]  = "";
static char bufStationInfo[100] = "";
static char bufProgramInfo[100] = "";
static uint16_t piCode = 0x0000;

const char *getStationName()
{
#ifdef THEME_EDITOR
  return("*STATION*");
#else
  return(bufStationName);
#endif
}

const char *getStationInfo()
{
  return(bufStationInfo);
}

const char *getProgramInfo()
{
  return(bufProgramInfo);
}

uint16_t getRdsPiCode()
{
  return(piCode);
}

void clearStationInfo()
{
  bufStationName[0] = '\0';
  bufStationInfo[0] = '\0';
  bufProgramInfo[0] = '\0';
  piCode = 0x0000;
}

static bool showStationName(const char *stationName)
{
  if(stationName && strcmp(bufStationName, stationName))
  {
    strcpy(bufStationName, stationName);
    return(true);
  }

  return(false);
}

static bool showStationInfo(const char *stationInfo)
{
  if(stationInfo && strcmp(bufStationInfo, stationInfo))
  {
    strcpy(bufStationInfo, stationInfo);
    return(true);
  }

  return(false);
}

static bool showProgramInfo(const char *programInfo)
{
  if(programInfo && strcmp(bufProgramInfo, programInfo))
  {
    strcpy(bufProgramInfo, programInfo);
    return(true);
  }

  return(false);
}

static bool showRdsPiCode(uint16_t rdsPiCode)
{
  if(rdsPiCode!=piCode)
  {
    piCode = rdsPiCode;
    return(true);
  }

  return(false);
}

static bool showRdsTime(const char *rdsTime)
{
  if(!rdsTime) return(false);

  // The standard RDS time format is “HH:MM”.
  // or sometimes more complex like “DD.MM.YY,HH:MM”.
  const char *timeField = strstr(rdsTime, ":");

  // If we find a valid time format...
  if(timeField && (timeField>=rdsTime+2) && timeField[1] && timeField[2])
  {
    // Extract hours and minutes
    int hours = (timeField[-2] - '0') * 10 + timeField[-1] - '0';
    int mins  = (timeField[1] - '0') * 10 + timeField[2] - '0';

    // If hours and minutes are valid, update clock
    if(hours>=0 && hours<24 && mins>=0 && mins<60)
      return(clockSet(hours, mins));
  }

  // No time
  return(false);
}

bool checkRds()
{
  bool needRedraw = false;

  rx.getRdsStatus();

  if(rx.getRdsReceived() && rx.getRdsSync() && rx.getRdsSyncFound())
  {
    needRedraw |= showStationName(rx.getRdsStationName());
    needRedraw |= showStationInfo(rx.getRdsStationInformation());
    needRedraw |= showProgramInfo(rx.getRdsProgramInformation());
    needRedraw |= showRdsPiCode(rx.getRdsPI());
    needRedraw |= showRdsTime(rx.getRdsTime());
  }

  // Return TRUE if any RDS information changes
  return(needRedraw);
}

bool checkCbChannel()
{
  const int column_step = 450; // In kHz
  const int row_step    = 10;
  const int max_columns = 8;   // A-H
  const int max_rows    = 45;

  if(currentFrequency<MIN_CB_FREQUENCY || currentFrequency>MAX_CB_FREQUENCY)
    return(showStationName(""));

  int offset = currentFrequency - MIN_CB_FREQUENCY;
  char type  = 'R';
  if((offset%10) == 5)
  {
    type = 'E';
    offset -= 5;
  }

  int column_index = offset / column_step;
  int remainder    = offset % column_step;
  if((column_index>=max_columns) || (remainder%row_step))
    return(showStationName(""));

  int row_number = remainder / row_step;
  if((row_number>=max_rows) || (row_number<0))
    return(showStationName(""));

  char buf[50];
  sprintf(buf, "%c%s%c", 'A' + column_index, cbChannelNumber[row_number], type);
  return(showStationName(buf));
}
