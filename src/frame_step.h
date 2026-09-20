#pragma once
#include <cstdint>
#include <algorithm>
#include <limits>
#include <map>

namespace frame_step {
// Records actual consecutive decoder output, including frames discarded by
// normal synchronization. A gap in the display buffer is not a one-frame step.
class ReferenceHistory {
 public:
  explicit ReferenceHistory(size_t capacity) : capacity_(std::max<size_t>(1, capacity)) {}
  void reset() { previous_.clear(); last_ = std::numeric_limits<int64_t>::min(); }
  void observe(int64_t pts) {
    if (last_ != std::numeric_limits<int64_t>::min() && pts > last_) {
      previous_[pts] = last_;
      while (previous_.size() > capacity_) previous_.erase(previous_.begin());
    } else previous_.clear();
    last_ = pts;
  }
  bool adjacent(int64_t older, int64_t newer) const {
    const auto found = previous_.find(newer);
    return found != previous_.end() && found->second == older;
  }
 private:
  size_t capacity_;
  int64_t last_ = std::numeric_limits<int64_t>::min();
  std::map<int64_t, int64_t> previous_;
};

// Presentation timestamps, not average durations, define adjacent frames.
// Feed decoded/filtered frames in presentation order after a backward seek.
struct ReferenceSearch {
  int64_t anchor;
  int direction;
  int64_t selected{};
  bool found{false};
  bool done{false};

  void observe(int64_t pts) {
    if (done) return;
    if (direction < 0) {
      if (pts < anchor) { selected = pts; found = true; }
      else done = true;
    } else if (pts > anchor) {
      selected = pts; found = done = true;
    }
  }
};
}  // namespace frame_step
