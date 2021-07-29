#include "Sound.hpp"
#include "utils.hpp"

void Sound::stereo_to_mono_(void) {
  size_t len = this->length();
  int channels = this->numberOfChannels();
  if (channels == 2) {
    // L/R channels can be in opposite phase
    // example: fma_medium/107/107535.mp3
    double pow1 = 0.0;
    double pow2 = 0.0;
    for (size_t i = 0; i < len; i++) {
      pow1 += (d[0][i] - d[1][i]) * (d[0][i] - d[1][i]);
      pow2 += (d[0][i] + d[1][i]) * (d[0][i] + d[1][i]);
    }
    if (pow1 > pow2 * 1000) {
      LOG_DEBUG("fake stereo with opposite phase detected");
      for (size_t i = 0; i < len; i++) {
        d[1][i] *= -1;
      }
    }
  }
  for (int i = 1; i < channels; i++) {
    for (int j = 0; j < len; j++)
      d[0][j] += d[i][j];
  }
  for (int i = 0; i < len; i++) {
    d[0][i] *= 1.0 / channels;
  }
  d.resize(1);
}
