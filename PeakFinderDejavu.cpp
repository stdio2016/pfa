#include <algorithm>
#include <cmath>
#include "PeakFinderDejavu.hpp"
#include "lib/Signal.hpp"
#include "lib/utils.hpp"
#include "lib/Timing.hpp"

// use algorithm from https://github.com/worldveil/dejavu
std::vector<Peak> PeakFinderDejavu::find_peaks(const float *sample, size_t len) {
  if (len < FFT_SIZE) {
    LOG_DEBUG("file is too short (%zd samples < FFT_SIZE = %d)", len, FFT_SIZE);
    return std::vector<Peak>();
  }
  
  Timing tm; // to measure run time

  tm.getRunTime();
  double totle = 0.0;
  for (int i = 0; i < len; i++) {
    totle += (double)sample[i] * sample[i];
  }
  totle /= len;
  // only peaks above db_min are considered fingerprints
  double db_min = std::max(log10(totle) * 10 - 60, -99.0);
  // my spectrogram is not normalized
  double dB_offset = log10(FFT_SIZE/2) * 20;
  this->rms = sqrt(totle);
  LOG_DEBUG("get rms %.3fms rms=%.1fdB", tm.getRunTime(), log10(rms) * 20);
  
  tm.getRunTime();
  int nFreq = FFT_SIZE/2 + 1;
  size_t blockn = specgram_num_frames(len, FFT_SIZE, NOVERLAP);
  this->spec = std::vector<double>(blockn * nFreq);
  specgram(sample, len, FFT_SIZE, NOVERLAP, spec.data());
  LOG_DEBUG("fft %.3fms", tm.getRunTime());
  
  tm.getRunTime();
  for (size_t i = 0; i < blockn * nFreq; i++) {
    spec[i] = log10(spec[i] + 1e-10) * 10 - dB_offset;
  }
  LOG_DEBUG("log %.3fms", tm.getRunTime());
  
  tm.getRunTime();
  std::vector<double> local_max = max_filter(spec, blockn, nFreq, PEAK_NEIGHBORHOOD_SIZE_TIME, PEAK_NEIGHBORHOOD_SIZE_FREQ);
  LOG_DEBUG("max filter %.3fms", tm.getRunTime());
  
  tm.getRunTime();
  std::vector<Peak> peaks;
  for (size_t i = 0; i < blockn * nFreq; i++) {
    if (spec[i] == local_max[i] && spec[i] > db_min) {
      Peak peak;
      peak.time = i / nFreq;
      peak.freq = i % nFreq;
      peak.energy = spec[i];
      peaks.push_back(peak);
    }
  }
  LOG_DEBUG("find peak %.3fms", tm.getRunTime());
  return peaks;
}
