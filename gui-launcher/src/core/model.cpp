#include "model.h"
#include "catalog.h"
#include <algorithm>
#include <filesystem>
#include <limits>
#include <stdexcept>

namespace launcher {
Session make_session(const std::wstring& directory) {
  const std::filesystem::path root(directory);
  if (!root.is_absolute()) throw std::invalid_argument("Launcher directory must be absolute.");
  Session session;
  session.working_directory = (root / L"work").lexically_normal().wstring();
  return session;
}

std::uint64_t add_right(Session& session, InputSource source) {
  if (session.next_input_id == 0 || session.next_input_id == std::numeric_limits<std::uint64_t>::max()) {
    throw std::overflow_error("Right-input IDs exhausted.");
  }
  for (const auto& input : session.right_inputs) {
    if (input.id >= session.next_input_id) throw std::invalid_argument("Invalid next input ID in session.");
  }
  const auto id = session.next_input_id++;
  session.right_inputs.push_back({id, true, std::move(source), {}});
  return id;
}

bool move_right(Session& session, std::uint64_t id, std::size_t destination) {
  auto& inputs = session.right_inputs;
  if (destination >= inputs.size()) return false;
  const auto found = std::find_if(inputs.begin(), inputs.end(), [id](const auto& i) { return i.id == id; });
  if (found == inputs.end()) return false;
  const auto origin = static_cast<std::size_t>(found - inputs.begin());
  if (origin < destination) std::rotate(found, found + 1, inputs.begin() + destination + 1);
  else if (origin > destination) std::rotate(inputs.begin() + destination, found, found + 1);
  return true;
}

bool remove_right(Session& session, std::uint64_t id) {
  auto& inputs = session.right_inputs;
  const auto found = std::find_if(inputs.begin(), inputs.end(), [id](const auto& i) { return i.id == id; });
  if (found == inputs.end()) return false;
  inputs.erase(found);
  return true;
}

std::uint64_t duplicate_right(Session& session, std::uint64_t id) {
  const auto found = std::find_if(session.right_inputs.begin(), session.right_inputs.end(),
                                 [id](const auto& i) { return i.id == id; });
  if (found == session.right_inputs.end()) throw std::invalid_argument("Input ID not found.");
  auto duplicate = *found;
  const auto new_id = add_right(session, duplicate.source);
  duplicate.id = new_id;
  session.right_inputs.back() = std::move(duplicate);
  return new_id;
}

bool set_option(Session& session, const std::string& option, OptionValue value, std::wstring& error) {
  error.clear();
  const auto* def = find_option(option);
  if (!def) { error = L"未知选项。"; return false; }
  if (def->scope == Scope::Query) { error = L"查询选项需使用独立查询入口。"; return false; }
  if (def->takes_value() != std::holds_alternative<std::wstring>(value)) {
    error = L"选项值类型错误：开关使用 bool，其余使用文本。"; return false;
  }
  if (def->scope == Scope::Global) { session.global[def->field] = std::move(value); return true; }
  if (def->scope == Scope::Config) {
    if (def->key == "options-file") session.configuration.files.push_back(std::get<std::wstring>(value));
    else session.configuration.load_automatic_file = !std::get<bool>(value);
    return true;
  }
  const auto& text = std::get<std::wstring>(value);
  if (def->scope == Scope::Paired) {
    const auto colon = text.find(L':');
    if (colon != std::wstring::npos && text.find(L':', colon + 1) != std::wstring::npos) {
      error = L"左右参数最多包含一个分隔冒号。"; return false;
    }
    // A CLI assignment replaces the entire pair. Direct per-side UI edits use the maps.
    session.common.erase(def->field);
    session.left_options.erase(def->field);
    session.right_defaults.erase(def->field);
    if (colon == std::wstring::npos) session.common[def->field] = text;
    else {
      session.left_options[def->field] = text.substr(0, colon);
      session.right_defaults[def->field] = text.substr(colon + 1);
    }
  } else if (def->scope == Scope::Common) session.common[def->field] = text;
  else if (def->scope == Scope::Left) session.left_options[def->field] = text;
  else if (def->scope == Scope::Right) session.right_defaults[def->field] = text;
  return true;
}

bool unset_option(Session& session, const std::string& option) {
  const auto* def = find_option(option);
  if (!def || def->scope == Scope::Query) return false;
  switch (def->scope) {
    case Scope::Global: session.global.erase(def->field); break;
    case Scope::Common: session.common.erase(def->field); break;
    case Scope::Left: session.left_options.erase(def->field); break;
    case Scope::Right: session.right_defaults.erase(def->field); break;
    case Scope::Paired:
      session.common.erase(def->field);
      session.left_options.erase(def->field);
      session.right_defaults.erase(def->field);
      break;
    case Scope::Config:
      if (def->key == "options-file") session.configuration.files.clear();
      else session.configuration.load_automatic_file = false;  // Restore GUI-managed default.
      break;
    default: break;
  }
  return true;
}
}  // namespace launcher
