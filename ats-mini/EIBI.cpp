#include "Common.h"
#include "Draw.h"
#include "EIBI.h"
#include "Button.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <FS.h>

#include <ctype.h>
#include <string.h>

#define EIBI_PATH "/schedules.bin"
#define TEMP_PATH "/schedules.tmp"
#ifndef EIBI_URL
#define EIBI_URL  "http://eibispace.de/dx/eibi.txt"
#endif

extern ButtonTracker pb1;

const BandLabel bandLabels[] =
{
  {  472,   479,  "630m (CW)"     },
  {  500,   518,  "NAVTEX"        },
  {  525,  1705,  "MW Broadcast"  },
  { 1600,  1800,  "Top Band"      },
  { 2300,  2495,  "120m BC"       },
  { 3200,  3400,  "90m BC"        },
  { 3500,  3800,  "80m (Amateur)" },
  { 3700,  3800,  "80m (SSB)"     },
  { 3900,  4000,  "75m BC"        },
  { 4720,  4750,  "60m (Amateur)" },
  { 4750,  4995,  "60m BC"        },
  { 5900,  6200,  "49m BC"        },
  { 7000,  7040,  "40m (CW)"      },
  { 7040,  7100,  "40m (DIGI)"    },
  { 7100,  7200,  "40m (SSB)"     },
  { 7200,  7600,  "41m BC"        },
  { 9400,  9900,  "31m BC"        },
  {10100, 10140,  "30m (CW)"      },
  {10136, 10140,  "30m (FT8)"     },
  {10140, 10150,  "30m (DIGI)"    },
  {11600, 12100,  "25m BC"        },
  {14000, 14070,  "20m (CW)"      },
  {14070, 14100,  "20m (DIGI/FT8)"},
  {14100, 14350,  "20m (SSB)"     },
  {15100, 15800,  "19m BC"        },
  {18068, 18110,  "17m (CW)"      },
  {18100, 18110,  "17m (FT8)"     },
  {18110, 18168,  "17m (SSB)"     },
  {17480, 17900,  "16m BC"        },
  {21000, 21070,  "15m (CW)"      },
  {21070, 21100,  "15m (DIGI/FT8)"},
  {21100, 21450,  "15m (SSB)"     },
  {21450, 21850,  "13m BC"        },
  {24890, 24915,  "12m (CW)"      },
  {24915, 24925,  "12m (FT8)"     },
  {24925, 24990,  "12m (SSB)"     },
  {26960, 27410,  "11m (CB)"      },
  {25670, 26100,  "11m BC"        },
  {28000, 28120,  "10m (CW)"      },
  {28120, 28190,  "10m (DIGI/FT8)"},
  {28200, 29700,  "10m (SSB/FM)"  },
  {26100, 26500,  "11m BC"        },
  {29600, 30000,  "9m BC"         }
};

// FIXME: this might be slow
bool eibiAvailable()
{
  return(LittleFS.exists(EIBI_PATH));
}

static bool entryIsNow(const StationSchedule *entry, int now)
{
  // Check if entry applies to all hours
  if(entry->start_h < 0 || entry->end_h < 0) return(true);

  // These are starting/ending times in minutes
  int start = entry->start_h * 60 + entry->start_m;
  int end   = entry->end_h * 60 + entry->end_m;

  // Check for inclusive schedule
  if(start <= end && now >= start && now <= end) return(true);

  // Check for exclusive schedule
  if(start > end && (now >= start || now <= end)) return(true);

  // Nope
  return(false);
}

const StationSchedule *eibiNext(uint16_t freq, uint8_t hour, uint8_t minute, size_t *offset)
{
  // Will return this static entry
  static StationSchedule entry;

  // Must have valid offset
  if(!offset) return(NULL);

  // If no valid offset yet, find some
  if(*offset==(size_t)-1) eibiLookup(freq, hour, minute, offset);

  // Open file with EIBI data
  fs::File file = LittleFS.open(EIBI_PATH, "rb");
  if(!file) return(NULL);

  StationSchedule *result = NULL;
  int now = hour * 60 + minute;

  // Go to the starting offset
  if(!file.seek(*offset, fs::SeekSet))
  {
    file.close();
    return(NULL);
  }

  while(file.read((uint8_t*)&entry, sizeof(entry)) == sizeof(entry))
  {
    if((entry.freq>freq) && entryIsNow(&entry, now))
    {
      *offset = file.position() - sizeof(entry);
      result = &entry;
      break;
    }
  }

  file.close();
  return(result);
}

const StationSchedule *eibiPrev(uint16_t freq, uint8_t hour, uint8_t minute, size_t *offset)
{
  // Will return this static entry
  static StationSchedule entry;

  // Must have valid offset
  if(!offset) return(NULL);

  // If no valid offset yet, find some
  if(*offset==(size_t)-1) eibiLookup(freq, hour, minute, offset);

  // Open file with EIBI data
  fs::File file = LittleFS.open(EIBI_PATH, "rb");
  if(!file) return(NULL);

  StationSchedule *result = NULL;
  int now = hour * 60 + minute;

  for(size_t pos = *offset; file.seek(pos, fs::SeekSet) ; pos -= sizeof(entry))
  {
    if(file.read((uint8_t*)&entry, sizeof(entry)) != sizeof(entry)) break;

    if((entry.freq<freq) && entryIsNow(&entry, now))
    {
      *offset = pos;
      result = &entry;
      break;
    }
  }

  file.close();
  return(result);
}

