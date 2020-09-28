#include<cstdio>
class BmpReader {
public:
    unsigned char header[54];
    unsigned char palette[256*3];
    unsigned char *ReadBMP(const char *filename, int *width, int *height, int *color);
    unsigned int readUnsigned(unsigned char *bytes, int size);
    int WriteBMP(const char *filename, int width, int height, unsigned char *bits);
    void writeUnsigned(unsigned number, int size, FILE *f);
};
