#include <vector>

std::vector<float> lopass(const std::vector<float> &snd, double ratio, int firSize);
std::vector<float> resample(const std::vector<float> &snd, int from, int to);

/*
  NFFT: STFT size
  noverlap: overlap between segments
  first get size of output with specgram_num_frames
  then use specgram to get spectrogram
  specgram outputs power spectrum density as 2d array
  out[time * (NFFT/2+1) + freq]
*/
size_t specgram_num_frames(size_t x_len, int NFFT, int noverlap);
void specgram(const float *x, size_t x_len, int NFFT, int noverlap, double *out);

// spectrogram image is also a kind of "signal"
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
