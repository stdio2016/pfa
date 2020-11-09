#include <cstdio>
#include <vector>
#include <algorithm>
#include "lib/fft.hpp"
#include "lib/Signal.hpp"
#include "lib/BmpReader.h"
#include "lib/Timing.hpp"
#include "Landmark.hpp"

template<typename T>
T *image_padding(const T *in, int height, int width, int hpad, int wpad) {
  int w = width + wpad*2;
  T *out = new T[(width+wpad*2) * (height+hpad*2)];
  for (int i = 0; i < height; i++) {
    for (int j = 0; j < width; j++) {
      out[w * (i+hpad) + j+wpad] = in[width * i + j];
    }
    for (int j = 0; j < std::min(width, wpad); j++) {
      out[w * (i+hpad) + wpad-1-j] = in[width * i + j];
    }
    for (int j = 0; j < std::min(width, wpad); j++) {
      out[w * (i+hpad) + width+wpad+j] = in[width * i + width-1-j];
    }
    for (int j = std::min(width, wpad); j < wpad; j++) {
      out[w * (i+hpad) + wpad-1-j] = in[width * i + width-1];
    }
    for (int j = std::min(width, wpad); j < wpad; j++) {
      out[w * (i+hpad) + width+wpad+j] = in[width * i + 0];
    }
  }
  for (int i = 0; i < hpad; i++) {
    int row = hpad-1 - i;
    if (row >= height) row = height-1;
    for (int j = 0; j < width+wpad*2; j++) {
      out[w * i + j] = out[w * (row+hpad) + j];
    }
  }
  for (int i = 0; i < hpad; i++) {
    int row = height-1 - i;
    if (row < 0) row = 0;
    for (int j = 0; j < width+wpad*2; j++) {
      out[w * (height+hpad+i) + j] = out[w * (row+hpad) + j];
    }
  }
  return out;
}

template<typename T>
std::vector<T> max_filter(const std::vector<T> &in, int height, int width, int kh, int kw) {
  T *pad = image_padding(in.data(), height, width, kh, kw);
  T *rowmax = new T[(height + kh*2) * width];
  // row max
  int k = 1 + kw*2;
  T *tmp = new T[std::max((1+kw*2)*2, width * (1+kh*2)*2)];
  for (int y = 0; y < height + kh*2; y++) {
    T *inrow = pad + y * (width+kw*2);
    T *outrow = rowmax + y * width;
    for (int x = 0; x < width; x += k-1) {
      tmp[k-1] = inrow[x+k-2];
      for (int j = 1; j < k-1; j++) {
        tmp[k-1-j] = std::max(tmp[k-j], inrow[x+k-2-j]);
      }
      tmp[k] = inrow[x+k-1];
      int to = std::min(k-1, width-x);
      for (int j = 1; j < to; j++) {
        tmp[k+j] = std::max(tmp[k-1+j], inrow[x+k-1+j]);
      }
      for (int j = 0; j < to; j++) {
        outrow[x+j] = std::max(tmp[1+j], tmp[k+j]);
      }
    }
  }
  
  // column max
  k = 1 + kh*2;
  std::vector<T> out(width * height);
  for (int y = 0; y < height; y += k-1) {
    for (int x = 0; x < width; x++)
      tmp[(k-1) * width + x] = rowmax[(y+k-2) * width + x];
    for (int j = 1; j < k-1; j++) {
      for (int x = 0; x < width; x++)
        tmp[(k-1-j) * width + x] = std::max(tmp[(k-j) * width + x], rowmax[(y+k-2-j) * width + x]);
    }
    for (int x = 0; x < width; x++)
      tmp[k * width + x] = rowmax[(y+k-1) * width + x];
    int to = std::min(k-1, height-y);
    for (int j = 1; j < to; j++) {
      for (int x = 0; x < width; x++)
        tmp[(k+j) * width + x] = std::max(tmp[(k-1+j) * width + x], rowmax[(y+k-1+j) * width + x]);
    }
    for (int j = 0; j < to; j++) {
      for (int x = 0; x < width; x++)
        out[(y+j) * width + x] = std::max(tmp[(1+j) * width + x], tmp[(k+j) * width + x]);
    }
  }
  delete[] tmp;
  delete[] rowmax;
  delete[] pad;
  return out;
}

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

