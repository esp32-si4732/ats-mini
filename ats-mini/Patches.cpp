#include "Common.h"
#include "Patches.h"
#include "Storage.h"
#include "Menu.h"
#include "Utils.h"
#include "Draw.h"
#include "patch_init.h"

#include <LittleFS.h>
#include <esp_heap_caps.h>
#include <freertos/semphr.h>

const char *const patchModeNames[PATCH_MODE_COUNT] = {"AM", "SSB"};
const char *const patchSetNames[PATCH_SET_COUNT + 1] = {"Default", "Custom 1", "Custom 2", "Custom 3"};

static SemaphoreHandle_t patchMutex;
// Protected by patchMutex; file operations and radio application run outside it.
static bool patchBusy = false;
static int pendingSelection = -1;
static uint8_t loadedSlot = 255;
static uint8_t loadedMode = 255;

struct PatchLock
{
  PatchLock() { if(patchMutex) xSemaphoreTake(patchMutex, portMAX_DELAY); }
  ~PatchLock() { if(patchMutex) xSemaphoreGive(patchMutex); }
};

static String patchPath(uint8_t slot, uint8_t mode, bool temporary = false)
{
  return String("/patch-") + patchModeNames[mode] + "-" + String(slot) + (temporary? ".tmp" : ".bin");
}

static bool patchRemoveFiles(uint8_t slot, bool temporary)
{
  bool ok = true;
  for(uint8_t mode = 0; mode < PATCH_MODE_COUNT; mode++)
  {
    String path = patchPath(slot, mode, temporary);
    if(LittleFS.exists(path) && !LittleFS.remove(path)) ok = false;
  }
  return ok;
}

void patchesInit()
{
  patchMutex = xSemaphoreCreateMutex();
  for(uint8_t slot = 1; slot <= PATCH_SET_COUNT; slot++) patchRemoveFiles(slot, true);
  if(dspPatchesIdx != DSP_PATCHES_DEFAULT && !patchesModes(dspPatchesIdx))
  {
    dspPatchesIdx = DSP_PATCHES_DEFAULT;
    prefsSave(SAVE_SETTINGS);
  }
}

void patchesSnapshot(uint8_t &selected, bool &busy)
{
  PatchLock lock;
  selected = dspPatchesIdx;
  busy = patchBusy || pendingSelection >= 0;
}

uint8_t patchesModes(uint8_t slot)
{
  uint8_t modes = 0;
  if(slot && slot <= PATCH_SET_COUNT)
    for(uint8_t mode = 0; mode < PATCH_MODE_COUNT; mode++)
      if(LittleFS.exists(patchPath(slot, mode))) modes |= 1U << mode;
  return modes;
}

// Called with patchMutex held; reserving an operation must use the same lock.
static bool patchesAvailable(const char *&error, uint8_t editingSlot)
{
  if(!patchMutex) error = "Patch storage is unavailable.";
  else if(patchBusy || pendingSelection >= 0) error = "Another patch request is in progress.";
  else if(editingSlot && dspPatchesIdx == editingSlot)
    error = "Select another patch set before editing this slot.";
  else return true;
  return false;
}

// Main task only, with either patchBusy or pendingSelection reserved throughout.
static void patchApplySelection(uint8_t slot)
{
  {
    PatchLock lock;
    if(slot == dspPatchesIdx) return;
    dspPatchesIdx = slot;
  }
  // The saved band frequency already includes the whole-kHz BFO offset.
  int16_t bfo = currentBFO % 1000;
  selectBand(bandIdx);
  if(isSSB()) updateBFO(bfo, true);
  prefsSave(SAVE_SETTINGS);
}

// Reserve before checking files so deletion cannot race with selection.
static bool patchReserveSelection(uint8_t slot, const char *&error)
{
  if(slot > PATCH_SET_COUNT) { error = "Invalid patch slot."; return false; }
  {
    PatchLock lock;
    if(!patchesAvailable(error, 0)) return false;
    patchBusy = true;
  }
  if(slot && !patchesModes(slot))
  {
    error = "This patch slot is empty.";
    PatchLock lock;
    patchBusy = false;
    return false;
  }
  return true;
}

bool patchesSelect(uint8_t slot)
{
  const char *error = nullptr;
  if(!patchReserveSelection(slot, error)) return false;
  patchApplySelection(slot);
  {
    PatchLock lock;
    patchBusy = false;
  }
  return true;
}

