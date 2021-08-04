#pragma once
#include <cstdint>
#include <vector>
#include "Landmark.hpp"

class PeakFinder {
public:
  // parameters
  int FFT_SIZE = 1024;
  int NOVERLAP = FFT_SIZE * 0.5;
  
  // computed spectrogram for convenience
  std::vector<double> spec;
  
  virtual std::vector<Peak> find_peaks(const float *sample, size_t len) = 0;
  
  virtual void drawSpecgram(const char *name, std::vector<Peak> peaks);
  
  // multithread need this
  virtual PeakFinder *clone() = 0;
  
  virtual ~PeakFinder() {}
};
