#define SDL_MAIN_HANDLED
#include "video_compare.h"
#include <SDL2/SDL.h>
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <iterator>
#include <mutex>
#include <sstream>
#include <thread>

// Uses SDL's private dummy-driver event queue and clipboard, never OS input.
class Output : public std::streambuf {
 public:
  std::mutex mutex;
  std::condition_variable changed;
  std::string data;
  std::streamsize xsputn(const char* s, std::streamsize n) override {
    std::lock_guard<std::mutex> lock(mutex);
    data.append(s, static_cast<size_t>(n)); changed.notify_all(); return n;
  }
  int overflow(int c) override { if (c != EOF) { const char ch = static_cast<char>(c); xsputn(&ch, 1); } return c; }
  size_t size() { std::lock_guard<std::mutex> lock(mutex); return data.size(); }
  size_t count_since(size_t offset, const std::string& marker) {
    std::lock_guard<std::mutex> lock(mutex);
    size_t count = 0;
    while ((offset = data.find(marker, offset)) != std::string::npos) { ++count; offset += marker.size(); }
    return count;
  }
  std::string wait_line(size_t offset, const std::string& marker) {
    std::unique_lock<std::mutex> lock(mutex);
    size_t start = std::string::npos, end = std::string::npos;
    if (!changed.wait_for(lock, std::chrono::seconds(5), [&] {
      start = data.find(marker, offset);
      if (start != std::string::npos) end = data.find('\n', start);
      return end != std::string::npos;
    })) throw std::runtime_error("Timed out waiting for " + marker);
    return data.substr(start + marker.size(), end - start - marker.size());
  }
};

static void key(SDL_Keycode code, SDL_Keymod mod = KMOD_NONE) {
  SDL_Event e{}; e.type = SDL_KEYDOWN; e.key.keysym.sym = code; e.key.keysym.mod = mod;
  if (SDL_PushEvent(&e) < 0) throw std::runtime_error(SDL_GetError());
}
static double position(Output& output) {
  const auto offset = output.size(); key(SDLK_c, KMOD_CTRL);
  const auto line = output.wait_line(offset, "Copied to clipboard: ");
  int h{}, m{}; double s{};
  if (std::sscanf(line.c_str(), "%d:%d:%lf", &h, &m, &s) != 3) throw std::runtime_error("Invalid timestamp: " + line);
  return h * 3600 + m * 60 + s;
}
static void expect_position(Output& output, double expected) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(6);
  double got = -1;
  do {
    got = position(output);
    if (std::abs(got - expected) < 0.0011) return;
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  } while (std::chrono::steady_clock::now() < deadline);
  throw std::runtime_error("Expected frame time " + std::to_string(expected) + ", got " + std::to_string(got));
}
static std::string read_file(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) throw std::runtime_error("Cannot read " + path);
  return std::string(std::istreambuf_iterator<char>(in), {});
}
static std::pair<std::string, std::string> picture(Output& output) {
  if (std::getenv("FRAME_STEP_SKIP_PNG")) return {};
  const auto offset = output.size(); key(SDLK_f);
  const auto line = output.wait_line(offset, "Saved ");
  const auto comma = line.find(", "), and_pos = line.find(" and ", comma + 2);
  if (comma == std::string::npos || and_pos == std::string::npos) throw std::runtime_error("Invalid PNG message: " + line);
  return {read_file(line.substr(0, comma)), read_file(line.substr(comma + 2, and_pos - comma - 2))};
}

