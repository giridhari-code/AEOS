/*
 * AEOS - Kernel Logging System Implementation
 */

#include "log.h"
#include "uart.h"

static log_level_t current_level = LOG_INFO;
static log_entry_t log_buffer[LOG_MAX_ENTRIES];
static uint32_t log_head = 0;
static uint32_t log_total_count = 0;
static uint32_t log_error_total = 0;

static const char *level_names[] = {
    "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL", "NONE"
};

static const char *level_colors[] = {
    "\033[90m",   /* TRACE: gray */
    "\033[36m",   /* DEBUG: cyan */
    "\033[32m",   /* INFO: green */
    "\033[33m",   /* WARN: yellow */
    "\033[31m",   /* ERROR: red */
    "\033[35m",   /* FATAL: magenta */
    ""            /* NONE */
};

#define COLOR_RESET "\033[0m"

/* Simple vsnprintf for embedded (no libc) */
static int vsnprintf_simple(char *buf, int bufsize, const char *fmt, va_list args)
{
    int pos = 0;
    const char *f = fmt;

    while (*f && pos < bufsize - 1) {
        if (*f == '%') {
            f++;
            if (*f == 'd' || *f == 'i') {
                int val = __builtin_va_arg(args, int);
                char tmp[12];
                int i = 0, neg = 0;

                if (val < 0) { neg = 1; val = -val; }
                if (val == 0) { tmp[i++] = '0'; }
                else {
                    while (val > 0 && i < 10) {
                        tmp[i++] = '0' + (val % 10);
                        val /= 10;
                    }
                }
                if (neg) tmp[i++] = '-';
                while (i > 0 && pos < bufsize - 1)
                    buf[pos++] = tmp[--i];
            } else if (*f == 'u') {
                unsigned val = (unsigned)__builtin_va_arg(args, unsigned);
                char tmp[12];
                int i = 0;

                if (val == 0) { tmp[i++] = '0'; }
                else {
                    while (val > 0 && i < 10) {
                        tmp[i++] = '0' + (val % 10);
                        val /= 10;
                    }
                }
                while (i > 0 && pos < bufsize - 1)
                    buf[pos++] = tmp[--i];
            } else if (*f == 'x' || *f == 'X') {
                unsigned val = (unsigned)__builtin_va_arg(args, unsigned);
                const char *hex = (*f == 'x') ? "0123456789abcdef" : "0123456789ABCDEF";
                char tmp[10];
                int i = 0;

                if (val == 0) { tmp[i++] = '0'; }
                else {
                    while (val > 0 && i < 8) {
                        tmp[i++] = hex[val & 0xF];
                        val >>= 4;
                    }
                }
                while (i > 0 && pos < bufsize - 1)
                    buf[pos++] = tmp[--i];
            } else if (*f == 's') {
                const char *s = __builtin_va_arg(args, const char *);
                if (!s) s = "(null)";
                while (*s && pos < bufsize - 1)
                    buf[pos++] = *s++;
            } else if (*f == 'c') {
                char c = (char)__builtin_va_arg(args, int);
                if (pos < bufsize - 1)
                    buf[pos++] = c;
            } else if (*f == '%') {
                if (pos < bufsize - 1)
                    buf[pos++] = '%';
            }
            f++;
        } else {
            buf[pos++] = *f++;
        }
    }
    buf[pos] = '\0';
    return pos;
}

/* Built-in printf-like function using uart */
static void kernel_printf(const char *fmt, ...)
{
    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    char buf[256];
    vsnprintf_simple(buf, sizeof(buf), fmt, args);
    __builtin_va_end(args);
    uart_puts(buf);
}

void log_init(void)
{
    uint32_t i;

    log_head = 0;
    log_total_count = 0;
    log_error_total = 0;
    current_level = LOG_INFO;

    for (i = 0; i < LOG_MAX_ENTRIES; i++) {
        log_buffer[i].timestamp = 0;
        log_buffer[i].level = LOG_NONE;
        log_buffer[i].source[0] = '\0';
        log_buffer[i].message[0] = '\0';
    }

    LOG_INFO("LOG", "Logging initialized (level=%s)", level_names[current_level]);
}

void log_set_level(log_level_t level)
{
    if (level <= LOG_NONE)
        current_level = level;
}

log_level_t log_get_level(void)
{
    return current_level;
}

void log_write(log_level_t level, const char *source, const char *fmt, ...)
{
    __builtin_va_list args;
    log_entry_t *entry;

    /* Drop messages below current level */
    if (level < current_level)
        return;

    /* Store in ring buffer */
    entry = &log_buffer[log_head];

    /* Get timestamp (external function) */
    extern uint64_t timer_get_ticks(void);
    entry->timestamp = timer_get_ticks();
    entry->level = level;

    /* Copy source tag (max 7 chars + NUL) */
    {
        int i;
        for (i = 0; i < LOG_SOURCE_LEN - 1 && source[i]; i++)
            entry->source[i] = source[i];
        entry->source[i] = '\0';
    }

    /* Format message */
    __builtin_va_start(args, fmt);
    vsnprintf_simple(entry->message, LOG_MSG_LEN, fmt, args);
    __builtin_va_end(args);

    /* Advance ring buffer */
    log_head = (log_head + 1) % LOG_MAX_ENTRIES;
    log_total_count++;

    /* Count errors */
    if (level >= LOG_ERROR)
        log_error_total++;

    /* Print to UART with color */
    kernel_printf("%s[%5s]%s [%s] %s",
                  level_colors[level], level_names[level], COLOR_RESET,
                  entry->source, entry->message);
}

void log_dump(void)
{
    uint32_t i, count;
    uint32_t start;

    count = (log_total_count < LOG_MAX_ENTRIES) ? log_total_count : LOG_MAX_ENTRIES;
    start = (log_total_count < LOG_MAX_ENTRIES) ? 0 : log_total_count - LOG_MAX_ENTRIES;

    kernel_printf("=== Log Buffer (%d entries) ===", count);

    for (i = 0; i < count; i++) {
        log_entry_t *e = &log_buffer[(start + i) % LOG_MAX_ENTRIES];
        kernel_printf("  [%5s] t=%d [%s] %s",
                      level_names[e->level],
                      (int)e->timestamp,
                      e->source,
                      e->message);
    }
}

uint32_t log_count(void)
{
    return log_total_count;
}

uint32_t log_error_count(void)
{
    return log_error_total;
}
