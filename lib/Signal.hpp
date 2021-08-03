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
