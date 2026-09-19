#include "model.h"
#include "catalog.h"
#include <algorithm>
#include <cfloat>
#include <climits>
#include <cmath>
#include <filesystem>
#include <limits>
#include <locale>
#include <regex>
#include <set>
#include <sstream>

namespace launcher {
namespace {
using Diagnostics = std::vector<Diagnostic>;
void error(Diagnostics& out, const std::string& code, const std::string& field, const std::wstring& text) {
  out.push_back({Severity::Error, code, field, text});
}
void warning(Diagnostics& out, const std::string& code, const std::string& field, const std::wstring& text) {
  out.push_back({Severity::Warning, code, field, text});
}
bool errors(const Diagnostics& out) {
  return std::any_of(out.begin(), out.end(), [](const auto& d) { return d.severity == Severity::Error; });
}
std::wstring wide(const std::string& ascii) { return {ascii.begin(), ascii.end()}; }
bool valid_text(const std::wstring& text) {
  for (std::size_t i = 0; i < text.size(); ++i) {
    const auto ch = static_cast<unsigned>(text[i]);
    if (ch == 0) return false;
    if (ch >= 0xD800 && ch <= 0xDBFF) {
      if (++i == text.size() || text[i] < 0xDC00 || text[i] > 0xDFFF) return false;
    } else if (ch >= 0xDC00 && ch <= 0xDFFF) return false;
  }
  return true;
}
std::vector<std::wstring> split(const std::wstring& text, wchar_t separator) {
  std::vector<std::wstring> parts;
  std::size_t first = 0;
  for (;;) {
    const auto next = text.find(separator, first);
    parts.push_back(text.substr(first, next == std::wstring::npos ? next : next - first));
    if (next == std::wstring::npos) break;
    first = next + 1;
  }
  return parts;
}
bool choice(const std::wstring& value, const std::wstring& choices) {
  const auto values = split(choices, L'|');
  return std::find(values.begin(), values.end(), value) != values.end();
}
bool decimal(const std::wstring& value, bool sign, long double& parsed, bool exponent = false) {
  // Bound numeric input before running a regex; useful values fit comfortably in this limit.
  if (value.size() > 256) return false;
  static const std::wregex unsigned_re(LR"((\d+(\.\d*)?|\.\d+))");
  static const std::wregex signed_re(LR"([+-]?(\d+(\.\d*)?|\.\d+))");
  static const std::wregex exponent_re(LR"([+-]?(\d+(\.\d*)?|\.\d+)([eE][+-]?\d+)?)");
  if (!std::regex_match(value, exponent ? exponent_re : (sign ? signed_re : unsigned_re))) return false;
  std::wistringstream stream(value);
  stream.imbue(std::locale::classic());
  stream >> parsed;
  return !stream.fail() && std::isfinite(parsed);
}
bool integer(const std::wstring& value, bool positive) {
  if (value.empty() || value.find_first_not_of(L"0123456789") != std::wstring::npos) return false;
  try {
    const auto number = std::stoull(value);
    return number <= static_cast<unsigned long long>(std::numeric_limits<int>::max()) && (!positive || number > 0);
  } catch (...) { return false; }
}
bool timestamp(const std::wstring& value) {
  const auto parts = split(value, L':');
  if (parts.empty() || parts.size() > 3) return false;
  long double total = 0;
  for (std::size_t i = 0; i < parts.size(); ++i) {
    long double component = 0;
    if (!decimal(parts[i], true, component, i + 1 == parts.size())) return false;
    if (i + 1 != parts.size()) {
      if (parts[i].find(L'.') != std::wstring::npos || component > INT_MAX || component < INT_MIN) return false;
    }
    total = total * 60 + component;  // Mirrors signed-component semantics; never rewrites the argument.
  }
  return std::isfinite(total) && std::fabs(total * 1000) < 9223372036854775807.0L;
}
bool timeshift(const std::wstring& value) {
  if (value.empty()) return false;
  if (value[0] != L'x') return timestamp(value);
  const auto end = value.find_first_of(L"+-", 1);
  const auto ratio = split(value.substr(1, end == std::wstring::npos ? end : end - 1), L'/');
  if (ratio.empty() || ratio.size() > 2) return false;
  long double numerator = 0, denominator = 1;
  if (!decimal(ratio[0], false, numerator) || numerator <= 0) return false;
  if (ratio.size() == 2 && (!decimal(ratio[1], false, denominator) || denominator <= 0)) return false;
  const auto multiplier = numerator / denominator;
  // The engine converts to AVRational with max denominator/numerator 1,000,000.
  if (multiplier <= 0 || !std::isfinite(multiplier) || numerator > DBL_MAX || denominator > DBL_MAX) return false;
  return end == std::wstring::npos || timestamp(value.substr(end));
}
bool valid_value(Rule rule, const std::wstring& value, const std::wstring& choices, bool allow_empty = false) {
  if (!valid_text(value)) return false;
  if (value.empty() && allow_empty) return true;
  long double number = 0;
  switch (rule) {
    case Rule::Text: return true;
    case Rule::RequiredText:
    case Rule::Font: return !value.empty();
    case Rule::Flag: return false;
    case Rule::Positive:
    case Rule::Nonnegative:
    case Rule::Signed:
      return decimal(value, rule == Rule::Signed, number) && std::fabs(number) <= FLT_MAX &&
             (rule != Rule::Positive || static_cast<float>(number) > 0);
    case Rule::UInt: return integer(value, false);
    case Rule::PositiveInt: return integer(value, true);
    case Rule::Peak: return integer(value, true) && std::stoull(value) <= 10000;
    case Rule::Choice: return choice(value, choices);
    case Rule::TimeShift: return timeshift(value);
    case Rule::Canvas: if (value == L"max") return true; [[fallthrough]];
    case Rule::Size:
    case Rule::WindowSize: {
      const auto dims = split(value, L'x');
      if (dims.size() != 2 || (dims[0].empty() && dims[1].empty())) return false;
      return (integer(dims[0], true) || (rule == Rule::WindowSize && dims[0].empty())) &&
             (integer(dims[1], true) || (rule == Rule::WindowSize && dims[1].empty()));
    }
  }
  return false;
}

std::wstring path_value(const std::wstring& text, const std::wstring& base, const std::string& field, Diagnostics& out) {
  if (text.empty() || !valid_text(text) || text.find_first_of(L"\r\n\t\"<>|") != std::wstring::npos) {
    error(out, "E_PATH", field, L"本地路径为空、包含非法字符或无效 Unicode。"); return {};
  }
  try {
    std::filesystem::path path(text);
    if (!path.is_absolute()) {
      if (path.has_root_name() || path.has_root_directory() || base.empty()) {
        error(out, "E_PATH_BASE", field, L"请使用完整绝对路径或相对工作目录的路径，避免驱动器相对路径。"); return {};
      }
      path = std::filesystem::path(base) / path;
    }
    return path.lexically_normal().wstring();
  } catch (const std::exception&) {
    error(out, "E_PATH", field, L"无法解析本地路径。"); return {};
  }
}

const std::wstring* get(const InputOptions& options, const std::string& key) {
  const auto found = options.find(key);
  return found == options.end() ? nullptr : &found->second;
}
std::wstring value_or(const InputOptions& options, const std::string& key, const std::wstring& fallback = {}) {
  const auto* value = get(options, key);
  return value ? *value : fallback;
}
const OptionDefinition* input_definition(Scope scope, const std::string& field) {
  for (const auto& def : option_catalog()) {
    if (def.field == field && (def.scope == scope || def.scope == Scope::Paired)) return &def;
  }
  return nullptr;
}
void validate_inputs(const InputOptions& inputs, Scope scope, const std::string& prefix, Diagnostics& out) {
  for (const auto& entry : inputs) {
    const auto* def = input_definition(scope, entry.first);
    const auto field = prefix + "." + entry.first;
    if (!def) { error(out, "E_SCOPE", field, L"该设置不支持此输入作用范围。"); continue; }
    if (def->scope == Scope::Paired && entry.second.find(L':') != std::wstring::npos) {
      error(out, "E_PAIR", field, L"单侧字段不能包含左右分隔冒号。"); continue;
    }
    const bool peak_reference = def->rule == Rule::Peak && entry.second == L"__";
    if (!peak_reference && !valid_value(def->rule, entry.second, def->choices, true)) {
      error(out, "E_VALUE", field, L"参数值无效或超出范围。");
    }
  }
}

bool placeholder(const std::wstring& text) { return text.find(L"__") != std::wstring::npos; }
std::wstring replace_placeholder(const std::wstring& text, const std::wstring& base,
                                  const std::string& field, Diagnostics& out) {
  if (placeholder(text) && placeholder(base)) {
    error(out, "E_PLACEHOLDER", field, L"引用和被引用值都含有未解析占位符。"); return text;
  }
  // Same replacement-format semantics as the engine, including $ markers in the replacement.
  return base.empty() ? text : std::regex_replace(text, std::wregex(L"__"), base, std::regex_constants::format_first_only);
}
void mutual(std::wstring& left, std::wstring& right, const std::string& field, Diagnostics& out) {
  if ((placeholder(left) && right.empty()) || (placeholder(right) && left.empty())) {
    error(out, "E_PLACEHOLDER", field, L"占位符引用了空的另一侧配置。"); return;
  }
  if (placeholder(left)) left = replace_placeholder(left, right, field, out);
  else if (placeholder(right)) right = replace_placeholder(right, left, field, out);
}

std::map<std::string, std::wstring> right_templates(const Session& session, Diagnostics& out) {
  std::map<std::string, std::wstring> templates;
  for (const std::string key : {"filters", "decoder", "demuxer", "hwaccel"}) {
    const auto base = value_or(session.common, key);
    auto left = get(session.left_options, key) ? replace_placeholder(*get(session.left_options, key), base, "left." + key, out) : base;
    auto right = get(session.right_defaults, key) ? replace_placeholder(*get(session.right_defaults, key), base, "right." + key, out) : base;
    mutual(left, right, key, out);
    if (placeholder(left) || placeholder(right)) error(out, "E_PLACEHOLDER", key, L"配置中仍有未解析占位符。");
    // For Ri decoders/demuxers, the engine's template holds only the parsed name.
    // Hardware templates retain type:device; option dictionaries remain inherited in the engine.
    if (key != "filters") {
      const auto parts = split(right, L':');
      right = parts[0];
      if (key == "hwaccel" && parts.size() > 1) right += L":" + parts[1];
    }
    templates[key] = right;
  }
  auto left_peak = value_or(session.left_options, "peak-nits");
  auto right_peak = value_or(session.right_defaults, "peak-nits");
  mutual(left_peak, right_peak, "peak-nits", out);
  return templates;
}

bool reference(const InputSource& source) { return source.kind == InputKind::Reference || source.text == L"__"; }
std::wstring input_path(const InputSource& source, const std::wstring& base, const std::string& field, Diagnostics& out) {
  if (source.text.empty() || !valid_text(source.text)) {
    error(out, "E_INPUT", field, L"输入不能为空或包含无效 Unicode。"); return {};
  }
  if (source.kind == InputKind::Address) return source.text;
  if (source.kind == InputKind::File || source.kind == InputKind::ImageSequence || source.kind == InputKind::Script) {
    return path_value(source.text, base, field, out);
  }
  error(out, "E_INPUT_KIND", field, L"输入类型无效。"); return {};
}

void emit(LaunchPlan& plan, const OptionDefinition& def, const std::optional<std::wstring>& value, const std::string& field) {
  plan.emissions.push_back({def.id, field, plan.arguments.size()});
  plan.arguments.push_back(def.flag);
  if (value) plan.arguments.push_back(*value);
}
void finish(BuildResult& result, LaunchPlan plan, bool allow_long_command = false) {
  if (errors(result.diagnostics)) return;
  plan.command_line = make_windows_command_line(plan.arguments);
  if (!allow_long_command && plan.command_line.size() + 1 > 32767) {
    error(result.diagnostics, "E_COMMAND_TOO_LONG", "arguments", L"命令行超过 Windows 长度限制；请减少输入/参数，或在配置文件功能接入后使用无损参数文件。");
    return;
  }
  plan.preview = L"工作目录：" + plan.working_directory + L"\r\n程序：" + plan.executable +
                 L"\r\n以下为参数数组预览，不经过 CMD/PowerShell：\r\n";
  for (std::size_t i = 0; i < plan.arguments.size(); ++i) {
    auto display = quote_windows_argument(plan.arguments[i]);
    // Keep control characters legible in the preview; execution still uses the unmodified value.
    for (const auto& pair : {std::pair<wchar_t, std::wstring>{L'\n', L"\\n"}, {L'\r', L"\\r"}, {L'\t', L"\\t"}}) {
      std::size_t position = 0;
      while ((position = display.find(pair.first, position)) != std::wstring::npos) {
        display.replace(position, 1, pair.second); position += pair.second.size();
      }
    }
    plan.preview += L"argv[" + std::to_wstring(i) + L"] = " + display + L"\r\n";
  }
  for (const auto& issue : result.diagnostics) plan.preview += L"提示：" + issue.message + L"\r\n";
  result.plan = std::move(plan);
}
}  // namespace

BuildResult build_comparison(const Session& session, bool allow_long_command) {
  BuildResult result;
  auto& out = result.diagnostics;
  LaunchPlan plan;
  if (session.schema_version != 1) error(out, "E_SCHEMA", "schema_version", L"不支持的会话版本。");
  plan.working_directory = path_value(session.working_directory, {}, "working_directory", out);
  plan.executable = path_value(session.engine_path, plan.working_directory, "engine_path", out);
  plan.arguments.push_back(plan.executable);
  const bool external = session.configuration.load_automatic_file || !session.configuration.files.empty();
  if (external) warning(out, "W_EXTERNAL_CONFIG", "configuration", L"外部参数文件尚未解析：实际有效值及文件内布尔开关由引擎合并；当前预览只展示本会话传入的参数。");

  validate_inputs(session.common, Scope::Common, "common", out);
  validate_inputs(session.left_options, Scope::Left, "left", out);
  validate_inputs(session.right_defaults, Scope::Right, "right-defaults", out);
  for (const auto& entry : session.global) {
    const auto* def = find_option(entry.first);
    if (!def || def->scope != Scope::Global || def->key != entry.first) {
      error(out, "E_SCOPE", "global." + entry.first, L"未知全局字段或字段放错作用范围。"); continue;
    }
    if (def->takes_value() != std::holds_alternative<std::wstring>(entry.second)) {
      error(out, "E_TYPE", def->id, L"选项值类型不匹配。"); continue;
    }
    if (def->takes_value() && !valid_value(def->rule, std::get<std::wstring>(entry.second), def->choices, def->permits_empty())) {
      error(out, "E_VALUE", def->id, L"参数值无效或超出范围。");
    }
  }
  const auto fit = session.global.find("window-fit-display");
  if (session.global.count("window-size") && fit != session.global.end() &&
      std::holds_alternative<bool>(fit->second) && std::get<bool>(fit->second)) {
    error(out, "E_CONFLICT", "window-size", L"自定义窗口尺寸不能与适应屏幕同时使用。");
  }
  const auto time = session.global.find("time-shift");
  if (time != session.global.end() && std::holds_alternative<std::wstring>(time->second)) {
    const auto& text = std::get<std::wstring>(time->second);
    if (text.find(L':') != std::wstring::npos && text.find(L'-') != std::wstring::npos) {
      warning(out, "W_SIGNED_TIMESTAMP", "time-shift", L"时间表达式原样传递；当前引擎将带符号的时/分/秒逐项相加，不将负号自动应用到整个时间戳。建议用负秒数消除歧义。");
    }
  }
  const auto templates = external ? std::map<std::string, std::wstring>{} : right_templates(session, out);
  for (const auto& def : option_catalog()) {
    if (def.scope == Scope::Query) continue;
    if (def.scope == Scope::Config) {
      if (def.key == "options-file") {
        for (const auto& file : session.configuration.files) {
          emit(plan, def, path_value(file, plan.working_directory, "configuration.files", out), "configuration.files");
        }
      } else if (!session.configuration.load_automatic_file) emit(plan, def, std::nullopt, "configuration.automatic");
    } else if (def.scope == Scope::Global) {
      const auto found = session.global.find(def.field);
      if (found == session.global.end()) continue;
      if (const auto* flag = std::get_if<bool>(&found->second)) {
        if (*flag && !def.takes_value()) emit(plan, def, std::nullopt, "global." + def.field);
      } else if (def.takes_value()) {
        auto value = std::get<std::wstring>(found->second);
        if (def.rule == Rule::Font && !choice(value, def.choices)) value = path_value(value, plan.working_directory, "font", out);
        emit(plan, def, value, "global." + def.field);
      }
    } else if (def.scope == Scope::Paired) {
      const auto* common = get(session.common, def.field);
      const auto* left = get(session.left_options, def.field);
      const auto* right = get(session.right_defaults, def.field);
      if (!common && !left && !right) continue;
      const auto l = left ? *left : (common ? *common : L"");
      const auto r = right ? *right : (common ? *common : L"");
      emit(plan, def, l == r ? l : l + L":" + r, "paired." + def.field);
    } else {
      const auto& options = def.scope == Scope::Common ? session.common : (def.scope == Scope::Left ? session.left_options : session.right_defaults);
      if (const auto* value = get(options, def.field)) emit(plan, def, *value, def.key);
    }
  }

  std::vector<const RightInput*> active;
  std::set<std::uint64_t> ids;
  for (const auto& input : session.right_inputs) {
    if (input.id == 0 || !ids.insert(input.id).second || input.id >= session.next_input_id) {
      error(out, "E_INPUT_ID", "right-inputs", L"输入 ID 重复、为零或与下一 ID 不一致。");
    }
    if (input.enabled) active.push_back(&input);
  }
  if (active.empty()) error(out, "E_NO_RIGHT", "right-inputs", L"至少启用一个右侧输入。");
  std::wstring left;
  if (reference(session.left)) {
    if (active.empty() || reference(active.front()->source)) error(out, "E_REFERENCE", "left", L"左侧引用需要一个不引用左侧的首个右侧输入。");
    else left = input_path(active.front()->source, plan.working_directory, "left-reference", out);
  } else left = input_path(session.left, plan.working_directory, "left", out);
  plan.arguments.push_back(L"--");  // Stop option parsing for input paths that begin with a dash.
  plan.arguments.push_back(left);
  for (const auto* input : active) {
    const auto prefix = "right[" + std::to_string(input->id) + "]";
    auto path = reference(input->source) ? left : input_path(input->source, plan.working_directory, prefix, out);
    if (path.find(L"::") != std::wstring::npos) error(out, "E_RIGHT_SEPARATOR", prefix, L"右侧输入含有引擎保留分隔符 ::，无法无损表达。");
    for (const auto& item : input->overrides) {
      if (!find_override(item.first) || find_override(item.first)->key != item.first) {
        error(out, "E_OVERRIDE_KEY", prefix + "." + item.first, L"不支持的逐视频覆盖字段。");
      }
    }
    for (const auto& def : override_catalog()) {
      const auto found = input->overrides.find(def.key);
      if (found == input->overrides.end() || found->second.mode == OverrideMode::Inherit) continue;
      const auto& setting = found->second;
      const auto field = prefix + "." + def.key;
      std::wstring value = setting.mode == OverrideMode::Clear ? L"" : setting.value;
      if (setting.mode != OverrideMode::Replace && setting.mode != OverrideMode::Clear && setting.mode != OverrideMode::Append) {
        error(out, "E_OVERRIDE_MODE", field, L"未知覆盖模式。"); continue;
      }
      if (value.find(L"::") != std::wstring::npos) error(out, "E_RIGHT_SEPARATOR", field, L"逐视频参数值不能含有保留分隔符 ::。");
      if (!valid_value(def.rule, value, def.choices, def.allow_empty)) error(out, "E_VALUE", field, L"逐视频参数值无效或超出范围。");
      if (setting.mode == OverrideMode::Append) {
        if (!def.allow_append || value.empty() || placeholder(value)) {
          error(out, "E_APPEND", field, L"仅滤镜允许追加，且追加内容必须非空且不含 __；专家表达式请使用替换模式。");
        } else if (external || !templates.at("filters").empty()) value = L"__," + value;
      }
      if (!external && placeholder(value)) {
        const auto base = templates.find(def.key);
        if (base != templates.end()) {
          const auto resolved = replace_placeholder(value, base->second, field, out);
          if (placeholder(resolved)) error(out, "E_PLACEHOLDER", field, L"逐视频引用缺少可解析的公共模板。");
        }
      }
      path += L"::" + wide(def.key) + L"=" + value;
      plan.emissions.push_back({def.id, field, plan.arguments.size()});
    }
    plan.arguments.push_back(std::move(path));
  }
  finish(result, std::move(plan), allow_long_command);
  return result;
}

BuildResult build_query(const std::wstring& executable, const std::wstring& working_directory,
                        const std::string& query, std::optional<std::wstring> search) {
  BuildResult result;
  LaunchPlan plan;
  const auto* def = find_option(query);
  if (!def || def->scope != Scope::Query) {
    error(result.diagnostics, "E_QUERY", "query", L"请选择一个受支持的独立查询命令。"); return result;
  }
  if (def->takes_value() != search.has_value()) {
    error(result.diagnostics, "E_QUERY_VALUE", def->id, L"搜索查询需要文本（可为空）；帮助和版本查询不接受值。"); return result;
  }
  if (search && !valid_text(*search)) error(result.diagnostics, "E_VALUE", def->id, L"查询文本包含无效 Unicode。");
  plan.working_directory = path_value(working_directory, {}, "working_directory", result.diagnostics);
  plan.executable = path_value(executable, plan.working_directory, "engine_path", result.diagnostics);
  plan.arguments.push_back(plan.executable);
  emit(plan, *find_option("CLI-060"), std::nullopt, "configuration.automatic");
  emit(plan, *def, search, "query");
  finish(result, std::move(plan));
  return result;
}
}  // namespace launcher
