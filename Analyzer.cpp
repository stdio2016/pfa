#include "Analyzer.hpp"
#include "lib/Sound.hpp"
#include "lib/ReadAudio.hpp"
#include "lib/Signal.hpp"
#include "lib/Timing.hpp"
#include "lib/utils.hpp"

std::vector<Landmark> Analyzer::fingerprint_file(const char *name) {
  Timing tm;
  Sound snd = ReadAudio(name);
  LOG_DEBUG("load audio %.3fms", tm.getRunTime());
  
  snd.stereo_to_mono_();
  
  snd.d[0] = resample(snd.d[0], snd.sampleRate, this->SAMPLE_RATE);
  LOG_DEBUG("resample %.3fms", tm.getRunTime());
  
  return fingerprint_waveform(snd.d[0].data(), snd.length());
}

std::vector<Landmark> Analyzer::fingerprint_waveform(const float *wave, size_t len) {
  Timing tm;
  std::vector<Landmark> out;
  
  peaks.clear();
  for (int rep = 0; rep < REPEAT_COUNT; rep++) {
    int shift_n = (peak_finder->FFT_SIZE - peak_finder->NOVERLAP) / REPEAT_COUNT * rep;
    
    std::vector<Peak> cur_peaks = peak_finder->find_peaks(wave + shift_n, len - shift_n);
    peaks.insert(peaks.begin(), cur_peaks.begin(), cur_peaks.end());
    
    std::vector<Landmark> lms = landmark_builder->peaks_to_landmarks(cur_peaks);
    out.insert(out.end(), lms.begin(), lms.end());
  }
  LOG_DEBUG("fingerprint %.3fms", tm.getRunTime());
  return out;
}
