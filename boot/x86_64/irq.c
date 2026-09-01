/*
 * AEOS x86_64 - IDT, PIC remap, PIT timer
 * Same public API as the ARM irq.c: tick_subscribe + friends.
 */

#include "memory.h"
#include <stdint.h>

/* ---- IDT ---- */
#define IDT_ENTRIES 256

typedef struct {
    uint16_t offset_lo;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  flags;          /* present | ring0 | interrupt gate */
    uint16_t offset_mid;
    uint32_t offset_hi;
    uint32_t zero;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idt_ptr_t;

static idt_entry_t idt[IDT_ENTRIES];
static idt_ptr_t   idtp;

extern void *vector_table[];

static void idt_set(int n, void *handler)
{
    uint64_t off = (uint64_t)(uintptr_t)handler;
    idt[n].offset_lo = (uint16_t)(off & 0xFFFF);
    idt[n].selector  = 0x08;
    idt[n].ist       = 0;
    idt[n].flags     = 0x8E;             /* P|DPL0|interrupt gate */
    idt[n].offset_mid = (uint16_t)((off >> 16) & 0xFFFF);
    idt[n].offset_hi  = (uint32_t)(off >> 32);
    idt[n].zero       = 0;
}

/* Ring-3 callable gate (syscall trap): DPL=3 so user code may
 * raise this vector via int. */
void idt_set_user_gate(int n, void *handler)
{
    uint64_t off = (uint64_t)(uintptr_t)handler;
    idt[n].offset_lo = (uint16_t)(off & 0xFFFF);
    idt[n].selector  = 0x08;
    idt[n].ist       = 0;
    idt[n].flags     = 0xEE;             /* P|DPL3|interrupt gate */
    idt[n].offset_mid = (uint16_t)((off >> 16) & 0xFFFF);
    idt[n].offset_hi  = (uint32_t)(off >> 32);
    idt[n].zero       = 0;
}

static inline void outb(uint16_t p, uint8_t v)
{
    __asm__ volatile ("outb %0, %1" : : "a"(v), "Nd"(p));
}
static inline uint8_t inb(uint16_t p)
{
    uint8_t v;
    __asm__ volatile ("inb %1, %0" : "=a"(v) : "Nd"(p));
    return v;
}

/* ---- PIC ---- */
#define PIC1_CMD 0x20
#define PIC1_DAT 0x21
#define PIC2_CMD 0xA0
#define PIC2_DAT 0xA1
#define PIC_EOI  0x20

static void pic_remap(void)
{
    outb(PIC1_CMD, 0x11);                /* init, ICW4 needed */
    outb(PIC2_CMD, 0x11);
    outb(PIC1_DAT, 0x20);                /* vector base 32 */
    outb(PIC2_DAT, 0x28);                /* vector base 40 */
    outb(PIC1_DAT, 0x04);                /* slave on IRQ2 */
    outb(PIC2_DAT, 0x02);
    outb(PIC1_DAT, 0x01);
    outb(PIC2_DAT, 0x01);
    outb(PIC1_DAT, 0xFE);                /* unmask IRQ0 only */
    outb(PIC2_DAT, 0xFF);                /* mask all slave */
}

static void pic_eoi(int irq)
{
    if (irq >= 8)
        outb(PIC2_CMD, PIC_EOI);
    outb(PIC1_CMD, PIC_EOI);
}

/* ---- PIT @ 100Hz ---- */
#define PIT_CH0 0x40
#define PIT_CMD 0x43
#define PIT_HZ  100
#define PIT_FREQ 1193182

/* ---- Tick registry (same as ARM) ---- */
#define MAX_TICK_TASKS 4

static void (*tick_tasks[MAX_TICK_TASKS])(void);
static uint32_t tick_task_count;

int tick_subscribe(void (*fn)(void))
{
    if (fn == 0 || tick_task_count >= MAX_TICK_TASKS)
        return -1;
    tick_tasks[tick_task_count++] = fn;
    return 0;
}

static void tick_dispatch(void)
{
    uint32_t i;
    for (i = 0; i < tick_task_count; i++)
        if (tick_tasks[i])
            tick_tasks[i]();
}

/* ---- tick counters (timer.c equivalent) ---- */
static volatile uint64_t g_ticks;

uint64_t timer_get_ticks(void) { return g_ticks; }
uint64_t timer_get_ms(void)    { return g_ticks * 10; }

/* Phase 1: PIT runs at a fixed 100Hz; policy changes are recorded
 * but the divisor reprogram is left for the APIC/tsc port. */
static uint32_t g_hz = 100;
void timer_set_hz(uint32_t hz) {
    if (hz >= 20 && hz <= 500) {
        g_hz = hz;
        /* Reprogram PIT with new divisor */
        uint16_t div = (uint16_t)(PIT_FREQ / hz);
        outb(PIT_CMD, 0x36);           /* Channel 0, lo/hi, mode 3 */
        outb(PIT_CH0, (uint8_t)(div & 0xFF));
        outb(PIT_CH0, (uint8_t)(div >> 8));
    }
}
uint32_t timer_get_hz(void)    { return g_hz; }

/* ---- Preemption arm (set by timer tick, consumed by stubs) ---- */
void *g_prend;                       /* -> preempt_thunk when armed */

int sched_should_preempt(void);      /* kernel/sched.c */
extern char preempt_thunk;

/* ---- C entry from every stub ----
 * Frame layout (from rsp): rax rcx rdx rsi rdi r8 r9 r10 r11,
 * then the pushed vector number, then errcode/dummy.
 * => index 9 holds the vector. */
void irq_handler_c(uint64_t *frame)
{
    uint64_t vector = frame[9];

    if (vector == 32) {                  /* PIT */
        g_ticks++;
        tick_dispatch();
        /* Quantum expired with someone waiting? Force a switch by
         * redirecting the interrupted task through preempt_thunk. */
        {
            extern int g_in_thunk;

            /* Redirect ONLY kernel-origin frames: a thunk must run
             * at CPL0, and ring-3 frames use their own private
             * rsp0 stack anyway. NOTE: EOI below must ALWAYS run,
             * otherwise the PIC blocks every future tick. */
            if (frame[11] == 0x08 && sched_should_preempt() &&
                !g_in_thunk && !g_prend)
                g_prend = (void *)&preempt_thunk;
        }
        pic_eoi(0);
        return;
    }

    if (vector < 32) {
        static const char *names[6] = {
            "DIV-BY-ZERO", "DEBUG", "NMI", "BREAKPOINT", "OVERFLOW", "BOUND"
        };
        const char *n = (vector < 6) ? names[vector] : "EXCEPTION";
        irq_disable();
        uart_puts("");
        uart_puts("*** KERNEL FAULT ***");
        unsigned long cr2 = 0;

        __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
        uart_puts_nolf("  Vector "); uart_dec((unsigned)vector);
        uart_puts_nolf(" ("); uart_puts(n); uart_puts(")");
        uart_puts_nolf("  CR2=0x");
        {
            extern void uart_hex64(unsigned long v);
            uart_hex64(cr2);
        }
        uart_puts("");
        uart_puts("System halted.");
        for (;;)
            __asm__ volatile ("hlt");
    }
}

void irq_init(void)
{
    int i;

    for (i = 0; i <= 32; i++)
        idt_set(i, vector_table[i]);

    idtp.limit = sizeof(idt) - 1;
    idtp.base  = (uint64_t)&idt[0];
    __asm__ volatile ("lidt %0" : : "m"(idtp));

    pic_remap();

    /* PIT channel 0, mode 3 (square wave), 100Hz */
    uint16_t div = (uint16_t)(PIT_FREQ / PIT_HZ);
    outb(PIT_CMD, 0x36);
    outb(PIT_CH0, (uint8_t)(div & 0xFF));
    outb(PIT_CH0, (uint8_t)(div >> 8));

    irq_enable();
}
