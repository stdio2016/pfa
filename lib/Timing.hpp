/**
 * Provides some timing functions
 */
#pragma once
#ifndef TIMING_HPP
#define TIMING_HPP
#include <chrono>

class Timing {
public:
  // automatically record start time
  Timing();
  
  // get time in milliseconds
  double getRunTime(bool cont=false);

private:
  std::chrono::steady_clock::time_point startTime;
};

#endif