const StationSchedule *eibiAtSameFreq(uint8_t hour, uint8_t minute, size_t *offset, bool same)
{
  // Will return this static entry
  static StationSchedule entry;

  // Must have valid offset
  if(!offset) return(NULL);

  // Open file with EIBI data
  fs::File file = LittleFS.open(EIBI_PATH, "rb");
  if(!file) return(NULL);

  // Read current entry to get frequency
  StationSchedule e0;
  if(!file.seek(*offset, fs::SeekSet) || file.read((uint8_t*)&e0, sizeof(e0)) != sizeof(e0))
  {
    file.close();
    return(NULL);
  }

  StationSchedule *result = NULL;
  int now = hour * 60 + minute;

  if(same && entryIsNow(&e0, now))
  {
    entry = e0;
    result = &entry;
    file.close();
    return(result);
  }

  while(file.read((uint8_t*)&entry, sizeof(entry)) == sizeof(entry))
  {
    if(entry.freq != e0.freq)
      break;
    else if(entryIsNow(&entry, now))
    {
      *offset = file.position() - sizeof(entry);
      result = &entry;
      break;
    }
  }

  file.close();
  return(result);
}

const StationSchedule *eibiLookup(uint16_t freq, uint8_t hour, uint8_t minute, size_t *offset)
{
  // Will return this static entry
  static StationSchedule entry;

  // Open file with EIBI data
  fs::File file = LittleFS.open(EIBI_PATH, "rb");
  if(!file) return(NULL);

  // Set up binary search
  ssize_t total = file.size() / sizeof(entry);
  ssize_t left  = 0;
  ssize_t right = total;
  ssize_t match = -1;
  ssize_t mid;

  // Search for the frequency
  while(left <= right)
  {
    // Go to the middle entry
    mid = (left + right) / 2;

    // Save current offset, correcting for file size
    if(offset) *offset = (mid<0? 0 : mid>=total? total-1 : mid) * sizeof(entry);

    // Go to the middle entry
    if(!file.seek(mid * sizeof(entry), fs::SeekSet))
    {
      file.close();
      return(NULL);
    }

    // Read middle entry
    if(file.read((uint8_t*)&entry, sizeof(entry)) != sizeof(entry))
    {
      file.close();
      return(NULL);
    }

    // Compare frequency
    if(entry.freq < freq)
      left = mid + 1;
    else if(entry.freq > freq)
      right = mid - 1;
    else
    {
      match = mid;
      right = mid - 1;
    }
  }

  // Drop out if not found
  if(match < 0)
  {
    file.close();
    return(NULL);
  }

  // If found entry is not the same as the last read entry...
  if(match != mid)
  {
    // Read the right entry
    if(file.seek(match * sizeof(entry), fs::SeekSet))
      if(file.read((uint8_t*)&entry, sizeof(entry)) == sizeof(entry))
        mid = match;

    // Drop out if failed to read
    if(match != mid)
    {
      file.close();
      return(NULL);
    }
  }

  // This is our current time in minutes
  int now = hour * 60 + minute;

  // Keep reading entries from file
  do
  {
    // Report offset within the file
    if(offset) *offset = file.position() - sizeof(entry);

    // Match frequency
    if(entry.freq != freq) break;

    // Match time
    if(entryIsNow(&entry, now))
    {
      file.close();
      return(&entry);
    }
  }
  while(file.read((uint8_t*)&entry, sizeof(entry)) == sizeof(entry));

  // Not found
  file.close();
  return(NULL);
}

char replace_accented_char(char c)
{
  switch((unsigned char)c)
  {
    // Lowercase vowels with accents
    case 0xE1: case 0xE0: case 0xE2: case 0xE3: case 0xE4: return 'a'; // á, à, â, ã, ä
    case 0xE9: case 0xE8: case 0xEA: case 0xEB: return 'e';             // é, è, ê, ë
    case 0xED: case 0xEC: case 0xEE: case 0xEF: return 'i';            // í, ì, î, ï
    case 0xF3: case 0xF2: case 0xF4: case 0xF5: case 0xF6: return 'o';  // ó, ò, ô, õ, ö
    case 0xFA: case 0xF9: case 0xFB: case 0xFC: return 'u';             // ú, ù, û, ü
    // Uppercase vowels with accents
    case 0xC1: case 0xC0: case 0xC2: case 0xC3: case 0xC4: return 'A';  // Á, À, Â, Ã, Ä
    case 0xC9: case 0xC8: case 0xCA: case 0xCB: return 'E';             // É, È, Ê, Ë
    case 0xCD: case 0xCC: case 0xCE: case 0xCF: return 'I';             // Í, Ì, Î, Ï
    case 0xD3: case 0xD2: case 0xD4: case 0xD5: case 0xD6: return 'O';  // Ó, Ò, Ô, Õ, Ö
    case 0xDA: case 0xD9: case 0xDB: case 0xDC: return 'U';             // Ú, Ù, Û, Ü
    // Other special chars
    case 0xF1: return 'n';  // ñ
    case 0xD1: return 'N';  // Ñ
    case 0xE7: return 'c';  // ç
    case 0xC7: return 'C';  // Ç
    default: return c;      // No change
  }
}

