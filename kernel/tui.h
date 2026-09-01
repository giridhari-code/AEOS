/*
 * AEOS - TUI (Text User Interface)
 *
 * A tiny VT100/ANSI toolkit for the serial console: cursor
 * positioning, SGR colors, box drawing and a full-screen live
 * dashboard. Everything is plain escape codes - works on any
 * terminal emulator attached to the UART.
 */

#ifndef AEOS_TUI_H
#define AEOS_TUI_H

#include <stdint.h>
#include <stddef.h>

/* Colors (SGR codes). */
#define TUI_BLACK   0
#define TUI_RED     1
#define TUI_GREEN   2
#define TUI_YELLOW  3
#define TUI_BLUE    4
#define TUI_MAGENTA 5
#define TUI_CYAN    6
#define TUI_WHITE   7

void tui_enter(void);               /* alt screen + hide cursor  */
void tui_leave(void);               /* restore terminal          */
void tui_clear(void);
void tui_at(uint32_t row, uint32_t col);
void tui_color(int fg, int bold);
void tui_reset(void);

void tui_box(uint32_t row, uint32_t col, uint32_t w, uint32_t h,
             const char *title);
void tui_text(uint32_t row, uint32_t col, const char *s, uint32_t maxw);

/* Progress bar: `width` cells, filled = pct(0..100). */
void tui_bar(uint32_t row, uint32_t col, uint32_t width, int pct,
             int color);

/* Non-blocking key read: 1 = got key in *out. */
int  tui_key(char *out);

/* Full-screen dashboard; returns when user presses q/Esc. */
void tui_dashboard(void);

#endif /* AEOS_TUI_H */
