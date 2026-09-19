#include "model.h"
#include <stdexcept>

namespace launcher {
std::wstring quote_windows_argument(const std::wstring& argument) {
  if (argument.find(L'\0') != std::wstring::npos) throw std::invalid_argument("NUL in argument.");
  // Always quote: preserves empty strings and trailing slashes, and simplifies auditing.
  std::wstring result = L"\"";
  std::size_t slashes = 0;
  for (wchar_t ch : argument) {
    if (ch == L'\\') { ++slashes; continue; }
    if (ch == L'\"') {
      result.append(slashes * 2 + 1, L'\\');
      result.push_back(ch);
    } else {
      result.append(slashes, L'\\');
      result.push_back(ch);
    }
    slashes = 0;
  }
  result.append(slashes * 2, L'\\');
  result.push_back(L'\"');
  return result;
}

std::wstring make_windows_command_line(const std::vector<std::wstring>& arguments) {
  if (arguments.empty()) throw std::invalid_argument("Missing executable argument.");
  std::wstring result;
  for (const auto& argument : arguments) {
    if (!result.empty()) result.push_back(L' ');
    result += quote_windows_argument(argument);
  }
  return result;
}
}  // namespace launcher
