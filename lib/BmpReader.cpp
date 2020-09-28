#include <cstdio>
#include "BmpReader.h"
unsigned char *BmpReader::ReadBMP(const char *filename, int *width, int *height, int *color) {
    unsigned char *buf;
    int w, h;
    int bitdepth;
    int headn;
    FILE *f = fopen(filename, "rb");
    if (f == NULL) {
        fprintf(stderr, "cannot open file %s\n", filename);
        return NULL;
    }
    if (fread(header, 1, 54, f) != 54) goto fail; // file too small
    if (header[0] != 'B' || header[1] != 'M') goto fail; // magic number BM
    headn = readUnsigned(header+10, 4);
    if (readUnsigned(header+14, 4) != 40) goto fail; // header size
    *width = w = readUnsigned(header+18, 4);
    *height = h = readUnsigned(header+22, 4);
    if (readUnsigned(header+26, 2) != 1) goto fail; // must be 1
    *color = bitdepth = readUnsigned(header+28, 2);
    if (bitdepth != 24 && bitdepth != 8) goto fail; // 24 bit color
    if (readUnsigned(header+30, 4) != 0) goto fail; // no compresion
    if (readUnsigned(header+46, 4) != 0) goto fail; // no palette
    buf = new unsigned char[w * h * 3];
    headn -= 54;

    // read palette
    if (bitdepth <= 8) {
        for (int i = 0; i < 1<<bitdepth; i++) {
            palette[i*3] = fgetc(f);
            palette[i*3+1] = fgetc(f);
            palette[i*3+2] = fgetc(f);
            fgetc(f);
            headn -= 4;
        }
    }

    for (int i = 0; i < headn; i++) fgetc(f);

    for (int y = 0; y < *height; y++) {
        fread(buf + w*3 * y, bitdepth>>3, w, f);
        if (bitdepth == 8) {
            for (int x = *width-1; x >= 0; x--) {
                int c = buf[y*w*3 + x];
                buf[(y*w + x)*3] = palette[c*3];
                buf[(y*w + x)*3+1] = palette[c*3+1];
                buf[(y*w + x)*3+2] = palette[c*3+2];
            }
        }
        // BMP is 4 byte aligned
        for (int padding = w*bitdepth>>3; padding&3; padding++) fgetc(f);
    }
    fclose(f);
    return buf;
fail:
    fprintf(stderr, "%s unsupported bmp format\n", filename);
    fclose(f);
    return NULL;
}

unsigned int BmpReader::readUnsigned(unsigned char *bytes, int size) {
    unsigned int n = 0;
    for (int i = 0; i < size; i++) {
        n = n | bytes[i]<<8*i;
    }
    return n;
}

int BmpReader::WriteBMP(const char *filename, int width, int height, unsigned char *bits) {
    FILE *f = fopen(filename, "wb");
    if (f == NULL) {
        fprintf(stderr, "cannot write file %s\n", filename);
        return 1;
    }
    int row = width*3, pad = 0;
    if (row&3) {
        pad = 4-(row&3);
        row += pad;
    }
    fwrite("BM", 2, 1, f);
    writeUnsigned(54 + row * height, 4, f); // file size
    writeUnsigned(0, 4, f); // reserved
    writeUnsigned(54, 4, f); // total header size
    writeUnsigned(40, 4, f); // header size
    writeUnsigned(width, 4, f);
    writeUnsigned(height, 4, f);
    writeUnsigned(1, 2, f); // planes
    writeUnsigned(24, 2, f); // 24 bit color
    writeUnsigned(0, 4, f); // compression
    writeUnsigned(row * height, 4, f); // data size
    writeUnsigned(0, 4, f); // h resolution
    writeUnsigned(0, 4, f); // v resolution
    writeUnsigned(0, 4, f); // palette size
    writeUnsigned(0, 4, f); // important colors
    for (int i = 0; i < height; i++) {
        fwrite(bits + 3*width*i, 3, width, f);
        for (int j = 0; j < pad; j++) fputc(0, f);
    }
    fclose(f);
    return 0;
}

void BmpReader::writeUnsigned(unsigned number, int size, FILE *f) {
    for (int i = 0; i < size; i++) {
        fputc(number>>(i*8) & 0xff, f);
    }
}
