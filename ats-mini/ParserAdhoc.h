#ifndef PARSER_ADHOC_H
#define PARSER_ADHOC_H

#include "ParserTypes.h"
#include "Themes.h"

struct AdhocCommand {
  enum class Id : uint8_t {
    Unknown = 0,
    RotateCw,
    RotateCcw,
    Click,
    ShortPress,
    BandUp,
    BandDown,
    ModeUp,
    ModeDown,
    StepUp,
    StepDown,
    BandwidthUp,
    BandwidthDown,
    AgcUp,
    AgcDown,
    VolumeUp,
    VolumeDown,
    BacklightUp,
    BacklightDown,
    SleepOn,
    SleepOff,
    CalUp,
    CalDown,
    CaptureScreen,
    CaptureScreenBinary,
    ToggleLog,
    GetMemories,
    SetMemory,
    ToggleThemeEditor,
    SetTheme,
    GetTheme,
    SetFrequency,
  };

  static constexpr size_t BAND_LEN = 8;
  static constexpr size_t MODE_LEN = 4;
  static constexpr size_t THEME_WORDS =
      (sizeof(ColorTheme) - offsetof(ColorTheme, bg)) / sizeof(uint16_t);

  Id id;
  uint32_t memory_slot;
  char band[BAND_LEN];
  uint32_t frequency;
  char mode[MODE_LEN];
  bool has_name;
  char name[10]; // Omitted preserves the name; present but empty clears it.
  uint16_t theme_words[THEME_WORDS];
};


// Pure complete-frame parser. Text frames include their line ending.
// On errors the output may be partially filled and must not be executed.
ParseResult parseAdhoc(ByteSpan frame, AdhocCommand& out);

namespace command_parser {
extern const FramingRules adhocFraming;
}

#endif
