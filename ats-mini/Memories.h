#ifndef MEMORIES_H
#define MEMORIES_H

#include "Common.h"

// Number of memory slots
#define MEMORY_COUNT  99

bool memoriesInit();
int getTotalMemories();
Memory getMemory(uint8_t slot);
void setMemory(uint8_t slot, const Memory &memory);
void setMemories(const Memory *replacement);
bool findMemoryName(uint32_t freq, uint8_t mode, uint8_t band, uint8_t preferred, char (&name)[10]);
bool isMemoryInBand(const Band *band, const Memory *memory);

#endif // MEMORIES_H
