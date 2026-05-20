#include "logger.h"
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <coreinit/time.h>
#include <coreinit/thread.h>
#include <coreinit/mutex.h>

static bool s_initialized = false;
static OSMutex s_log_mutex;

// Externals visible to config system
log_level_t g_log_min_level = LOG_INFO;
bool g_log_enabled = true;

static const char *level_str(log_level_t lvl) {
    switch (lvl) {
    case LOG_DEBUG: return "DEBUG";
    case LOG_INFO:  return "INFO";
    case LOG_WARN:  return "WARN";
    case LOG_ERROR: return "ERROR";
    default:        return "?";
    }
}

static void truncate_if_needed(void) {
    FILE *f = fopen(LOG_PATH, "r");
    if (!f) return;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fclose(f);

    if (size > (long)LOG_MAX_SIZE) {
        // Keep only the last 256KB by rotating
        rename(LOG_PATH, "sd:/wiiu-bypass/log.old.txt");
    }
}

void Log_Init(void) {
    if (s_initialized) return;
    OSInitMutex(&s_log_mutex);
    s_initialized = true;

    // Fresh start marker
    FILE *f = fopen(LOG_PATH, "a");
    if (f) {
        fprintf(f, "\n=== WiiU-Bypass log started ===\n");
        fclose(f);
    }
}

void Log_Deinit(void) {
    if (!s_initialized) return;

    FILE *f = fopen(LOG_PATH, "a");
    if (f) {
        fprintf(f, "=== WiiU-Bypass log ended ===\n");
        fclose(f);
    }

    s_initialized = false;
}

void Log_Write(log_level_t level, const char *tag, const char *fmt, ...) {
    if (!s_initialized || !g_log_enabled) return;
    if (level < g_log_min_level) return;

    OSLockMutex(&s_log_mutex);

    truncate_if_needed();

    FILE *f = fopen(LOG_PATH, "a");
    if (!f) {
        OSUnlockMutex(&s_log_mutex);
        return;
    }

    // Timestamp: seconds since boot
    uint64_t sec = OSTicksToSeconds(OSGetTime());
    fprintf(f, "[%06llu] [%s] [%s] ", sec, level_str(level), tag ? tag : "-");

    va_list args;
    va_start(args, fmt);
    vfprintf(f, fmt, args);
    va_end(args);

    fprintf(f, "\n");
    fclose(f);

    OSUnlockMutex(&s_log_mutex);
}
