#include "Sound.hpp"
#include <string>

// choose a decoder based on file extension
Sound ReadAudio(std::string filename);

Sound ReadAudio_mp3(std::string filename);
Sound ReadAudio_wav(std::string filename);
