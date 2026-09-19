#pragma once
#include <string>
#include <vector>

namespace launcher {
enum class Scope { Global, Common, Left, Right, Paired, Query, Config };
enum class Rule { Flag, Text, RequiredText, Font, Positive, Nonnegative, Signed,
                  UInt, PositiveInt, Peak, WindowSize, Size, Canvas, Choice, TimeShift };

struct OptionDefinition {
  std::string id;
  std::string key;
  std::wstring flag;
  std::wstring short_flag;
  Scope scope;
  std::string field;
  Rule rule;
  std::wstring choices;  // Pipe-separated canonical values and accepted aliases.
  std::wstring default_hint;  // Display only: never automatically emitted.
  bool takes_value() const { return rule != Rule::Flag; }
  bool permits_empty() const {
    return rule == Rule::Text || key == "aspect-lock" || key == "aspect-view-mode" || key == "conversion-fit";
  }
};

struct OverrideDefinition {
  std::string id;
  std::string key;
  Rule rule;
  std::wstring choices;
  bool allow_empty;
  bool allow_append;
};

const std::vector<OptionDefinition>& option_catalog();
const std::vector<OverrideDefinition>& override_catalog();
// Case-sensitive: CLI-010, 10-bpc, --10-bpc and -b identify the same definition.
const OptionDefinition* find_option(const std::string& id_key_or_alias);
const OverrideDefinition* find_override(const std::string& id_or_key);
}  // namespace launcher
