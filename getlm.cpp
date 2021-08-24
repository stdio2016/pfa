// cl /EHsc /O2 /openmp getlm.cpp Landmark.cpp Database.cpp lib/WavReader.cpp lib/Timing.cpp lib/ReadAudio.cpp lib/BmpReader.cpp lib/Signal.cpp lib/utils.cpp lib/Sound.cpp PeakFinder.cpp PeakFinderDejavu.cpp Analyzer.cpp
#include <cstring>
#include <fstream>
#include <stdio.h>
#include "Analyzer.hpp"
#include "Landmark.hpp"
#include "PeakFinderDejavu.hpp"
int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("Usage: ./getlm file out [REPEAT_COUNT]\n");
    return 1;
  }
  std::string outname;
  if (argc < 3) outname = argv[1] + std::string(".lm");
  else outname = argv[2];

  Analyzer analyzer;
  PeakFinder *peakfinder = new PeakFinderDejavu;
  LandmarkBuilder *lmbuilder = new LandmarkBuilder;
  
  int extractpeak = 0;
  if (argc > 3) {
    if (sscanf(argv[3], "%d", &analyzer.REPEAT_COUNT) == 1) {
      if (analyzer.REPEAT_COUNT > 10 || analyzer.REPEAT_COUNT < 1) {
        printf("REPEAT_COUNT must be 1 ~ 10\n");
        return 1;
      }
    }
    else if (argv[3] == std::string("peaks")) {
      extractpeak = 1;
    }
  }
  
  analyzer.peak_finder = peakfinder;
  analyzer.landmark_builder = lmbuilder;
  
  std::ofstream fout(outname, std::ios::binary);
  if (!fout) {
    printf("Cannot write to file %s\n", argv[2]);
    return 1;
  }
  
  try {
    std::vector<Landmark> lms = analyzer.fingerprint_file(argv[1]);
    printf("File contains %zd landmarks and %zd peaks\n", lms.size(), analyzer.peaks.size());
    
    if (extractpeak) {
      fout.write((const char *)analyzer.peaks.data(), analyzer.peaks.size() * sizeof(Peak));
    }
    else {
      fout.write((const char *)lms.data(), lms.size() * sizeof(lms[0]));
    }
  }
  catch (std::runtime_error &err) {
    printf("%s\n", err.what());
  }
  
  delete peakfinder;
  delete lmbuilder;
}
