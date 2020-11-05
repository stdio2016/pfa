#include <cmath>
#include "Signal.hpp"

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
