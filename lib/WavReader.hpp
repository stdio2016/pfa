#include <stdio.h>

class WavReader {
public:
  unsigned char header[44];
  unsigned char buf[1024];
  unsigned nSamples; // channels * length
  unsigned hz;
  int channels;
  int bitDepth;
  float *samples;
  int ReadWAV(const char *filename);
  int WriteWAV(const char *filename, int bitDepth);

private:
  int readFmt(void);
};
