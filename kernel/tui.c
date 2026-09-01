/*
 * AEOS - TUI (Text User Interface)
 *
 * Full-screen dashboard over the serial console: task table,
 * memory bars and the live AI/CMT engine state, refreshed on a
 * ~100ms cadence. q or Esc drops back to the shell.
 *
 * Pure ANSI escape codes - renders on any VT100-ish terminal.
 */

#include "tui.h"
#include "sched.h"
#include "memory.h"
#include "uart.h"
#include "ai_api.h"
#include "cmt.h"

#ifndef AEOS_IDLE_WAIT
#define AEOS_IDLE_WAIT() __asm__ volatile("nop")
#endif

#define ROWS 24
#define COLS 80

#define PANEL_TASKS_ROW 2
#define PANEL_MEM_ROW   14
#define PANEL_AI_ROW    18

/* ---- ANSI primitives ---------------------------------------------- */

void tui_enter(void)
{
    uart_puts_nolf("\033[?1049h\033[H\033[2J\033[?25l");
}

void tui_leave(void)
{
    uart_puts_nolf("\033[?25h\033[?1049l");
}

void tui_clear(void)
{
    uart_puts_nolf("\033[H\033[2J");
}

void tui_at(uint32_t row, uint32_t col)
{
    char buf[24];
    int i = 0;
    uint32_t v;
    char tmp[6];
    int n;

    buf[i++] = '\033';
    buf[i++] = '[';

    v = row + 1; n = 0;
    if (!v) tmp[n++] = '0';
    while (v) { tmp[n++] = (char)('0' + v % 10); v /= 10; }
    while (n) buf[i++] = tmp[--n];

    buf[i++] = ';';

    v = col + 1; n = 0;
    if (!v) tmp[n++] = '0';
    while (v) { tmp[n++] = (char)('0' + v % 10); v /= 10; }
    while (n) buf[i++] = tmp[--n];

    buf[i++] = 'H';
    buf[i] = '\0';
    uart_puts_nolf(buf);
}

void tui_color(int fg, int bold)
{
    char buf[12];

    buf[0] = '\033'; buf[1] = '[';
    buf[2] = bold ? '1' : '0';
    buf[3] = ';';
    buf[4] = '3';
    buf[5] = (char)('0' + fg);
    buf[6] = 'm';
    buf[7] = '\0';
    uart_puts_nolf(buf);
}

void tui_reset(void)
{
    uart_puts_nolf("\033[0m");
}

static void term_putc(char c)
{
    uart_putc(c);
}

void tui_box(uint32_t row, uint32_t col, uint32_t w, uint32_t h,
             const char *title)
{
    uint32_t x, y;

    tui_color(TUI_CYAN, 1);

    tui_at(row, col);
    term_putc(',');
    for (x = 1; x < w - 1; x++) term_putc('-');
    term_putc('.');

    for (y = 1; y < h - 1; y++) {
        tui_at(row + y, col);
        term_putc('|');
        for (x = 1; x < w - 1; x++) term_putc(' ');
        tui_at(row + y, col + w - 1);
        term_putc('|');
    }

    tui_at(row + h - 1, col);
    term_putc('`');
    for (x = 1; x < w - 1; x++) term_putc('-');
    term_putc('\'');

    if (title && title[0])
        tui_text(row, col + 2, title, w - 4);
    tui_reset();
}

void tui_text(uint32_t row, uint32_t col, const char *s, uint32_t maxw)
{
    tui_at(row, col);
    while (*s && maxw--) {
        if (*s == '\n') break;
        term_putc(*s++);
    }
}

void tui_bar(uint32_t row, uint32_t col, uint32_t width, int pct,
             int color)
{
    uint32_t filled, i;

    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    filled = ((uint32_t)pct * width) / 100;

    tui_at(row, col);
    tui_color(TUI_WHITE, 0);
    term_putc('[');
    tui_color(color, 1);
    for (i = 0; i < width; i++)
        term_putc(i < filled ? '#' : '.');
    tui_color(TUI_WHITE, 0);
    term_putc(']');
    tui_reset();
}

int tui_key(char *out)
{
    return uart_getc_nonblock(out);
}

void tui_num(uint32_t row, uint32_t col, uint32_t v)
{
    char tmp[11], out[12];
    int n = 0, j = 0;

    if (!v) tmp[n++] = '0';
    while (v) { tmp[n++] = (char)('0' + v % 10); v /= 10; }
    while (n) out[j++] = tmp[--n];
    out[j] = '\0';

    tui_text(row, col, out, 11);
}

