#include <stdio.h>
#include <cmath>
#include "lib/BmpReader.h"
#include "lib/fft.hpp"
#include "lib/WavReader.hpp"
#include <vector>
#include <cmath>
#include <cstdlib>

int main(int argc, char const *argv[]) {
  if (argc < 2) {
    puts("usage: ./funimg bmpfile");
    return 1;
  }
  BmpReader br;
  int width, height, color;
  unsigned char *bmp = br.ReadBMP(argv[1], &width, &height, &color);
  if (!bmp) {
    printf("Fail to read bmp\n");
    return 1;
  }
  if (height > 1000) {
    printf("Image too high\n");
    return 1;
  }
  FFT<double> fft(1024);
  std::vector<double> din(2048), dout(2048), toamp(256), hann(2048);
  for (int i = 0; i < 256; i++) {
    double db = sqrt(i / 255.0 * 4900.0) - 92;
    toamp[i] = exp(db * (0.05 * log(10))) * 1.4;
  }
  for (int i = 0; i < 2048; i++) {
    hann[i] = 0.5 - 0.5 * cos(i * 3.14159 / 1024.0);
  }
  int nSamples = width * 2048;
  std::vector<float> out(nSamples);
  for (int i = 0; i < width; i++) {
    for (int j = 1; j < height; j++) {
      double amp = toamp[bmp[(j*width + i)*3 + 1]];
      double r = 3.14159 / RAND_MAX * rand();
      din[j*2] = amp * cos(r);
      din[j*2+1] = amp * sin(r);
    }
    fft.realIFFT(&din[0], &dout[0]);
    for (int j = 0; j < 2048; j++) {
      out[i * 1024 + j] += hann[j] * dout[j];
    }
  }
  WavReader wav;
  wav.channels = 1;
  wav.hz = 17640;
  wav.nSamples = nSamples;
  wav.samples = &out[0];
  wav.WriteWAV("ifft.wav", 16);
  return 0;
}
