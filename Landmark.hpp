#pragma once
// use algorithm from https://github.com/worldveil/dejavu
#include <vector>
#include <stdio.h>
#include <cstdint>

struct Peak {
  int32_t time;
  int32_t freq;
  float energy;
};

struct Landmark {
  int32_t time1;
  int32_t freq1;
  float energy1;
  int32_t time2;
  int32_t freq2;
  float energy2;
};

class LandmarkBuilder {
public:
  // parameters
  int SAMPLE_RATE = 8820; // 44100 / 5
  int FFT_SIZE = 1024;
  int FAN_COUNT = 8;
  int NOVERLAP = FFT_SIZE * 0.5;
  int PEAK_NEIGHBORHOOD_SIZE_FREQ = 10;
  int PEAK_NEIGHBORHOOD_SIZE_TIME = 5;
  int LM_DT_MAX = 35;
  int LM_DT_MIN = 6;
  int LM_DF_MAX = 127;
  
  std::vector<Peak> find_peaks(const std::vector<float> &sample);
  
  std::vector<Landmark> peaks_to_landmarks(const std::vector<Peak> &peaks);
  
  // intermediate results
  double rms;
  std::vector<double> spec;
  
  void drawSpecgram(const char *name, std::vector<Peak> peaks);
};
