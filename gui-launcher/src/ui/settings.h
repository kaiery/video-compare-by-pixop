#pragma once
#include <windows.h>
#include "core/model.h"
#include "core/catalog.h"

namespace launcher::ui {
enum class Page { Playback, Display, Color, Filters, Decode, Analysis, Font, Config, Queries, Main };
struct SettingDefinition {
  const char* id;
  Page page;
  int main_control; // Nonzero for existing P03 controls; no duplicate advanced editor.
  const wchar_t* label;
  const wchar_t* hint;
};
const std::vector<SettingDefinition>& settings_catalog();
// Stable IDs: CLI-n => 2000+(n-1)*10; enable/value offsets 0/1.
// Paired rows additionally use offsets 2/3 (left) and 4/5 (right).
int setting_control(const std::string& id);
// RV-n => 4000+(n-1)*10: mode/value offsets 0/1.
int override_control(const std::string& id);
bool show_settings(HWND owner, HINSTANCE instance, Session& session);
bool show_overrides(HWND owner, HINSTANCE instance, const Session& session, RightInput& input);
}