// use algorithm from https://github.com/worldveil/dejavu
std::vector<Peak> LandmarkBuilder::find_peaks(const std::vector<float> &sample) {
  Timing tm; // to measure run time
  int len = sample.size();

  tm.getRunTime();
  double totle = 0.0;
  for (int i = 0; i < len; i++) {
    totle += (double)sample[i] * sample[i];
  }
  totle /= len;
  // only peaks above db_min are considered fingerprints
  double db_min = log10(totle) * 10 - 60;
  this->rms = sqrt(totle);
  if (log_file)
    fprintf(log_file, "get rms %.3fms\n", tm.getRunTime());
  
  tm.getRunTime();
  FFT<double> fft(FFT_SIZE/2);
  // hann window
  std::vector<double> window(FFT_SIZE);
  for (int i = 0; i < FFT_SIZE; i++) {
    window[i] = 0.5 * (1 - cos(2 * 3.14159265359 * i / FFT_SIZE));
  }
  
  std::vector<double> out(FFT_SIZE), in(FFT_SIZE);
  int nFreq = FFT_SIZE/2 + 1;
  this->spec = std::vector<double>((len-NOVERLAP) / (FFT_SIZE-NOVERLAP) * nFreq);
  int blockn = 0;
  for (int i = 0; i + FFT_SIZE <= len; i += FFT_SIZE-NOVERLAP) {
    for (int j = 0; j < FFT_SIZE; j++) {
      in[j] = sample[i+j] * window[j];
    }
    fft.realFFT(in.data(), out.data());
    spec[0 + nFreq * blockn] = out[0] * out[0] + 1e-10;
    spec[FFT_SIZE/2 + nFreq * blockn] = out[1] * out[1] + 1e-10;
    for (int j = 2; j < FFT_SIZE; j+=2) {
      spec[j/2 + nFreq * blockn] = out[j] * out[j] + out[j+1] * out[j+1] + 1e-10;
    }
    blockn += 1;
  }
  if (log_file)
    fprintf(log_file, "fft %.3fms\n", tm.getRunTime());
  
  tm.getRunTime();
  for (int i = 0; i < blockn * nFreq; i++) {
    spec[i] = log10(spec[i]) * 10;
  }
  if (log_file)
    fprintf(log_file, "log %.3fms\n", tm.getRunTime());
  
  tm.getRunTime();
  std::vector<double> local_max = max_filter(spec, blockn, nFreq, PEAK_NEIGHBORHOOD_SIZE, PEAK_NEIGHBORHOOD_SIZE);
  if (log_file)
    fprintf(log_file, "max filter %.3fms\n", tm.getRunTime());
  
  tm.getRunTime();
  std::vector<Peak> peaks;
  for (int i = 0; i < blockn * nFreq; i++) {
    if (spec[i] == local_max[i] && spec[i] > db_min) {
      Peak peak;
      peak.time = i / nFreq;
      peak.freq = i % nFreq;
      peaks.push_back(peak);
    }
  }
  if (log_file)
    fprintf(log_file, "find peak %.3fms\n", tm.getRunTime());
  return peaks;
}

// use algorithm from https://github.com/worldveil/dejavu
std::vector<Landmark> LandmarkBuilder::peaks_to_landmarks(const std::vector<Peak> &peaks) {
  std::vector<Landmark> lms;
  for (int i = 0; i < peaks.size(); i++) {
    std::vector<Landmark> lm_to_sort;
    for (int j = i+1; j < peaks.size(); j++) {
      //if (j-i > FAN_COUNT) break;
      Landmark lm;
      lm.time1 = peaks[i].time;
      lm.freq1 = peaks[i].freq;
      lm.time2 = peaks[j].time;
      lm.freq2 = peaks[j].freq;
      int dt = lm.time2 - lm.time1;
      int df = lm.freq2 - lm.freq1;
      if (lm.time2 - lm.time1 <= 40) {
        if (lm.time2 > lm.time1 && abs(df) < 256) {
          int dist = dt * dt * 20 + df * df;
          lm.time1 = dist;
          lm_to_sort.push_back(lm);
        }
      }
      else break;
    }
    std::sort(lm_to_sort.begin(), lm_to_sort.end(), [](Landmark a, Landmark b) {
      return a.time1 < b.time1;
    });
    int cnt = 0;
    for (Landmark lm : lm_to_sort) {
      lm.time1 = peaks[i].time;
      lms.push_back(lm);
      cnt += 1;
      if (cnt >= FAN_COUNT) break;
    }
  }
  return lms;
}

void LandmarkBuilder::drawSpecgram(const char *name, std::vector<Peak> peaks) {
  Timing tm;
  int nFreq = FFT_SIZE/2 + 1;
  int blockn = spec.size() / nFreq;
  std::vector<unsigned char> bmp(blockn * nFreq * 3);
  for (int i = 0; i < blockn; i++) {
    double dbOffset = log10(FFT_SIZE / 2) * 20;
    for (int j = 0; j < nFreq; j++) {
      double db = spec[i * nFreq + j] - dbOffset;
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
  if (log_file)
    fprintf(log_file, "write spectrogram %.2fms\n", tm.getRunTime());
}
