#include <cstring>
#include <stdio.h>
#include <ctime>
#include <string>
#include <cstdarg>
#include <omp.h>

static FILE *log_file;

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

#ifdef _WIN32
static struct tm* localtime_r(time_t *timep, struct tm *result) {
  localtime_s(result, timep);
  return result;
}
#endif

void init_logger(const char *app_name) {
  // use current time for log name
  time_t start_time;
  time(&start_time);
  char namebuf[100];
  struct tm timeinfo;
  localtime_r(&start_time, &timeinfo);
  strftime(namebuf, 98, "%Y%m%d-%H%M%S", &timeinfo);
  
  std::string path = "logs/";
  path = path + app_name + "-" + namebuf + ".log";
  log_file = fopen(path.c_str(), "w");
}

void mylogger(int level, const char *fmt, ...) {
  va_list ap;
  va_list ap2;
  
  time_t start_time;
  time(&start_time);
  char namebuf[100];
  struct tm timeinfo;
  localtime_r(&start_time, &timeinfo);
  strftime(namebuf, 98, "%H:%M:%S", &timeinfo);
  
  const char *severity = "TRACE";
  if (level == 1) severity = "DEBUG";
  if (level == 2) severity = "INFO";
  if (level == 3) severity = "WARN";
  if (level == 4) severity = "ERROR";
  if (level == 5) severity = "FATAL";
  
  #pragma omp critical(mylogger)
  {
    va_start(ap, fmt);
    va_copy(ap2, ap);
    if (log_file) {
      fprintf(log_file, "[%s] [thread %d/%s]: ", namebuf, omp_get_thread_num(), severity);
      vfprintf(log_file, fmt, ap);
      fputc('\n', log_file);
      fflush(log_file);
    }
    va_end(ap);
    if (level >= 2) {
      fprintf(stderr, "[%s] [thread %d/%s]: ", namebuf, omp_get_thread_num(), severity);
      vfprintf(stderr, fmt, ap2);
      fputc('\n', stderr);
    }
    va_end(ap2);
  }
}