int main(int argc, char** argv) {
  if (argc < 5) { std::cerr << "Usage: integration_frame_step BUFFER_SIZE PTS_FILE LEFT RIGHT [RIGHT...]\n"; return 2; }
  std::vector<double> times;
  std::ifstream pts(argv[2]); double value;
  while (pts >> value) times.push_back(value);
  if (times.size() < 8) { std::cerr << "Need at least eight reference timestamps\n"; return 2; }
  SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
  SDL_setenv("SDL_RENDER_DRIVER", "software", 1);
#ifdef _WIN32
  _putenv_s("VIDEO_COMPARE_LOG_FRAME_STEP", "1");
#else
  setenv("VIDEO_COMPARE_LOG_FRAME_STEP", "1", 1);
#endif
  VideoCompareConfig config;
  config.window_size = std::make_tuple(320, 180);
  config.frame_buffer_size = std::stoul(argv[1]);
  config.disable_auto_filters = true;
  config.left.file_name = argv[3];
  if (std::getenv("FRAME_STEP_TRUST_PTS")) av_dict_set(&config.left.decoder_options, "trust_dec_pts", "1", 0);
  for (int i = 4; i < argc; ++i) {
    InputVideo input; input.side = Side::Right(i - 4); input.file_name = argv[i]; input.side_description = "Right";
    config.right_videos.push_back(input);
  }
  Output output;
  auto* previous = std::cout.rdbuf(&output);
  std::string error;
  std::vector<double> reverse_ms;
  size_t reverse_seeks = 0;
  std::thread driver;
  std::atomic<bool> finished{false};
  std::thread watchdog([&] {
    for (int i = 0; i < 1200 && !finished; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(100));
    if (!finished) { std::cerr << "FAIL: navigation watchdog expired\n"; std::_Exit(3); }
  });
  try {
    VideoCompare compare(config);
    driver = std::thread([&] {
      try {
        expect_position(output, times.back()); // Run naturally through EOF.
        const auto boundary_start = output.size();
        key(SDLK_d); // An extra forward step must hold the last frame and pause.
        output.wait_line(boundary_start, "[frame-step] idle");
        expect_position(output, times.back());
        std::vector<std::pair<std::string, std::string>> reverse(times.size());
        reverse.back() = picture(output);
        const auto reverse_start = output.size();
        for (size_t n = times.size() - 1; n > 0; --n) {
          const auto started = std::chrono::steady_clock::now();
          key(SDLK_a, n % 2 ? KMOD_NONE : KMOD_SHIFT);
          expect_position(output, times[n - 1]);
          reverse_ms.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count());
          reverse[n - 1] = picture(output);
        }
        reverse_seeks = output.count_since(reverse_start, "[frame-step] seek");
        if (const char* limit = std::getenv("FRAME_STEP_MAX_REVERSE_SEEKS")) {
          if (reverse_seeks > std::stoul(limit)) throw std::runtime_error("Reverse navigation exceeded seek budget: " + std::to_string(reverse_seeks));
        }
        key(SDLK_a); std::this_thread::sleep_for(std::chrono::milliseconds(200));
        expect_position(output, times.front());
        for (size_t n = 1; n < times.size(); ++n) {
          key(SDLK_d, n % 2 ? KMOD_SHIFT : KMOD_NONE);
          expect_position(output, times[n]);
          if (picture(output) != reverse[n]) throw std::runtime_error("Forward/reverse PNG mismatch at " + std::to_string(n));
        }
        // Resume from an older displayed frame, not the most recent decoder head.
        for (int i = 0; i < 6; ++i) { key(SDLK_a); expect_position(output, times[times.size() - 2 - i]); }
        key(SDLK_SPACE);
        expect_position(output, times.back());
        if (argc > 5) { key(SDLK_TAB); std::this_thread::sleep_for(std::chrono::milliseconds(100)); }
        key(SDLK_a); expect_position(output, times[times.size() - 2]);
      } catch (const std::exception& e) { error = e.what(); }
      SDL_Event quit{}; quit.type = SDL_QUIT; SDL_PushEvent(&quit);
    });
    compare();
  } catch (const std::exception& e) { error = e.what(); }
  if (driver.joinable()) driver.join();
  finished = true; watchdog.join();
  std::cout.rdbuf(previous);
  if (!reverse_ms.empty()) {
    double total = 0; for (double ms : reverse_ms) total += ms;
    const double first = reverse_ms.front();
    std::sort(reverse_ms.begin(), reverse_ms.end());
    std::cout << "REVERSE: steps=" << reverse_ms.size() << " seeks=" << reverse_seeks << " first_ms=" << first
              << " mean_ms=" << total / reverse_ms.size() << " p50_ms=" << reverse_ms[reverse_ms.size()/2]
              << " p95_ms=" << reverse_ms[(reverse_ms.size()-1)*95/100] << " max_ms=" << reverse_ms.back() << '\n';
  }
  if (!error.empty()) { std::cerr << "FAIL: " << error << '\n' << output.data; return 1; }
  std::cout << "PASS: " << times.size() << " frames EOF-to-start-to-EOF; A/D and Shift aliases, buffer=" << config.frame_buffer_size
            << (std::getenv("FRAME_STEP_SKIP_PNG") ? ", per-frame timestamps match (PNG comparison disabled)" : ", per-frame timestamps and both PNGs match")
            << "; endpoints, resume and right switching checked\n";
  return 0;
}
