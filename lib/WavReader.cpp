#include "WavReader.hpp"
#include <stdio.h>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cmath>

static uint32_t read4b(uint8_t *buf) {
  return buf[0] | buf[1]<<8 | buf[2]<<16 | buf[3]<<24;
}

static uint32_t read2b(uint8_t *buf) {
  return buf[0] | buf[1]<<8;
}

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

int WavReader::readFmt(void) {
  int errs = 0;
  if (read2b(buf) != 1) {
    printf("fmt tag %u not supported\n", read2b(buf));
    errs += 1;
  }
  unsigned byteRate = 0, blockAlign = 0;

  channels = read2b(buf + 2);
  hz = read4b(buf + 4);
  byteRate = read4b(buf + 8);
  blockAlign = read2b(buf + 12);
  bitDepth = read2b(buf + 14);

  if (bitDepth != 8 && bitDepth != 16) {
    printf("unsupported bit depth: %d\n", bitDepth);
    errs += 1;
  }
  
  if (byteRate != hz * channels * (bitDepth/8)) {
    printf("bit rate incorrect\n");
  }

  if (blockAlign != channels * (bitDepth/8)) {
    printf("block align incorrect\n");
  }
  return errs;
}

int WavReader::ReadWAV(const char *filename) {
  nSamples = 0;
  bitDepth = 16;
  samples = 0;
  
  FILE *fin = fopen(filename, "rb");
  unsigned size = 0, maxSamples = 0;
  bool fmted = false;
  if (!fin) {
    return 2;
  }
  if (fread(buf, 4, 3, fin) != 3) {
    printf("less than 12 bytes\n");
    goto fail;
  }
  if (memcmp(buf + 0, "RIFF", 4) != 0) {
    printf("RIFF expected\n");
    goto fail;
  }
  size = read4b(buf + 4);
  if (size < 36) {
    printf("file too small\n");
    goto fail;
  }

  if (memcmp(buf + 8, "WAVE", 4) != 0) {
    printf("WAVE expected\n");
    goto fail;
  }

  while (size >= 8) {
    if (fread(buf, 4, 2, fin) != 2) {
      printf("not enough data to read\n");
      goto fail;
    }
    size -= 8;
    unsigned chkSize = read4b(buf + 4);
    unsigned remain = chkSize;
    printf("chunk %c%c%c%c size %u\n", buf[0],buf[1],buf[2],buf[3], chkSize);
    if (chkSize > size) {
      printf("chunk size too big %u\n", chkSize);
      goto fail;
    }
    int type = 0;
    if (memcmp(buf, "fmt ", 4) == 0) type = 1;
    else if (memcmp(buf, "data", 4) == 0) type = 2;
    while (remain > 0) {
      int shouldRead = 1024;
      if (remain < shouldRead) shouldRead = remain;
      if (fread(buf, 1, shouldRead, fin) != shouldRead) {
        printf("not enough data to read\n");
        goto fail;
      }
      if (type == 2 && fmted) {
        if (bitDepth == 8) {
          for (int i = 0; i < shouldRead; i++) {
            samples[nSamples++] = buf[i] * (1.0f/127.5f) - 1.0f;
          }
        }
        else if (bitDepth == 16) {
          for (int i = 0; i < shouldRead; i += 2) {
            int r = read2b(buf + i);
            if (r > 32767) r -= 65536;
            samples[nSamples++] = r * (1.0f/32768.0f);
          }
        }
      }
      remain -= shouldRead;
      size -= shouldRead;
    }
    if (chkSize&1) { // padding to multiple of 2 bytes?
      fgetc(fin);
    }
    if (type == 1) {
      if (chkSize > 100 || chkSize < 16) {
        printf("fmt chunk size incorrect\n");
      }
      else if (fmted) {
        printf("multiple fmt chunks\n");
      }
      else {
        if (readFmt() == 0) {
          fmted = true;
          maxSamples = size / (bitDepth/8);
          samples = (float*) malloc(maxSamples * sizeof(float));
          if (!samples) {
            printf("no memory\n");
            goto fail;
          }
          nSamples = 0;
        }
      }
    }
  }
  fclose(fin);
  return !fmted;
fail:
  fclose(fin);
  return 1;
}

int WavReader::WriteWAV(const char *filename, int bitDepth) {
  if (bitDepth != 8 && bitDepth != 16) {
    return 400;
  }
  FILE *fout = fopen(filename, "wb");
  if (!fout) {
    return 1;
  }
  // create header
  unsigned size = nSamples * (bitDepth/8);
  memcpy(header+0, "RIFF", 4);
  write4b(header+4, size + 36);
  memcpy(header+8, "WAVE", 4);
  memcpy(header+12, "fmt ", 4);
  write4b(header+16, 16);
  write2b(header+20, 1);
  write2b(header+22, channels);
  write4b(header+24, hz);
  write4b(header+28, hz * channels * (bitDepth/8));
  write2b(header+32, channels * (bitDepth/8));
  write2b(header+34, bitDepth);
  memcpy(header+36, "data", 4);
  write4b(header+40, size);
  
  if (fwrite(header, 44, 1, fout) != 1) {
    goto fail;
  }
  if (bitDepth == 8) {
    unsigned remain = nSamples, j = 0;
    while (remain > 0) {
      int chk = 1024;
      if (remain < 1024) chk = remain;
      for (int i = 0; i < chk; i++, j++) {
        float r = samples[j] * 127.5f + 127.5f;
        if (std::isnan(r)) r = 0;
        else if (r >= 255) r = 255;
        else if (r <= 0) r = 0;
        buf[i] = (int)round(r);
      }
      if (fwrite(buf, 1, chk, fout) != chk) goto fail;
      remain -= chk;
    }
  }
  else if (bitDepth == 16) {
    unsigned remain = nSamples, j = 0;
    while (remain > 0) {
      int chk = 512;
      if (remain < 512) chk = remain;
      for (int i = 0; i < chk; i++, j++) {
        float r = samples[j] * 32768.0f;
        if (std::isnan(r)) r = 0;
        else if (r >= 32767) r = 32767;
        else if (r <= -32768) r = -32768;
        write2b(buf + i*2, (int)round(r));
      }
      if (fwrite(buf, 2, chk, fout) != chk) goto fail;
      remain -= chk;
    }
  }
  fclose(fout);
  return 0;
fail:
  fclose(fout);
  return 1;
}
