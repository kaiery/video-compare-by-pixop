#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include "core/model.h"

namespace launcher::process {
enum class Phase : DWORD { Starting, Running, Stopping, StopTimedOut, Exited, Failed };
struct Status {
  Phase phase{Phase::Starting};
  DWORD pid{}, exit_code{};
  std::wstring message;
};
struct Snapshot : Status {
  bool active{};
  std::wstring tail;
};
std::wstring system_error(DWORD code);
std::wstring exit_description(DWORD code);
std::wstring executable_path();
bool check_engine(const std::wstring& path, std::wstring& description);
std::string utf8(const std::wstring& text);
class Utf8Decoder {
  std::string pending_;
public:
  std::wstring feed(const std::string& bytes, bool final = false);
};

// Owns only the helper handle/events. Destruction never kills an engine: the
// independent helper keeps draining logs when the user elects to leave it running.
class Run {
  HANDLE worker_{}, stop_{}, kill_{};
  std::wstring folder_;
  Snapshot last_;
  void close_handles();
public:
  Run() = default;
  Run(const Run&) = delete;
  Run& operator=(const Run&) = delete;
  ~Run();
  bool active() const;
  bool start(const LaunchPlan& plan, const std::wstring& logs_root, DWORD timeout_ms, std::wstring& error);
  Snapshot poll();
  void request_stop();
  void force_stop();
  const std::wstring& folder() const { return folder_; }
};

// Shared by the independent console-less helper and integration tests.
int run_request(const std::wstring& request_file);
}
