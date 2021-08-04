#include "PeakFinder.hpp"

// use algorithm from https://github.com/worldveil/dejavu
class PeakFinderDejavu : public PeakFinder {
  // parameters inherited from PeakFinder
  // int FFT_SIZE = 1024;
  // int NOVERLAP = FFT_SIZE * 0.5;
public:
  // parameters just for PeakFinderDejavu
  int PEAK_NEIGHBORHOOD_SIZE_FREQ = 10;
  int PEAK_NEIGHBORHOOD_SIZE_TIME = 5;
  
  double rms;
  
  std::vector<Peak> find_peaks(const float *sample, size_t len) override;
  
  PeakFinderDejavu *clone() override {
    return new PeakFinderDejavu(*this);
  }
  
  virtual ~PeakFinderDejavu() {}
};
