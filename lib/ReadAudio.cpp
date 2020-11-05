#include "ReadAudio.hpp"
#define MINIMP3_IMPLEMENTATION
#include "minimp3/minimp3.h"
#include "minimp3/minimp3_ex.h"
#include "WavReader.hpp"
#include <cstdlib>
#include <stdexcept>

Sound ReadAudio(std::string filename) {
  size_t dot = filename.find_last_of('.');
  if (dot == filename.npos) {
    // file format unknown!
    throw std::runtime_error("Unsupported file extension. File must be mp3 or wav.");
  }
  std::string ext = filename.substr(dot);
  if (ext == ".wav") {
    return ReadAudio_wav(filename);
  }
  if (ext == ".mp3") {
    return ReadAudio_mp3(filename);
  }
  throw std::runtime_error("Unsupported file extension. File must be mp3 or wav.");
}

Sound ReadAudio_wav(std::string filename) {
  WavReader wav;
  int error = wav.ReadWAV(filename.c_str());
  if (error == 2) {
    throw std::runtime_error("Cannot read file, file might not exist");
  }
  if (wav.channels < 1 || wav.hz < 1) {
    throw std::runtime_error("Malformed wav file");
  }
  unsigned len = wav.nSamples / wav.channels;
  Sound snd(len, wav.channels, wav.hz);
  int channels = wav.channels;
  float *sam = wav.samples;
  for (unsigned i = 0; i < len; i++) {
    for (int ch = 0; ch < channels; ch++) {
      snd.d[ch][i] = sam[i * channels + ch];
    }
  }
  free(sam);
  return snd;
}

Sound ReadAudio_mp3(std::string filename) {
  mp3dec_t mp3d;
  mp3dec_file_info_t info;
  mp3dec_init(&mp3d);
  info.buffer = 0;
  int error = mp3dec_load(&mp3d, filename.c_str(), &info, NULL, NULL);
  if (error) {
    free(info.buffer);
    if (error == MP3D_E_IOERROR)
      throw std::runtime_error("Cannot read file, file might not exist");
    else
      throw std::runtime_error("Malformed mp3 file");
  }
  if (info.hz == 0 || info.channels == 0) {
    free(info.buffer);
    throw std::runtime_error("Malformed mp3 file");
  }
  
  unsigned len = info.samples / info.channels;
  Sound snd(len, info.channels, info.hz);
  int channels = info.channels;
  mp3d_sample_t *sam = info.buffer;
  for (unsigned i = 0; i < len; i++) {
    for (int ch = 0; ch < channels; ch++) {
      snd.d[ch][i] = sam[i * channels + ch] * (1.0 / 32768.0);
    }
  }
  free(info.buffer);
  return snd;
}
