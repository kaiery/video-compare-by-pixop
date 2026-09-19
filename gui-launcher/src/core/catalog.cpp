#include "catalog.h"

namespace launcher {
const std::vector<OptionDefinition>& option_catalog() {
  using S = Scope;
  using R = Rule;
  // Explicit, standalone schema. No engine headers or libraries are imported.
  static const std::vector<OptionDefinition> definitions{
    {"CLI-001", "help", L"--help", L"-h", S::Query, "help", R::Flag, L"", L""},
    {"CLI-002", "version", L"--version", L"-V", S::Query, "version", R::Flag, L"", L""},
    {"CLI-003", "show-controls", L"--show-controls", L"-c", S::Query, "show-controls", R::Flag, L"", L""},
    {"CLI-004", "verbose", L"--verbose", L"-v", S::Global, "verbose", R::Flag, L"", L"false"},
    {"CLI-005", "options-file", L"--options-file", L"-o", S::Config, "options-file", R::RequiredText, L"", L""},
    {"CLI-006", "fullscreen", L"--fullscreen", L"-u", S::Global, "fullscreen", R::Flag, L"", L"false"},
    {"CLI-007", "high-dpi", L"--high-dpi", L"-d", S::Global, "high-dpi", R::Flag, L"", L"false"},
    {"CLI-008", "font", L"--font", L"-g", S::Global, "font", R::Font, L"auto|scp|sarasa", L"auto"},
    {"CLI-009", "ui-scale", L"--ui-scale", L"-U", S::Global, "ui-scale", R::Positive, L"", L"1"},
    {"CLI-010", "10-bpc", L"--10-bpc", L"-b", S::Global, "10-bpc", R::Flag, L"", L"false"},
    {"CLI-011", "fast-alignment", L"--fast-alignment", L"-F", S::Global, "fast-alignment", R::Flag, L"", L"false"},
    {"CLI-012", "bilinear-texture", L"--bilinear-texture", L"-I", S::Global, "bilinear-texture", R::Flag, L"", L"false"},
    {"CLI-013", "subtraction-mode", L"--subtraction-mode", L"-S", S::Global, "subtraction-mode", R::Flag, L"", L"false"},
    {"CLI-014", "display-number", L"--display-number", L"-n", S::Global, "display-number", R::UInt, L"", L"0"},
    {"CLI-015", "display-mode", L"--mode", L"-m", S::Global, "display-mode", R::Choice, L"split|vstack|hstack", L"split"},
    {"CLI-016", "window-size", L"--window-size", L"-w", S::Global, "window-size", R::WindowSize, L"", L"derived"},
    {"CLI-017", "window-fit-display", L"--window-fit-display", L"-W", S::Global, "window-fit-display", R::Flag, L"", L"false"},
    {"CLI-018", "aspect-lock", L"--aspect-lock", L"-k", S::Global, "aspect-lock", R::Choice, L"off|window|content", L"off"},
    {"CLI-019", "aspect-view-mode", L"--aspect-view-mode", L"-x", S::Global, "aspect-view-mode", R::Choice, L"stretch|original|dynamic|16:9|4:3|1:1|16x9|4x3|1x1", L"stretch"},
    {"CLI-020", "auto-loop-mode", L"--auto-loop-mode", L"-a", S::Global, "auto-loop-mode", R::Choice, L"off|on|pp", L"off"},
    {"CLI-021", "frame-buffer-size", L"--frame-buffer-size", L"-f", S::Global, "frame-buffer-size", R::PositiveInt, L"", L"50"},
    {"CLI-022", "time-shift", L"--time-shift", L"-t", S::Global, "time-shift", R::TimeShift, L"", L"0"},
    {"CLI-023", "wheel-sensitivity", L"--wheel-sensitivity", L"-s", S::Global, "wheel-sensitivity", R::Signed, L"", L"1"},
    {"CLI-024", "color-space", L"--color-space", L"-C", S::Paired, "color-space", R::Text, L"", L"auto"},
    {"CLI-025", "color-range", L"--color-range", L"-A", S::Paired, "color-range", R::Text, L"", L"auto"},
    {"CLI-026", "color-primaries", L"--color-primaries", L"-P", S::Paired, "color-primaries", R::Text, L"", L"auto"},
    {"CLI-027", "color-trc", L"--color-trc", L"-N", S::Paired, "color-trc", R::Text, L"", L"auto"},
    {"CLI-028", "tone-map-mode", L"--tone-map-mode", L"-T", S::Paired, "tone-map-mode", R::Choice, L"auto|off|on|rel", L"auto"},
    {"CLI-029", "left-peak-nits", L"--left-peak-nits", L"-L", S::Left, "peak-nits", R::Peak, L"", L"SDR:100 HDR:500"},
    {"CLI-030", "right-peak-nits", L"--right-peak-nits", L"-R", S::Right, "peak-nits", R::Peak, L"", L"SDR:100 HDR:500"},
    {"CLI-031", "boost-tone", L"--boost-tone", L"-B", S::Paired, "boost-tone", R::Nonnegative, L"", L"1"},
    {"CLI-032", "filters", L"--filters", L"-i", S::Common, "filters", R::Text, L"", L""},
    {"CLI-033", "left-filters", L"--left-filters", L"-l", S::Left, "filters", R::Text, L"", L""},
    {"CLI-034", "right-filters", L"--right-filters", L"-r", S::Right, "filters", R::Text, L"", L""},
    {"CLI-035", "find-filters", L"--find-filters", L"", S::Query, "find-filters", R::Text, L"", L""},
    {"CLI-036", "conversion-size", L"--conversion-size", L"", S::Global, "conversion-size", R::Canvas, L"max", L"max"},
    {"CLI-037", "conversion-fit", L"--conversion-fit", L"", S::Global, "conversion-fit", R::Choice, L"stretch|native", L"stretch"},
    {"CLI-038", "histogram-window", L"--histogram-window", L"", S::Global, "histogram-window", R::Flag, L"", L"false"},
    {"CLI-039", "vectorscope-window", L"--vectorscope-window", L"", S::Global, "vectorscope-window", R::Flag, L"", L"false"},
    {"CLI-040", "waveform-window", L"--waveform-window", L"", S::Global, "waveform-window", R::Flag, L"", L"false"},
    {"CLI-041", "histogram-options", L"--histogram-options", L"", S::Global, "histogram-options", R::Text, L"", L""},
    {"CLI-042", "vectorscope-options", L"--vectorscope-options", L"", S::Global, "vectorscope-options", R::Text, L"", L""},
    {"CLI-043", "waveform-options", L"--waveform-options", L"", S::Global, "waveform-options", R::Text, L"", L""},
    {"CLI-044", "scope-size", L"--scope-size", L"", S::Global, "scope-size", R::Size, L"", L"1024x256"},
    {"CLI-045", "scope-notop", L"--scope-notop", L"", S::Global, "scope-notop", R::Flag, L"", L"false"},
    {"CLI-046", "find-protocols", L"--find-protocols", L"", S::Query, "find-protocols", R::Text, L"", L""},
    {"CLI-047", "demuxer", L"--demuxer", L"", S::Common, "demuxer", R::Text, L"", L"auto"},
    {"CLI-048", "left-demuxer", L"--left-demuxer", L"", S::Left, "demuxer", R::Text, L"", L"auto"},
    {"CLI-049", "right-demuxer", L"--right-demuxer", L"", S::Right, "demuxer", R::Text, L"", L"auto"},
    {"CLI-050", "find-demuxers", L"--find-demuxers", L"", S::Query, "find-demuxers", R::Text, L"", L""},
    {"CLI-051", "decoder", L"--decoder", L"", S::Common, "decoder", R::Text, L"", L"auto"},
    {"CLI-052", "left-decoder", L"--left-decoder", L"", S::Left, "decoder", R::Text, L"", L"auto"},
    {"CLI-053", "right-decoder", L"--right-decoder", L"", S::Right, "decoder", R::Text, L"", L"auto"},
    {"CLI-054", "find-decoders", L"--find-decoders", L"", S::Query, "find-decoders", R::Text, L"", L""},
    {"CLI-055", "hwaccel", L"--hwaccel", L"", S::Common, "hwaccel", R::Text, L"", L"none"},
    {"CLI-056", "left-hwaccel", L"--left-hwaccel", L"", S::Left, "hwaccel", R::Text, L"", L"none"},
    {"CLI-057", "right-hwaccel", L"--right-hwaccel", L"", S::Right, "hwaccel", R::Text, L"", L"none"},
    {"CLI-058", "find-hwaccels", L"--find-hwaccels", L"", S::Query, "find-hwaccels", R::Text, L"", L""},
    {"CLI-059", "libvmaf-options", L"--libvmaf-options", L"", S::Global, "libvmaf-options", R::Text, L"", L""},
    {"CLI-060", "disable-auto-options-file", L"--no-auto-options-file", L"", S::Config, "disable-auto-options-file", R::Flag, L"", L"GUI:true Engine:false"},
    {"CLI-061", "disable-auto-filters", L"--no-auto-filters", L"", S::Global, "disable-auto-filters", R::Flag, L"", L"false"}
  };
  return definitions;
}

const std::vector<OverrideDefinition>& override_catalog() {
  using R = Rule;
  static const std::vector<OverrideDefinition> definitions{
    {"RV-001", "filters", R::Text, L"", true, true},
    {"RV-002", "color-space", R::Text, L"", true, false},
    {"RV-003", "color-range", R::Text, L"", true, false},
    {"RV-004", "color-primaries", R::Text, L"", true, false},
    {"RV-005", "color-trc", R::Text, L"", true, false},
    {"RV-006", "decoder", R::Text, L"", true, false},
    {"RV-007", "demuxer", R::Text, L"", true, false},
    {"RV-008", "hwaccel", R::Text, L"", true, false},
    {"RV-009", "tone-map-mode", R::Choice, L"auto|off|on|rel", true, false},
    {"RV-010", "peak-nits", R::Peak, L"", true, false},
    {"RV-011", "boost-tone", R::Nonnegative, L"", true, false}
  };
  return definitions;
}

const OptionDefinition* find_option(const std::string& name) {
  const std::wstring wide(name.begin(), name.end());
  for (const auto& def : option_catalog()) {
    if (name == def.id || name == def.key || wide == def.flag || (!wide.empty() && wide == def.short_flag)) return &def;
  }
  return nullptr;
}
const OverrideDefinition* find_override(const std::string& name) {
  for (const auto& def : override_catalog()) if (name == def.id || name == def.key) return &def;
  return nullptr;
}
}  // namespace launcher
