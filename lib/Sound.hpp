#include <vector>

class Sound {
public:
  // usage: sound.d[channel][time in samples]
  std::vector<std::vector<double>> d;
  
  double sampleRate;
  
  Sound(): d(1), sampleRate(44100) {}
  
  Sound(size_t length, int channels, double sampleRate): 
    d(channels), sampleRate(sampleRate)
  {
    for (int i = 0; i < channels; i++) {
      d[i].resize(length);
    }
  }

  inline int numberOfChannels() const {
    return d.size();
  }

  inline size_t length() const {
    return d[0].size();
  }
};