static bool eibiParseLine(const char *line, StationSchedule &entry)
{
  char nameStr[sizeof(entry.name) + 1];
  char freqStr[15] = {0};
  char timeStr[10] = {0};
  char tmpCol[12]  = {0};
  char *p, *t;

  // Scan line for data
  if(sscanf(line, "%14c%9c%11c%24c", freqStr, timeStr, tmpCol, nameStr)<3)
    return(false);

  // Terminate found data
  freqStr[14] = '\0';
  timeStr[9] = '\0';
  nameStr[24] = '\0';

  // Parse frequency
  entry.freq = (uint16_t)atof(freqStr);
  if(!entry.freq) return(false);

  // Parse time
  int sh, sm, eh, em;
  if(sscanf(timeStr, "%2d%2d-%2d%2d", &sh, &sm, &eh, &em) != 4) return(false);
  entry.start_h = sh;
  entry.start_m = sm;
  entry.end_h   = eh;
  entry.end_m   = em;

  // Remove jammers
  if(strstr(nameStr, "Jammer")) return(false);

  // Remove leading and trailing white space from name
  for(p = nameStr ; *p==' ' || *p=='\t' ; ++p);
  for(t = p + strlen(p) - 1 ; t>=p && (*t==' ' || *t=='\t') ; *t--='\0');

  // Replace accented characters
  for (t = p; *t != '\0'; t++) {
    *t = replace_accented_char(*t);
  }

  // Copy name
  strncpy(entry.name, p, sizeof(entry.name) - 1);
  entry.name[sizeof(entry.name)-1] = '\0';

  // Done
  return(true);
}

bool eibiLoadSchedule()
{
  static const char *eibiMessage = "Loading EiBi Schedule";
  HTTPClient http;

  // Need to be connected to the network
  if(getWiFiStatus() < 2) return(false);

  drawScreen(eibiMessage, "Connecting...");

  // Open HTTP connection to EiBi site
  http.begin(EIBI_URL);
  if(http.GET() != HTTP_CODE_OK)
  {
    drawScreen(eibiMessage, "Failed connecting to EiBi!");
    http.end();
    return(false);
  }

  // Open file in the local flash file system
  fs::File file = LittleFS.open(TEMP_PATH, "wb");
  if(!file)
  {
    drawScreen(eibiMessage, "Failed opening local storage!");
    http.end();
    return(false);
  }

  // Start loading data
  WiFiClient *stream = http.getStreamPtr();
  int totalLen = http.getSize();
  int byteCnt, lineCnt, charCnt;
  char charBuf[200];

  for(byteCnt = charCnt = lineCnt = 0 ; http.connected() && (totalLen<0 || byteCnt<totalLen) ; )
  {
    if(consumeAbortPending())
    {
      file.close();
      http.end();
      LittleFS.remove(TEMP_PATH);
      drawScreen(eibiMessage, "CANCELED!");
      return(false);
    }

    if(!stream->available()) delay(1);
    else
    {
      char c = stream->read();
      byteCnt++;

      if(c!='\n' && charCnt<sizeof(charBuf)-1) charBuf[charCnt++] = c;
      else
      {
        char *p, *t;

        // Remove whitespace
        charBuf[charCnt] = '\0';
        for(p = charBuf ; *p && *p<=' ' ; ++p);
        for(t = charBuf + charCnt - 1 ; t>=p && *t<=' ' ; *t--='\0');

        // If valid non-empty schedule line...
        if(t-p+1>0 && isdigit(*p))
        {
          // Remove LFs
          for(t = p ; *t ; ++t)
            if(*t=='\r') *t = ' ';

          // If parsed a new entry...
          StationSchedule entry;
          if(eibiParseLine(p, entry))
          {
            // Write it to the output file
            file.write((uint8_t*)&entry, sizeof(entry));
            lineCnt++;

            if(!(lineCnt & 31))
            {
              char statusMessage[64];
              sprintf(statusMessage, "... %d bytes, %d entries ...", byteCnt, lineCnt);
              drawScreen(eibiMessage, statusMessage);
            }
          }
        }

        // Done with the current buffer, start a new one
        charCnt = 0;
        if(c!='\n') charBuf[charCnt++] = c;
      }
    }
  }

  // Done with file and HTTP connection
  file.close();
  http.end();

  // Move new schedule to its permanent place
  LittleFS.remove(EIBI_PATH);
  LittleFS.rename(TEMP_PATH, EIBI_PATH);

  // Success
  identifyFrequency(currentFrequency + currentBFO / 1000);
  drawScreen(eibiMessage, "DONE!");
  return(true);
}
