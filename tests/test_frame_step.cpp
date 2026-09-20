#include "frame_step.h"
#include "queue.h"
#include <cassert>
#include <iostream>

int main() {
  frame_step::ReferenceHistory history(3);
  for (auto pts : {0, 40000, 100000, 160000}) history.observe(pts);
  assert(history.adjacent(40000, 100000));
  assert(!history.adjacent(0, 100000)); // a skipped display frame is not adjacent
  history.observe(200000);
  assert(!history.adjacent(0, 40000)); // bounded timestamp metadata
  history.reset();
  history.observe(40000);
  assert(!history.adjacent(0, 40000)); // never join across seeks
  frame_step::ReferenceSearch previous{160000, -1};
  for (auto pts : {0, 40000, 100000, 160000, 200000}) previous.observe(pts);
  assert(previous.done && previous.found && previous.selected == 100000);
  frame_step::ReferenceSearch next{40000, 1};
  for (auto pts : {0, 40000, 100000, 160000}) next.observe(pts);
  assert(next.done && next.found && next.selected == 100000);
  frame_step::ReferenceSearch first{0, -1}; first.observe(0);
  assert(first.done && !first.found);
  frame_step::ReferenceSearch last{160000, 1};
  for (auto pts : {0, 40000, 100000, 160000}) last.observe(pts);
  assert(!last.found);
  frame_step::ReferenceSearch rounded{40001, -1};
  for (auto pts : {0, 40000, 40001}) rounded.observe(pts);
  assert(rounded.found && rounded.selected == 40000);
  Queue<int> queue(2);
  int value = -1;
  assert(!queue.try_pop(value) && !queue.is_drained());
  queue.push(7); queue.stop();
  assert(!queue.is_drained());
  assert(queue.try_pop(value) && value == 7 && queue.is_drained());
  queue.restart();
  assert(!queue.is_drained());
  std::cout << "PASS: actual PTS adjacency, irregular durations, boundaries and microsecond precision\n";
}
