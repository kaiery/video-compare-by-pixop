#pragma once
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace launcher {
using OptionValue = std::variant<bool, std::wstring>;
using GlobalOptions = std::map<std::string, OptionValue>;
using InputOptions = std::map<std::string, std::wstring>;

enum class InputKind { File, ImageSequence, Address, Script, Reference };
struct InputSource {
  InputKind kind{InputKind::File};
  std::wstring text;
};
enum class OverrideMode { Inherit, Replace, Append, Clear };
struct InputOverride {
  OverrideMode mode{OverrideMode::Inherit};
  std::wstring value;
};
struct RightInput {
  std::uint64_t id{0};  // Stable across reorder, independent of active-list index.
  bool enabled{true};
  InputSource source;
  std::map<std::string, InputOverride> overrides;
};
struct ConfigurationSources {
  bool load_automatic_file{false};
  std::vector<std::wstring> files;  // Order matters. Contents read in P06, not here.
};
struct Session {
  unsigned schema_version{1};
  std::wstring engine_path;
  std::wstring working_directory;
  GlobalOptions global;
  InputOptions common;
  InputSource left;
  InputOptions left_options;
  InputOptions right_defaults;
  std::vector<RightInput> right_inputs;
  ConfigurationSources configuration;
  std::uint64_t next_input_id{1};
};

Session make_session(const std::wstring& launcher_directory);
std::uint64_t add_right(Session& session, InputSource source);
bool move_right(Session& session, std::uint64_t id, std::size_t destination);
bool remove_right(Session& session, std::uint64_t id);
std::uint64_t duplicate_right(Session& session, std::uint64_t id);
// Assign a known CLI option by stable ID/key/alias. Type/scope errors do not mutate.
// Query options use build_query instead. Value validation is deferred to build_comparison.
bool set_option(Session& session, const std::string& option, OptionValue value, std::wstring& error);
bool unset_option(Session& session, const std::string& option);

enum class Severity { Error, Warning };
struct Diagnostic {
  Severity severity;
  std::string code;
  std::string field;
  std::wstring message;
};
struct Emission {
  std::string id;
  std::string field;  // e.g. right[17].filters; original stable ID, not list index.
  std::size_t argument_index;  // Index into LaunchPlan::arguments, which includes argv[0].
};
struct LaunchPlan {
  std::wstring executable;
  std::wstring working_directory;
  std::vector<std::wstring> arguments;  // Includes absolute executable as argv[0].
  std::wstring command_line;  // CreateProcess-compatible; NOT cmd.exe/PowerShell syntax.
  std::wstring preview;       // Human-readable argv + working directory; do not execute.
  std::vector<Emission> emissions;
};
struct BuildResult {
  std::optional<LaunchPlan> plan;  // Never present when validation has errors.
  std::vector<Diagnostic> diagnostics;
  bool ok() const { return plan.has_value(); }
};

BuildResult build_comparison(const Session& session, bool allow_long_command = false);
BuildResult build_query(const std::wstring& executable, const std::wstring& working_directory,
                        const std::string& query, std::optional<std::wstring> search = std::nullopt);
std::wstring quote_windows_argument(const std::wstring& argument);
std::wstring make_windows_command_line(const std::vector<std::wstring>& arguments);
}  // namespace launcher
