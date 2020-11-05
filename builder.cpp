#include <cmath>
#include <stdio.h>
#include <algorithm>
#include <string>
#include <stdexcept>
#include <fstream>
#include "lib/ReadAudio.hpp"
#include "lib/Timing.hpp"
#include "lib/Signal.hpp"
#include "Landmark.hpp"

// use algorithm from https://github.com/worldveil/dejavu
void processMusic(std::string name, LandmarkBuilder builder) {
  Timing tm;
  try {
    Sound snd = ReadAudio(name.c_str());
    #pragma omp master
    printf("read file %.3fms\n", tm.getRunTime());

    tm.getRunTime();
    size_t len = snd.length();
    int channels = snd.numberOfChannels();
    for (int i = 1; i < channels; i++) {
      for (int j = 0; j < len; j++)
        snd.d[0][j] += snd.d[i][j];
    }
    for (int i = 0; i < len; i++) {
      snd.d[0][i] *= 1.0 / channels;
    }
    snd.d.resize(1);
    #pragma omp master
    printf("stereo to mono %.3fms\n", tm.getRunTime());

    tm.getRunTime();
    channels = 1;
    double rate = (double)snd.sampleRate / (double)builder.SAMPLE_RATE;
    for (int i = 0; i < channels; i++) {
      //if (rate > 1)
      //  snd.d[i] = lopass(snd.d[i], 1.0/rate, 50);
      snd.d[i] = resample(snd.d[i], snd.sampleRate, builder.SAMPLE_RATE);
      //if (rate < 1)
      //  snd.d[i] = lopass(snd.d[i], rate, 50);
    }
    len = snd.length();
    #pragma omp master
    printf("resample %.3fms\n", tm.getRunTime());
    
    std::vector<Peak> peaks = builder.find_peaks(snd.d[0]);
    
    tm.getRunTime();
    std::vector<Landmark> lms = builder.peaks_to_landmarks(peaks);
    #pragma omp master
    printf("create landmark pairs %.3fms\n", tm.getRunTime());
    
    std::string bmpName = name.substr(0, name.size()-4) + "_spec.bmp";
    builder.drawSpecgram(bmpName.c_str(), peaks);
    
    std::string shortname = name;
    if (shortname.find('/') != shortname.npos) {
      shortname = shortname.substr(shortname.find_last_of('/')+1, -1);
    }
    #pragma omp critical
    printf("compute %s rms=%.2fdB peak=%d landmarks=%d\n", shortname.c_str(),
      log10(builder.rms) * 20, (int)peaks.size(), (int)lms.size());
  }
  catch (std::runtime_error x) {
    printf("%s\n", x.what());
  }
}

int main(int argc, char const *argv[]) {
  if (argc < 2) {
    printf("Usage: ./a.out <music list file>\n");
    return 1;
  }
  std::ifstream flist(argv[1]);
  if (!flist) {
    printf("cannot read music list!\n");
    return 1;
  }
  std::string line;
  std::vector<std::string> filenames;
  while (std::getline(flist, line)) {
    filenames.push_back(line);
  }
  flist.close();
  
  Timing timing;
  LandmarkBuilder builder;
  #pragma omp parallel for schedule(dynamic) firstprivate(builder)
  for (int i = 0; i < filenames.size(); i++) {
    std::string name = filenames[i];
    printf("File: %s\n", name.c_str());
    processMusic(name, builder);
  }
  printf("Total time: %.3fs\n", timing.getRunTime() * 0.001);
  return 0;
}
