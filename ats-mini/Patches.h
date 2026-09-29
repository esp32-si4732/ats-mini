#ifndef PATCHES_H
#define PATCHES_H

#include <FS.h>
#include "PatchParser.h"

#define PATCH_SET_COUNT 3

enum PatchMode : uint8_t { PATCH_AM, PATCH_SSB, PATCH_MODE_COUNT };
extern const char *const patchModeNames[PATCH_MODE_COUNT];
extern const char *const patchSetNames[PATCH_SET_COUNT + 1];

void patchesInit();
bool patchesTick();
void patchesSnapshot(uint8_t &selected, bool &busy);
uint8_t patchesModes(uint8_t slot);
// Apply and save a selection on the main task.
bool patchesSelect(uint8_t slot);
void loadDSPPatch(uint8_t bandwidth, bool draw = true);
bool patchesDelete(uint8_t slot, const char *&error);
bool patchesRequestSelection(uint8_t slot, const char *&error);

// Owned by one HTTP request; destruction cleans up an interrupted upload.
struct PatchUpload
{
  PatchUpload() = default;
  ~PatchUpload() { cancel(); }
  PatchUpload(const PatchUpload &) = delete;
  PatchUpload &operator=(const PatchUpload &) = delete;

  const char *error = nullptr;
  void begin(uint8_t slot, PatchMode mode);
  void write(size_t index, const uint8_t *data, size_t len, bool final);
  void finish();

private:
  void cancel();
  // Receiving and Ready both hold the storage reservation.
  enum class State : uint8_t { Idle, Receiving, Ready };
  State state = State::Idle;
  uint8_t slot = 0;
  PatchMode mode = PATCH_AM;
  File file;
  PatchStream parser;
  size_t received = 0;
};

#endif
