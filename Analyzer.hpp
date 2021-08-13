#pragma once
#include <vector>
#include "Landmark.hpp"
#include "PeakFinder.hpp"

class Analyzer {
public:
  // parameters
  int SAMPLE_RATE = 8000;
  int REPEAT_COUNT = 1;
  
  PeakFinder *peak_finder;
  LandmarkBuilder *landmark_builder;
  
  std::vector<Peak> peaks;
  
  std::vector<Landmark> fingerprint_file(const char *name);
  
  std::vector<Landmark> fingerprint_waveform(const float *wave, size_t len);
};
