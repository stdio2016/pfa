#include <algorithm>
#include "PeakFinder.hpp"
#include "lib/BmpReader.h"
#include "lib/utils.hpp"
#include "lib/Timing.hpp"

static void getOldColor(float db, unsigned char *bmp) {
  bmp[0] = 0;
  bmp[2] = 0;
  if (db > -22) {
    bmp[1] = 255;
  }
  else if (db > -92) {
    bmp[1] = (db+92)*(db+92) / 4900 * 255;
  }
  else {
    bmp[1] = 0;
  }
}

void PeakFinder::drawSpecgram(const char *name, std::vector<Peak> peaks) {
  Timing tm;
  int nFreq = FFT_SIZE/2 + 1;
  int blockn = spec.size() / nFreq;
  std::vector<unsigned char> bmp(blockn * nFreq * 3);
  for (int i = 0; i < blockn; i++) {
    for (int j = 0; j < nFreq; j++) {
      double db = spec[i * nFreq + j];
      int idx = (i + j * blockn) * 3;
      getOldColor(db, &bmp[idx]);
    }
  }
  for (int i = 0; i < peaks.size(); i++) {
    int t = peaks[i].time;
    int f = peaks[i].freq;
    for (int y = std::max(0, f-2); y < std::min(nFreq, f+3); y++) {
      for (int x = std::max(0, t-2); x < std::min(blockn, t+3); x++) {
        if (abs(t-x) == 2 || abs(f-y) == 2) {
          int idx = (x + y * blockn) * 3;
          bmp[idx+0]=0;
          bmp[idx+1]=0;
          bmp[idx+2]=255;
        }
      }
    }
  }
  BmpReader br;
  br.WriteBMP(name, blockn, nFreq, bmp.data());
  LOG_DEBUG("write spectrogram %.2fms", tm.getRunTime());
}
