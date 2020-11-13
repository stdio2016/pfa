#include <cstring>
#include <stdio.h>

wchar_t *utf8_to_wchar(const char *str) {
  size_t need = 0;
  size_t len = strlen(str);
  size_t i = 0, j = 0;
  while (str[i]) {
    if ((str[i] & 0xF0) == 0xF0) {
      if (len-i < 4) break;
      need += 2;
      i += 4;
    }
    else if ((str[i] & 0xE0) == 0xE0) {
      if (len-i < 3) break;
      need += 1;
      i += 3;
    }
    else if ((str[i] & 0xC0) == 0xC0) {
      if (len-i < 2) break;
      need += 1;
      i += 2;
    }
    else {
      need += 1;
      i += 1;
    }
  }
  wchar_t *out = new wchar_t[need+1];
  i = 0;
  while (str[i]) {
    if ((str[i] & 0xF0) == 0xF0) {
      if (len-i < 4) break;
      int d = (str[i] & 7)<<18 | (str[i+1] & 0x3F)<<12 | (str[i+2] & 0x3F)<<6 | (str[i+3] & 0x3F);
      d -= 0x10000;
      out[j++] = 0xD800 | (d & 0x3FF);
      out[j++] = 0xDC00 | (d>>10 & 0x3FF);
      i += 4;
    }
    else if ((str[i] & 0xE0) == 0xE0) {
      if (len-i < 3) break;
      out[j++] = (str[i] & 0xF)<<12 | (str[i+1] & 0x3F)<<6 | (str[i+2] & 0x3F);
      i += 3;
    }
    else if ((str[i] & 0xC0) == 0xC0) {
      if (len-i < 2) break;
      out[j++] = (str[i] & 0x1F)<<6 | (str[i+1] & 0x3F);
      i += 2;
    }
    else {
      out[j++] = str[i] & 0x7F;
      i += 1;
    }
  }
  out[need] = 0;
  return out;
}
