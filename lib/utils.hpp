wchar_t *utf8_to_wchar(const char *str);

void init_logger(const char *app_name);

void mylogger(int level, const char *fmt, ...);

#define LOG_DEBUG(fmt, ...) mylogger(1, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) mylogger(2, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) mylogger(3, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) mylogger(4, fmt, ##__VA_ARGS__)
#define LOG_FATAL(fmt, ...) mylogger(4, fmt, ##__VA_ARGS__)
