#include <cmath>
#include <stdio.h>
#include <algorithm>
#include <string>
#include <stdexcept>
#include <fstream>
#include "ReadAudio.hpp"
#include "WavReader.hpp"
#include "Timing.hpp"
#include "fft.hpp"
#include "BmpReader.h"

std::vector<float> lopass(const std::vector<float> &snd, double ratio, int firSize) {
  std::vector<float> fir(firSize*2+1);
  for (int i = -firSize; i <= firSize; i++) {
    if (i == 0)
      fir[firSize] = ratio;
    else {
      double x = 3.14159 * ratio * i;
      fir[i+firSize] = sin(x) / x * ratio;
    }
  }
  size_t len = snd.size();
  std::vector<float> out(len + firSize*2);
  for (int i = 0; i < len; i++) {
    for (int j = 0; j <= firSize*2; j++) {
      out[i+j] += snd[i] * fir[j];
    }
  }
  std::copy(&out[firSize], &out[len+firSize], out.begin());
  out.resize(len);
  return out;
}

std::vector<float> resample(const std::vector<float> &snd, int from, int to) {
  size_t len = snd.size();
  std::vector<float> out;
  out.reserve((double)len * from / to + 10);
  size_t t = 0, rem = 0;
  size_t step = from / to;
  size_t remstep = from % to;
  double to_1div = 1.0 / to;
  while (t < len) {
    double frac = (double)rem * to_1div;
    if (t >= len-1) {
      out.push_back(snd[len-1]);
    }
    else {
      out.push_back((1.0-frac) * snd[t] + frac * snd[t+1]);
    }
    t += step;
    rem += remstep;
    if (rem >= to) {
      rem -= to;
      t += 1;
    }
  }
  return out;
}

