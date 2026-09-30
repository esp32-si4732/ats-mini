#include "Memories.h"
#include "Utils.h"

#include <algorithm>
#include <freertos/semphr.h>

// All slot access and the derived index share the same lock. Never return
// pointers into memories: the web task can replace them while we draw.
static StaticSemaphore_t memoryMutexStorage;
static SemaphoreHandle_t memoryMutex;

struct MemoryLock
{
  MemoryLock() { xSemaphoreTake(memoryMutex, portMAX_DELAY); }
  ~MemoryLock() { xSemaphoreGive(memoryMutex); }
};

static Memory *memories = nullptr;
static uint8_t nameIndex[MEMORY_COUNT];
static int nameCount = 0;
static bool indexDirty = true;

// Called once during startup, before loading preferences or starting network tasks.
bool memoriesInit()
{
  memoryMutex = xSemaphoreCreateMutexStatic(&memoryMutexStorage);
  memories = static_cast<Memory *>(ps_calloc(MEMORY_COUNT, sizeof(Memory)));
  return(memories != nullptr);
}

int getTotalMemories() { return(MEMORY_COUNT); }

Memory getMemory(uint8_t slot)
{
  MemoryLock lock;
  return(memories[slot]);
}

void setMemory(uint8_t slot, const Memory &memory)
{
  MemoryLock lock;
  memories[slot] = memory;
  indexDirty = true;
}

void setMemories(const Memory *replacement)
{
  MemoryLock lock;
  memcpy(memories, replacement, MEMORY_COUNT * sizeof(Memory));
  indexDirty = true;
}

static bool hasMemoryName(const Memory &memory)
{
  return(memory.freq && memory.name[strspn(memory.name, " ")]);
}

bool findMemoryName(uint32_t freq, uint8_t mode, uint8_t band, uint8_t preferred, char (&name)[10])
{
  MemoryLock lock;
  name[0] = '\0';
  if(preferred<MEMORY_COUNT)
  {
    const Memory &memory = memories[preferred];
    if(memory.freq==freq && memory.mode==mode && hasMemoryName(memory))
    {
      memcpy(name, memory.name, sizeof(name));
      return(true);
    }
  }

  // Rebuild lazily so a complete preferences load sorts only once.
  if(indexDirty)
  {
    nameCount = 0;
    for(int i=0 ; i<MEMORY_COUNT ; i++)
      if(hasMemoryName(memories[i])) nameIndex[nameCount++] = i;
    std::sort(nameIndex, nameIndex + nameCount, [](uint8_t a, uint8_t b) {
      const Memory &left = memories[a], &right = memories[b];
      if(left.freq != right.freq) return(left.freq < right.freq);
      if(left.mode != right.mode) return(left.mode < right.mode);
      return(a < b);
    });
    indexDirty = false;
  }

  const auto *first = std::lower_bound(nameIndex, nameIndex + nameCount, freq, [mode](uint8_t slot, uint32_t hz) {
    const Memory &memory = memories[slot];
    return(memory.freq<hz || (memory.freq==hz && memory.mode<mode));
  });

  int match = -1;
  for(const auto *entry=first ; entry<nameIndex + nameCount ; entry++)
  {
    int slot = *entry;
    const Memory &memory = memories[slot];
    if(memory.freq!=freq || memory.mode!=mode) break;
    // Matches are in slot order; keep the first unless this band matches.
    if(match<0 || memory.band==band) match = slot;
    if(memory.band==band) break;
  }
  if(match<0) return(false);
  memcpy(name, memories[match].name, sizeof(name));
  return(true);
}

//
// Check if given memory entry belongs to given band
//
bool isMemoryInBand(const Band *band, const Memory *memory)
{
  uint16_t freq = freqFromHz(memory->freq, memory->mode);
  if(freq<band->minimumFreq) return(false);
  if(freq>band->maximumFreq) return(false);
  if(freq==band->maximumFreq && bfoFromHz(memory->freq)) return(false);
  if(memory->mode==FM && band->bandMode!=FM) return(false);
  if(memory->mode!=FM && band->bandMode==FM) return(false);
  return(true);
}
