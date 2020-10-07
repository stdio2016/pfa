#include <cmath>
#include <stdio.h>
#include <algorithm>
#include "ReadAudio.hpp"
#include "WavReader.hpp"
#include "Timing.hpp"

std::vector<double> lopass(const std::vector<double> &snd, double ratio, int firSize) {
  std::vector<double> fir(firSize*2+1);
  for (int i = -firSize; i <= firSize; i++) {
    if (i == 0)
      fir[firSize] = ratio;
    else {
      double x = 3.14159 * ratio * i;
      fir[i+firSize] = sin(x) / x * ratio;
    }
  }
  size_t len = snd.size();
  std::vector<double> out(len + firSize*2);
  for (int i = 0; i < len; i++) {
    for (int j = 0; j <= firSize*2; j++) {
      out[i+j] += snd[i] * fir[j];
    }
  }
  std::copy(&out[firSize], &out[len+firSize], out.begin());
  out.resize(len);
  return out;
}

std::vector<double> downsample_some(const std::vector<double> &snd, double ratio) {
  if (ratio >= 1.0) {
    return snd;
  }
  std::vector<double> dat = lopass(snd, ratio, 20);
  size_t len = snd.size();
  double result_len = len * ratio;
  size_t new_len = round(result_len);
  double skip = 1.0/ratio;
  std::vector<double> out;
  out.reserve(new_len);
  for (size_t i = 0; i < new_len; i++) {
    double pos = skip * i;
    size_t t = floor(pos);
    double frac = pos - floor(pos);
    if (t >= len-1) {
      out.push_back(dat[len-1]);
    }
    else {
      out.push_back((1.0-frac) * dat[t] + frac * dat[t+1]);
    }
  }
  return out;
}

std::vector<double> halfsample(const std::vector<double> &snd) {
  std::vector<double> dat = lopass(snd, 0.5, 20);
  size_t len = snd.size();
  size_t new_len = len-len/2;
  std::vector<double> out;
  out.reserve(new_len);
  for (size_t i = 0; i < new_len; i++) {
    out.push_back(dat[i*2]);
  }
  return out;
}

std::vector<double> downsample(const std::vector<double> &snd, double ratio) {
  Timing tm;
  std::vector<double> out = snd;
  size_t len = snd.size();
  size_t new_len = round(len * ratio);
  while (ratio < 0.5) {
    out = halfsample(out);
    ratio *= 2.0;
  }
  out = downsample_some(out, ratio);
  out.resize(new_len);
  return out;
}

int main(int argc, char const *argv[]) {
  if (argc < 2) {
    return 1;
  }
  Timing tm;
  Sound snd = ReadAudio(argv[1]);
  printf("read file %f\n", tm.getRunTime());

  tm.getRunTime();
  size_t len = snd.length();
  int channels = snd.numberOfChannels();
  printf("duration %f\n", len/snd.sampleRate);
  for (int i = 1; i < channels; i++) {
    for (int j = 0; j < len; j++)
      snd.d[0][j] += snd.d[i][j];
  }
  for (int i = 0; i < len; i++) {
    snd.d[0][i] *= 1.0 / channels;
  }
  snd.d.resize(1);
  printf("stereo to mono %f\n", tm.getRunTime());

  printf("sample rate %f\n", snd.sampleRate);
  tm.getRunTime();
  snd.d[0] = downsample(snd.d[0], 8000.0/snd.sampleRate);
  printf("low pass filter %f\n", tm.getRunTime());

  tm.getRunTime();
  std::vector<float> out3(snd.length());
  for (int i = 0; i < snd.length(); i++) out3[i] = snd.d[0][i];
  WavReader wav;
  wav.hz = 8000;
  wav.nSamples = snd.length();
  wav.channels = 1;
  wav.samples = &out3[0];
  wav.WriteWAV("lo.wav", 16);
  printf("write to file %f\n", tm.getRunTime());
  return 0;
}
