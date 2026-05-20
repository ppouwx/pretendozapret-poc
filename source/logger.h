#pragma once
#include <stdint.h>
#include <stdbool.h>

#define LOG_PATH "sd:/wiiu-bypass/log.txt"
#define LOG_MAX_SIZE (512 * 1024) // 512KB, auto-truncate when exceeded

typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO  = 1,
    LOG_WARN  = 2,
    LOG_ERROR = 3,
} log_level_t;

extern log_level_t g_log_min_level;
extern bool g_log_enabled;

void Log_Init(void);
void Log_Deinit(void);
void Log_Write(log_level_t level, const char *tag, const char *fmt, ...);

#define Log_Debug(tag, fmt, ...)  Log_Write(LOG_DEBUG, tag, fmt, ##__VA_ARGS__)
#define Log_Info(tag, fmt, ...)   Log_Write(LOG_INFO,  tag, fmt, ##__VA_ARGS__)
#define Log_Warn(tag, fmt, ...)   Log_Write(LOG_WARN,  tag, fmt, ##__VA_ARGS__)
#define Log_Error(tag, fmt, ...)  Log_Write(LOG_ERROR, tag, fmt, ##__VA_ARGS__)
