#include "WavReader.hpp"
#include "Timing.hpp"

int main(int argc, char *argv[]) {
  if (argc < 2) {
    puts("usage: ./readmp3 mp3file");
    return 1;
  }
  const char *file_name = argv[1];
  WavReader wav;
  Timing tm;
  if (wav.ReadWAV(file_name)) {
    puts("This is not a wav file");
    return 1;
  }
  printf("decode %f\n", tm.getRunTime());
  printf("hz = %u channels = %u samples = %u\n", wav.hz, wav.channels, wav.nSamples);
  tm.getRunTime();
  wav.WriteWAV("www.wav", 16);
  printf("output %f\n", tm.getRunTime());
  return 0;
}
