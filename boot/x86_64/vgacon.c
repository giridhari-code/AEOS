/*
 * AEOS x86_64 - VGA text-mode console (0xB8000)
 *
 * Mirrors every serial character onto the classic 80x25 text
 * framebuffer so hypervisors (VirtualBox/VMware) and real monitors
 * show the boot live - no serial cable needed.
 */

#include <stdint.h>
#include <stddef.h>

#define VGA_MEM   ((volatile uint16_t *)0xB8000)
#define VGA_COLS  80
#define VGA_ROWS  25
#define ATTR      0x0F00                  /* white on black */

static int cur_row, cur_col;
static int ready;

static void vga_scroll(void)
{
    int i;

    for (i = 0; i < (VGA_ROWS - 1) * VGA_COLS; i++)
        VGA_MEM[i] = VGA_MEM[i + VGA_COLS];
    for (i = (VGA_ROWS - 1) * VGA_COLS; i < VGA_ROWS * VGA_COLS; i++)
        VGA_MEM[i] = ATTR | ' ';
}

void vgacon_init(void)
{
    int i;

    for (i = 0; i < VGA_ROWS * VGA_COLS; i++)
        VGA_MEM[i] = ATTR | ' ';
    cur_row = cur_col = 0;
    ready = 1;

    /* hardware cursor: park at bottom-right, we manage visually */
    __asm__ volatile ("outb %b0, %w1" :: "a"((unsigned)0x0F),
                      "Nd"((unsigned)0x3D4));
    __asm__ volatile ("outb %b0, %w1" :: "a"((unsigned)((VGA_ROWS-1)*VGA_COLS+VGA_COLS-1) & 0xFF),
                      "Nd"((unsigned)0x3D5));
}

void vgacon_putc(char c)
{
    if (!ready)
        return;

    if (c == '\n') {
        cur_col = 0;
        cur_row++;
    } else if (c == '\r') {
        cur_col = 0;
    } else {
        VGA_MEM[cur_row * VGA_COLS + cur_col] = ATTR | (uint8_t)c;
        cur_col++;
    }

    if (cur_col >= VGA_COLS) {
        cur_col = 0;
        cur_row++;
    }
    if (cur_row >= VGA_ROWS) {
        vga_scroll();
        cur_row = VGA_ROWS - 1;
    }
}
