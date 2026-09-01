/*
 * AEOS - Kernel Logging System
 *
 * Structured logging with levels, timestamps, and source tracking.
 * Designed for embedded systems: fixed-size buffers, no allocation.
 *
 * Usage:
 *   log_init();
 *   log_info("SYS", "System started");
 *   log_warn("MEM", "Low memory: %d pages free", free_pages);
 *   log_error("DRV", "Device not responding");
 *
 * Output goes to UART. Future: circular buffer for crash dumps.
 */

#ifndef AEOS_LOG_H
#define AEOS_LOG_H

#include <stdint.h>

/* Log levels (ordered by severity) */
typedef enum {
    LOG_TRACE = 0,
    LOG_DEBUG = 1,
    LOG_INFO  = 2,
    LOG_WARN  = 3,
    LOG_ERROR = 4,
    LOG_FATAL = 5,
    LOG_NONE  = 6  /* Disable all logging */
} log_level_t;

/* Log entry structure (for future ring buffer) */
typedef struct {
    uint64_t    timestamp;    /* Tick count when logged */
    log_level_t level;
    char        source[8];    /* Subsystem tag (e.g., "MEM", "DRV") */
    char        message[128]; /* Formatted message */
} log_entry_t;

/* Configuration */
#define LOG_MAX_ENTRIES  64
#define LOG_SOURCE_LEN   8
#define LOG_MSG_LEN      128

/* Initialize logging subsystem */
void log_init(void);

/* Set minimum log level (messages below this are dropped) */
void log_set_level(log_level_t level);

/* Get current log level */
log_level_t log_get_level(void);

/* Core logging function */
void log_write(log_level_t level, const char *source, const char *fmt, ...);

/* Convenience macros */
#define LOG_TRACE(src, fmt, ...) log_write(LOG_TRACE, src, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(src, fmt, ...) log_write(LOG_DEBUG, src, fmt, ##__VA_ARGS__)
#define LOG_INFO(src, fmt, ...)  log_write(LOG_INFO,  src, fmt, ##__VA_ARGS__)
#define LOG_WARN(src, fmt, ...)  log_write(LOG_WARN,  src, fmt, ##__VA_ARGS__)
#define LOG_ERROR(src, fmt, ...) log_write(LOG_ERROR, src, fmt, ##__VA_ARGS__)
#define LOG_FATAL(src, fmt, ...) log_write(LOG_FATAL, src, fmt, ##__VA_ARGS__)

/* Dump recent log entries to UART */
void log_dump(void);

/* Get log statistics */
uint32_t log_count(void);
uint32_t log_error_count(void);

#endif /* AEOS_LOG_H */