template<typename T>
T *image_padding(T *in, int height, int width, int hpad, int wpad) {
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
T *max_filter(T *in, int height, int width, int kh, int kw) {
  T *pad = image_padding(in, height, width, kh, kw);
  T *rowmax = new T[(height + kh*2) * width];
  // row max
  int k = 1 + kw*2;
  T *tmp = new T[k*2];
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
  delete[] tmp;
  
  // column max
  k = 1 + kh*2;
  tmp = new T[width * k*2];
  T *out = new T[width * height];
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

void getOldColor(float db, unsigned char *bmp) {
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
void processMusic(std::string name) {
  const int SAMPLE_RATE = 8820; // 44100 / 5
  const int FFT_SIZE = 1024;
  const int FAN_COUNT = 5;
  const int NOVERLAP = FFT_SIZE * 0.4;
  const int PEAK_NEIGHBORHOOD_SIZE = 10;
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
    double rate = (double)snd.sampleRate / (double)SAMPLE_RATE;
    for (int i = 0; i < channels; i++) {
      //if (rate > 1)
      //  snd.d[i] = lopass(snd.d[i], 1.0/rate, 50);
      snd.d[i] = resample(snd.d[i], snd.sampleRate, SAMPLE_RATE);
      //if (rate < 1)
      //  snd.d[i] = lopass(snd.d[i], rate, 50);
    }
    len = snd.length();
    #pragma omp master
    printf("resample %.3fms\n", tm.getRunTime());
    
    tm.getRunTime();
    double totle = 0.0;
    for (int i = 0; i < len; i++) {
      totle += (double)snd.d[0][i] * snd.d[0][i];
    }
    totle /= len;
    double db_min = log10(totle) * 10 - 60;
    printf("get rms %.3fms\n", tm.getRunTime());
    
    tm.getRunTime();
    FFT<double> fft(FFT_SIZE/2);
    std::vector<double> window(FFT_SIZE);
    for (int i = 0; i < FFT_SIZE; i++) {
      window[i] = 0.5 * (1 - cos(2 * 3.14159265359 * i / FFT_SIZE));
    }
    std::vector<double> out(FFT_SIZE), in(FFT_SIZE);
    int nFreq = FFT_SIZE/2 + 1;
    std::vector<double> spec((len-NOVERLAP) / (FFT_SIZE-NOVERLAP) * nFreq);
    int blockn = 0;
    int att=0, atf=0;
    for (int i = 0; i + FFT_SIZE <= len; i += FFT_SIZE-NOVERLAP) {
      for (int j = 0; j < FFT_SIZE; j++) {
        in[j] = snd.d[0][i+j] * window[j];
      }
      fft.realFFT(in.data(), out.data());
      spec[0 + nFreq * blockn] = out[0] * out[0] + 1e-10;
      spec[FFT_SIZE/2 + nFreq * blockn] = out[1] * out[1] + 1e-10;
      for (int j = 2; j < FFT_SIZE; j+=2) {
        spec[j/2 + nFreq * blockn] = out[j] * out[j] + out[j+1] * out[j+1] + 1e-10;
      }
      blockn += 1;
    }
    #pragma omp master
    printf("fft %.3fms\n", tm.getRunTime());
    
    tm.getRunTime();
    for (int i = 0; i < blockn * nFreq; i++) {
      spec[i] = log10(spec[i]) * 10;
    }
    #pragma omp master
    printf("log %.3fms\n", tm.getRunTime());
    
    tm.getRunTime();
    double *local_max = max_filter(spec.data(), blockn, nFreq, PEAK_NEIGHBORHOOD_SIZE, PEAK_NEIGHBORHOOD_SIZE);
    #pragma omp master
    printf("max filter %.3fms\n", tm.getRunTime());
    
    tm.getRunTime();
    std::vector<int> peaks;
    for (int i = 0; i < blockn * nFreq; i++) {
      if (spec[i] == local_max[i] && spec[i] > db_min) {
        peaks.push_back(i / nFreq);
        peaks.push_back(i % nFreq);
      }
    }
    #pragma omp master
    printf("find peak %.3fms\n", tm.getRunTime());
    
    tm.getRunTime();
    int lms = 0;
    for (int i = 0; i < peaks.size()/2; i++) {
      for (int j = i+1; j < peaks.size()/2; j++) {
        if (j-i > FAN_COUNT) break;
        int t1 = peaks[i*2];
        int f1 = peaks[i*2+1];
        int t2 = peaks[j*2];
        int f2 = peaks[j*2+1];
        if (t2 - t1 <= 200 && t2 - t1 >= 0) {
          lms += 1;
        }
      }
    }
    #pragma omp master
    printf("create landmark pairs %.3fms\n", tm.getRunTime());
    
    std::string shortname = name;
    if (shortname.find('/') != shortname.npos) {
      shortname = shortname.substr(shortname.find_last_of('/')+1, -1);
    }
    #pragma omp critical
    printf("compute %s rms=%.2fdB peak=%d landmarks=%d\n", shortname.c_str(),
      log10(totle) * 10, (int)peaks.size()/2, lms);

    if (true) {
      tm.getRunTime();
      unsigned char *bmp = new unsigned char[blockn * nFreq * 3];
      for (int i = 0; i < blockn; i++) {
        double dbOffset = log10(FFT_SIZE / 2) * 20;
        for (int j = 0; j < nFreq; j++) {
          double db = spec[i * nFreq + j] - dbOffset;
          int idx = (i + j * blockn) * 3;
          getOldColor(db, &bmp[idx]);
        }
      }
      for (int i = 0; i < peaks.size()/2; i++) {
        int t = peaks[i*2];
        int f = peaks[i*2+1];
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
      std::string bmpName = name.substr(0, name.size()-4) + "_spec.bmp";
      br.WriteBMP(bmpName.c_str(), blockn, nFreq, bmp);
      printf("write spectrogram %.2fms\n", tm.getRunTime());
      delete[] bmp;
    }
    delete[] local_max;
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
  #pragma omp parallel for schedule(dynamic)
  for (int i = 0; i < filenames.size(); i++) {
    std::string name = filenames[i];
    printf("File: %s\n", name.c_str());
    processMusic(name);
  }
  printf("Total time: %.3fs\n", timing.getRunTime() * 0.001);
  return 0;
}