void PatchUpload::begin(uint8_t slot, PatchMode mode)
{
  if(!slot || slot > PATCH_SET_COUNT || mode >= PATCH_MODE_COUNT)
  { error = "Invalid patch slot or mode."; return; }
  {
    PatchLock lock;
    if(!patchesAvailable(error, slot)) return;
    patchBusy = true;
    state = State::Receiving;
  }
  this->slot = slot;
  this->mode = mode;
  file = LittleFS.open(patchPath(slot, mode, true), "w");
  if(!file)
  {
    error = "Cannot create the patch file. Check free storage.";
    cancel();
  }
}

void PatchUpload::write(size_t index, const uint8_t *data, size_t len, bool final)
{
  if(error) return;
  if(state != State::Receiving || index != received)
    error = "Incomplete or unexpected patch upload.";
  else if(!parser.write(data, len)) error = "Invalid patch structure or file exceeds 32 KiB.";
  else if(file.write(data, len) != len) error = "Could not write patch data. Check free storage.";
  else
  {
    received += len;
    if(final)
    {
      if(!parser.finish()) error = "Incomplete patch or missing trailer.";
      else state = State::Ready;
      file.close();
      if(!error)
      {
        File saved = LittleFS.open(patchPath(slot, mode, true), "r");
        if(!saved || saved.size() != received)
          error = "Could not write patch data. Check free storage.";
      }
    }
  }
  if(error) cancel();
}

void PatchUpload::finish()
{
  if(!error)
  {
    if(state != State::Ready) error = "Upload one complete .bin patch.";
    // Keep the old binary intact until the complete upload passes validation.
    else if(!LittleFS.rename(patchPath(slot, mode, true), patchPath(slot, mode)))
      error = "Could not install patch. Check free storage and try again.";
  }
  cancel();
}

void PatchUpload::cancel()
{
  if(state == State::Idle) return;
  file.close();
  LittleFS.remove(patchPath(slot, mode, true));
  PatchLock lock;
  patchBusy = false;
  state = State::Idle;
}

bool patchesDelete(uint8_t slot, const char *&error)
{
  if(!slot || slot > PATCH_SET_COUNT) { error = "Invalid patch slot."; return false; }
  {
    PatchLock lock;
    if(!patchesAvailable(error, slot)) return false;
    patchBusy = true;
  }
  bool ok = patchRemoveFiles(slot, false);
  if(!ok) error = "Could not delete all patches. Try again.";
  {
    PatchLock lock;
    patchBusy = false;
  }
  return ok;
}

bool patchesRequestSelection(uint8_t slot, const char *&error)
{
  if(!patchReserveSelection(slot, error)) return false;
  PatchLock lock;
  pendingSelection = slot;
  patchBusy = false;
  return true;
}

bool patchesTick()
{
  int selection;
  {
    PatchLock lock;
    selection = pendingSelection;
  }
  if(selection < 0) return false;
  patchApplySelection(selection);
  {
    PatchLock lock;
    pendingSelection = -1;
  }
  return true;
}

// Only inactive slots can change; selection is blocked during file operations.
void loadDSPPatch(uint8_t bandwidth, bool draw)
{
  if(currentMode == FM)
  {
    // The normal FM power-up discards any AM/SSB patch.
    loadedSlot = loadedMode = 255;
    return;
  }
  uint8_t mode = isSSB()? PATCH_SSB : PATCH_AM;
  uint8_t slot = dspPatchesIdx;
  if(slot > PATCH_SET_COUNT || (slot && !LittleFS.exists(patchPath(slot, mode)))) slot = 0;
  if(mode == loadedMode && slot == loadedSlot) return;

  uint8_t *content = nullptr;
  size_t size = 0;
  if(slot)
  {
    File file = LittleFS.open(patchPath(slot, mode), "r");
    size = file? file.size() : 0;
    if(size >= 24 && size <= PATCH_MAX_BYTES && size % 8 == 0)
      content = static_cast<uint8_t *>(heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    bool valid = content && file.read(content, size) == size;
    file.close();
    PatchFraming framing;
    for(size_t i = 0; valid && i < size; i += 8) valid = framing.row(content + i);
    if(!valid || !framing.finish())
    {
      free(content);
      content = nullptr;
      slot = 0;
      statusShow("Patch unavailable", "Using Default");
    }
  }

  if(mode == PATCH_SSB)
  {
    if(draw) drawMessage("Loading SSB");
    rx.loadPatch(content? content : ssb_patch_content, content? size : sizeof(ssb_patch_content), bandwidth);
  }
  else if(content)
  {
    if(draw) drawMessage("Loading AM");
    rx.loadAMPatch(content, size);
  }
  else if(mode == PATCH_AM && loadedMode == PATCH_AM && loadedSlot != 0 && loadedSlot != 255)
  {
    // setAM() does not power-cycle when already in AM.
    rx.loadAMPatch(nullptr, 0);
  }
  free(content);
  loadedSlot = slot;
  loadedMode = mode;
}