static void pad_text(uint32_t row, uint32_t col,
                     const char *s, uint32_t width)
{
    uint32_t k = 0;

    tui_at(row, col);
    while (*s && k < width) { term_putc(*s++); k++; }
    while (k++ < width) term_putc(' ');
}

/* ---- Panels --------------------------------------------------------- */

static void panel_tasks(void)
{
    static const char *st[] = { "UNUSED", "READY ", "RUN   ", "FIN   " };
    int idx;
    uint32_t line = PANEL_TASKS_ROW;

    tui_color(TUI_YELLOW, 1);
    tui_text(line++, 3, "TASKS", 60);
    tui_reset();

    tui_color(TUI_CYAN, 0);
    tui_text(line++, 3, "id   name        state   ticks  yields", 70);
    tui_reset();

    for (idx = 0; idx < MAX_TASKS; idx++) {
        char name[TASK_NAME_LEN + 1];
        int state;
        uint32_t ticks, yields;

        if (sched_snapshot(idx, name, &state, &ticks, &yields) != 0)
            continue;

        pad_text(line, 3, "", 0);           /* set cursor */
        tui_num(line, 3, (uint32_t)idx);
        pad_text(line, 8, name, TASK_NAME_LEN + 1);
        tui_text(line, 22, st[state & 3], 6);
        tui_num(line, 31, ticks);
        tui_num(line, 40, yields);
        line++;
    }
    tui_reset();
}

static void panel_memory(void)
{
    size_t total_p = pmm_get_total_pages();
    size_t free_p = pmm_get_free_pages();
    size_t hu = heap_get_used();
    size_t hc = heap_get_used() + heap_get_free();
    int used_pct = total_p ?
        (int)(100 * (total_p - free_p) / total_p) : 0;
    int hpct = hc ? (int)(100 * hu / hc) : 0;

    tui_color(TUI_YELLOW, 1);
    tui_text(PANEL_MEM_ROW, 3, "MEMORY", 60);
    tui_reset();

    tui_bar(PANEL_MEM_ROW + 1, 3, 40, used_pct, TUI_GREEN);
    tui_text(PANEL_MEM_ROW + 1, 47, "physical pages", 20);

    tui_bar(PANEL_MEM_ROW + 2, 3, 40, hpct, TUI_MAGENTA);
    tui_text(PANEL_MEM_ROW + 2, 47, "kernel heap", 20);
}

static void panel_ai(void)
{
    extern uint64_t timer_get_ticks(void);

    tui_color(TUI_YELLOW, 1);
    tui_text(PANEL_AI_ROW, 3, "AI / CMT ENGINES", 60);
    tui_reset();

    tui_text(PANEL_AI_ROW + 1, 3, "CMT level ", 30);
    tui_color(TUI_GREEN, 1);
    tui_num(PANEL_AI_ROW + 1, 14, (uint32_t)cmt_consciousness_level());
    tui_reset();
    tui_text(PANEL_AI_ROW + 1, 17, " action ", 30);
    tui_color(TUI_CYAN, 1);
    tui_text(PANEL_AI_ROW + 1, 26, cmt_action_name(cmt_last_action()), 10);
    tui_reset();
    tui_text(PANEL_AI_ROW + 1, 38, " policy ", 30);
    tui_num(PANEL_AI_ROW + 1, 48, cmt_current_hz());
    tui_text(PANEL_AI_ROW + 1, 52, " Hz", 10);

    tui_text(PANEL_AI_ROW + 2, 3, "inferences ", 30);
    tui_color(TUI_MAGENTA, 1);
    tui_num(PANEL_AI_ROW + 2, 15, cmt_inference_count());
    tui_reset();
    tui_text(PANEL_AI_ROW + 2, 28, "uptime ticks ", 30);
    tui_num(PANEL_AI_ROW + 2, 42, (uint32_t)timer_get_ticks());
}

/* ---- Main loop ------------------------------------------------------- */

void tui_dashboard(void)
{
    char key;
    uint64_t last_frame = 0;

    tui_enter();
    tui_clear();
    tui_box(0, 0, COLS, ROWS, " AEOS DASHBOARD ");
    tui_text(ROWS - 1, 3, "[q] quit   refresh ~100ms", 50);

    panel_tasks();
    panel_memory();
    panel_ai();

    for (;;) {
        extern uint64_t timer_get_ticks(void);

        if (tui_key(&key) &&
            (key == 'q' || key == 'Q' || key == 27))
            break;

        if (timer_get_ticks() - last_frame >= 10) {
            last_frame = timer_get_ticks();
            panel_tasks();
            panel_memory();
            panel_ai();
        }
        AEOS_IDLE_WAIT();
    }

    tui_leave();
}
