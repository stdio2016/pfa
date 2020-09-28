#include <stdio.h>
#include <stdexcept>
#define MINIMP3_IMPLEMENTATION
#include "minimp3/minimp3.h"
#include "minimp3/minimp3_ex.h"
#include "fft.hpp"
#include "BmpReader.h"
#include "Timing.hpp"

static void write4b(uint8_t *buf, uint32_t n) {
  buf[0] = n&0xff;
  buf[1] = n>>8&0xff;
  buf[2] = n>>16&0xff;
  buf[3] = n>>24&0xff;
}

static void write2b(uint8_t *buf, uint32_t n) {
  buf[0] = n&0xff;
  buf[1] = n>>8&0xff;
}

void getOldColor(float db, unsigned char *bmp) {
  bmp[0] = 0;
  bmp[2] = 0;
  if (db > -22) {
    bmp[1] = 255;
  }
  else if (db > -92) {
    bmp[1] = (db+92)*(db+92) / 4900 * 255;
  }
  else {
    bmp[1] = 0;
  }
}

void getColor(float db, unsigned char *bmp) {
  if (db > -10) { // brown
    bmp[0] = 0;
    bmp[1] = 0;
    bmp[2] = 128;
  }
  else if (db > -20) { // brown -> red
    bmp[0] = 0;
    bmp[1] = 0;
    bmp[2] = 255 - (db+20)/10 * 127;
  }
  else if (db > -30) { // red -> yellow
    bmp[0] = 0;
    bmp[1] = 255 - (db+30)/10 * 255;
    bmp[2] = 255;
  }
  else if (db > -40) { // yellow -> green
    bmp[0] = 0;
    bmp[1] = 192;
    bmp[2] = (db+40)/10 * 255;
  }
  else if (db > -50) { // green -> cyan
    bmp[0] = 192 - (db+50)/10 * 192;
    bmp[1] = 192;
    bmp[2] = 0;
  }
  else if (db > -65) { // cyan -> blue
    bmp[0] = 192;
    bmp[1] = (db+65)/15 * 192;
    bmp[2] = 0;
  }
  else if (db > -80) { // blue -> indigo
    bmp[0] = 128 + (db+80)/15 * 64;
    bmp[1] = 0;
    bmp[2] = 0;
  }
  else { // indigo
    bmp[0] = 128;
    bmp[1] = 0;
    bmp[2] = 0;
  }
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    puts("usage: ./readmp3 mp3file");
    return 1;
  }
  const char *file_name = argv[1];
  mp3dec_t mp3d;
  mp3dec_file_info_t info;
  Timing tm;
  mp3dec_init(&mp3d);
  info.buffer = 0;
  if (mp3dec_load(&mp3d, file_name, &info, NULL, NULL)) {
    puts("This is not an MP3 file");
    return 1;
  }
  if (info.hz == 0) {
    puts("This is not an MP3 file");
    free(info.buffer);
    return 1;
  }
  printf("decode %f\n", tm.getRunTime());
  printf("hz = %u channels = %u samples = %u bits = %u\n", info.hz, info.channels, info.samples, (unsigned)sizeof(mp3d_sample_t)*8);
  tm.getRunTime();

  FFT<float> fft(1024);
  float *ts = new float[2048];
  unsigned len = info.samples / info.channels;
  int nblocks = len / 2048;
  float *output = new float[nblocks * 2048 * info.channels];
  for (int i = 0; i < nblocks; i++) {
    for (int ch = 0; ch < info.channels; ch++) {
      float adjust = 1 / 32768.0f;
      for (int j = 0; j < 2048; j++) {
        ts[j] = info.buffer[(i*2048+j) * info.channels + ch] * adjust;
      }
      fft.realFFT(ts, output + (ch * nblocks + i) * 2048);
      fft.realIFFT(output + (ch * nblocks + i) * 2048, ts);
      for (int j = 0; j < 2048; j++) {
        ts[j] *= 32768.0f / 2048.0f * 2.0f;
        int out = 0;
        if (ts[j] > 32767) out = 32767;
        else if (ts[j] < -32768) out = -32768;
        else if (ts[j] == ts[j]) out = int(round(ts[j]));
        else out = 12345;
        info.buffer[(i*2048+j) * info.channels + ch] = out;
      }
    }
  }
  printf("try fft and ifft %f\n", tm.getRunTime());
  tm.getRunTime();
  
  FILE *f = fopen("out.wav", "wb");
  uint32_t size = info.samples * sizeof(mp3d_sample_t);
  uint8_t header[44] = {0};
  memcpy(header+0, "RIFF", 4);
  write4b(header+4, size + 36);
  memcpy(header+8, "WAVE", 4);
  memcpy(header+12, "fmt ", 4);
  write4b(header+16, 16);
  write2b(header+20, 1);
  write2b(header+22, info.channels);
  write4b(header+24, info.hz);
  write4b(header+28, info.hz * sizeof(mp3d_sample_t));
  write2b(header+32, 4);
  write2b(header+34, sizeof(mp3d_sample_t) * 8);
  memcpy(header+36, "data", 4);
  write4b(header+40, size);
  fwrite(header, 44, 1, f);
  fwrite(info.buffer, sizeof(mp3d_sample_t), info.samples, f);
  fclose(f);
  printf("output %f\n", tm.getRunTime());
  tm.getRunTime();

  int highBin = 1024;
  if (info.hz > 16000) {
    highBin = 8000.0 * 2048.0 / info.hz;
  }
  unsigned char *bmp = new unsigned char[nblocks * highBin * 3];
  for (int i = 0; i < nblocks; i++) {
    float base10 = log(10);
    float adjust = log(32768.0 * 2048.0) / base10 * 20 - 6;
    for (int j = 0; j < 2048; j++) {
      ts[j] = info.buffer[(i*2048+j) * info.channels] * (2.0 / (32768.0 * 2048.0));
    }
    fft.realFFT(ts, ts);
    for (int j = 0; j < highBin; j++) {
      float mag = (ts[j*2] * ts[j*2] + ts[j*2+1] * ts[j*2+1]) + 1e-10;
      int idx = (i + j * nblocks) * 3;
      float db = log(mag) / base10 * 10;
      getOldColor(db, &bmp[idx]);
    }
  }
  printf("fft %f\n", tm.getRunTime());
  tm.getRunTime();
  BmpReader br;
  br.WriteBMP("out.bmp", nblocks, highBin, bmp);
  printf("write spectrogram %f\n", tm.getRunTime());
  
  delete[] bmp;
  delete[] ts;
  free(info.buffer);
  return 0;
}
