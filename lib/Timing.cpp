#include <chrono>
#include "Timing.hpp"

Timing::Timing() {
  startTime = std::chrono::steady_clock::now();
}

double Timing::getRunTime(bool cont) {
  auto now = std::chrono::steady_clock::now();
  std::chrono::duration<double, std::milli> t = now - startTime;
  if (!cont) {
    startTime = now;
  }
  return t.count();
}
