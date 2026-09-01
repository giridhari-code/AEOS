/*
 * AEOS - Kernel Shell
 *
 * Commands are dispatched against live kernel subsystems:
 *   help | ps | mem | ai | uptime | ticks | about | clear
 *
 * The shell is just a task: while waiting for input it yields, so
 * workers keep running. Input is polled from the PL011 RX FIFO.
 */

#include "shell.h"
#include "sched.h"
#include "memory.h"
#include "uart.h"
#include "ai.h"
#include "proc.h"
#include "afs.h"
#include "tui.h"
#ifdef AEOS_HAS_VMM
#include "smp.h"
#endif
#include "psci.h"

#define LINE_MAX 64

static char line[LINE_MAX];
static unsigned line_len;

/* ============================================================
 * Line handling
 * ============================================================ */

static void prompt(void)
{
    uart_puts_nolf("aeos> ");
}

static void line_reset(void)
{
    line_len = 0;
    line[0] = '\0';
    prompt();
}

static void line_backspace(void)
{
    if (line_len == 0)
        return;
    line_len--;
    /* Erase the character on the terminal: BS, space, BS. */
    uart_putc('\b');
    uart_putc(' ');
    uart_putc('\b');
}

static void line_accept_char(char c)
{
    if (line_len >= LINE_MAX - 1)
        return;
    line[line_len++] = c;
    line[line_len] = '\0';
    uart_putc(c);                        /* echo */
}

/* ============================================================
 * Commands
 * ============================================================ */

/* First token of the line (in-place split). */
static char *cmd_arg(void)
{
    unsigned i;

    for (i = 0; i < line_len; i++)
        if (line[i] == ' ')
            break;
    if (i == line_len)
        return (char *)0;
    line[i] = '\0';
    while (line[i + 1] == ' ')
        i++;
    if (!line[i + 1])
        return (char *)0;
    return &line[i + 1];
}

static void cmd_ls(void)
{
    afs_ls();
}

static void cmd_cat(const char *name)
{
    static char buf[2048];
    int n;
    unsigned i;

    n = afs_read(name, buf, sizeof(buf) - 1);
    if (n < 0) {
        uart_puts_nolf("  no such file: ");
        uart_puts(name);
        return;
    }
    buf[n] = '\0';
    for (i = 0; i < (unsigned)n; i++)
        uart_putc(buf[i]);
    if (n && buf[n - 1] != '\n')
        uart_puts("");
}

static void memcpy_str(char *dst, const char *src)
{
    while (*src)
        *dst++ = *src++;
    *dst = '\0';
}

static void snprintf_payload(char *dst, const char *name)
{
    static const char text[] =
        "written by AEOS shell - persistent storage works!";

    (void)name;
    memcpy_str(dst, text);
}

static unsigned strlen_(const char *s)
{
    unsigned n = 0;

    while (s[n])
        n++;
    return n;
}

static void cmd_write(const char *name)
{
    static char payload[1024];

    snprintf_payload(payload, name);
    {
        int n = afs_write(name, payload, strlen_(payload));

        if (n < 0)
            uart_puts("  write failed (fs full or not mounted)");
        else {
            uart_puts_nolf("  wrote ");
            uart_dec((unsigned)n);
            uart_puts_nolf(" bytes to ");
            uart_puts(name);
        }
    }
}

static void cmd_rm(const char *name)
{
    if (afs_delete(name) != 0)
        uart_puts("  delete failed");
}

/*
 * Self-update: re-write the RUNNING kernel image back to the boot
 * volume. The linker symbol _kernel_image_end marks the end of the
 * loaded binary (text+rodata+data+got), so the copy is byte-exact
 * with what disktool.py would have provisioned. After the next
 * reboot the BIOS loads THIS image - the OS bootstrapped itself.
 *
 * Integrity check: compute simple checksum before write, verify after.
 */
static uint32_t simple_checksum(const uint8_t *data, size_t len)
{
    uint32_t sum = 0;
    size_t i;
    for (i = 0; i < len; i++)
        sum = (sum << 1) ^ data[i];
    return sum;
}

static void cmd_install(void)
{
    extern uint8_t _kernel_start[];
    extern uint8_t _kernel_image_end[];
    size_t kernel_size = (size_t)(_kernel_image_end - _kernel_start);
    uint32_t checksum_before, checksum_after;

    /* Refuse if kernel image is too large to verify (safety interlock) */
    if (kernel_size > 1024 * 1024) {
        uart_puts("  REFUSE: kernel image too large for integrity check");
        return;
    }

    /* Compute checksum of source image */
    checksum_before = simple_checksum(_kernel_start, kernel_size);
    uart_puts_nolf("  checksum before: 0x");
    {
        extern void uart_hex32(uint32_t v);
        uart_hex32(checksum_before);
    }
    uart_puts("");

    /* Write to disk */
    long n = afs_write("kernel.bin", _kernel_start, kernel_size);
    if (n < 0) {
        uart_puts("  install FAILED (fs not mounted or full)");
        return;
    }
    uart_puts_nolf("  self-update: ");
    uart_dec((unsigned)n);
    uart_puts(" bytes written to kernel.bin");

    /* Verify: read back first 4096 bytes and compare checksum of that region.
     * Full-image verification would need a buffer equal to kernel_size,
     * which is too large for stack allocation. Instead, verify the header. */
    {
        static char verify_buf[4096];
        int read_back = afs_read("kernel.bin", verify_buf, sizeof(verify_buf));
        if (read_back < 0) {
            uart_puts("  WARNING: could not verify (read failed)");
        } else {
            checksum_after = simple_checksum((const uint8_t *)verify_buf,
                                             (size_t)read_back);
            uart_puts_nolf("  checksum after:  0x");
            {
                extern void uart_hex32(uint32_t v);
                uart_hex32(checksum_after);
            }
            uart_puts("");
            if (checksum_before == checksum_after) {
                uart_puts("  integrity check PASSED");
                uart_puts("  reboot to activate");
            } else {
                uart_puts("  WARNING: checksum mismatch - write may be corrupted");
                uart_puts("  reboot to retry (original image unchanged in RAM)");
            }
        }
    }
}

static void cmd_cores(void)
{
#ifdef AEOS_HAS_VMM
    smp_report();
#else
    uart_puts("  SMP not supported on this port");
#endif
}

static void cmd_tui(void)
{
    tui_dashboard();
    uart_puts("  back to shell");
}

static void cmd_reboot(void)
{
    uart_puts("  rebooting...");
    psci_system_reset();                /* never returns */
}

static void cmd_help(void)
{
    uart_puts("commands:");
    uart_puts("  help          this text");
    uart_puts("  ps            task table");
    uart_puts("  procs         process table");
    uart_puts("  mem           memory stats");
    uart_puts("  ai            AI engine report");
    uart_puts("  ls            list files (AEOS-FS)");
    uart_puts("  cat NAME      print file contents");
    uart_puts("  write NAME    store demo text as file");
    uart_puts("  rm NAME       delete a file");
    uart_puts("  cores         SMP core report");
    uart_puts("  tui           full-screen dashboard");
    uart_puts("  install       write running kernel to disk");
    uart_puts("  reboot        reset via PSCI (BIOS reruns)");
    uart_puts("  uptime        ticks since boot");
    uart_puts("  about         banner");
    uart_puts("  clear         clear screen");
}

static void cmd_ps(void)
{
    sched_report();
}

static void cmd_procs(void)
{
    proc_report();
}

static void cmd_mem(void)
{
    uart_puts_nolf("  pmm: ");
    uart_dec(pmm_get_free_pages());
    uart_puts_nolf("/");
    uart_dec(pmm_get_total_pages());
    uart_puts(" pages free");

    uart_puts_nolf("  heap used: ");
    uart_dec(heap_get_used());
    uart_puts(" bytes");
    uart_puts_nolf("  heap free: ");
    uart_dec(heap_get_free());
    uart_puts(" bytes");
}

static void cmd_uptime(void)
{
    uint32_t t = sched_uptime_ticks();

    uart_puts_nolf("  uptime: ");
    uart_dec(t);
    uart_puts_nolf(" ticks (~");
    uart_dec(t / 100);
    uart_puts(" s)");
}

static void cmd_about(void)
{
    uart_puts("AEOS - Ajeeb Embodied AI OS");
    uart_puts("Phase 0 kernel: boot/mem/irq/AI/sched/shell");
}

static int strncmp2(const char *line, const char *prefix)
{
    while (*prefix) {
        if (*line != *prefix)
            return 0;
        line++;
        prefix++;
    }
    return 1;
}

static int streq(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

static void execute(void)
{
    uart_puts("");

    if (line_len == 0) {
        prompt();
        return;
    }

    if (streq(line, "help")) {
        cmd_help();
    } else if (streq(line, "ps")) {
        cmd_ps();
    } else if (streq(line, "procs")) {
        cmd_procs();
    } else if (streq(line, "ls")) {
        cmd_ls();
    } else if (streq(line, "install")) {
        cmd_install();
    } else if (streq(line, "cores")) {
        cmd_cores();
    } else if (streq(line, "tui")) {
        cmd_tui();
    } else if (streq(line, "reboot")) {
        cmd_reboot();
    } else if (strncmp2(line, "cat ")) {
        char *a = cmd_arg();

        if (a)
            cmd_cat(a);
        else
            uart_puts("  usage: cat NAME");
    } else if (strncmp2(line, "write ")) {
        char *a = cmd_arg();

        if (a)
            cmd_write(a);
        else
            uart_puts("  usage: write NAME");
    } else if (strncmp2(line, "rm ")) {
        char *a = cmd_arg();

        if (a)
            cmd_rm(a);
        else
            uart_puts("  usage: rm NAME");
    }
    else if (streq(line, "mem"))
        cmd_mem();
    else if (streq(line, "ai"))
        ai_report();
    else if (streq(line, "uptime"))
        cmd_uptime();
    else if (streq(line, "about"))
        cmd_about();
    else if (streq(line, "clear")) {
        uart_puts("\033[2J\033[H");
    } else {
        uart_puts_nolf("unknown command: ");
        uart_puts(line);
        uart_puts("(try 'help')");
    }

    line_reset();
}

/* ============================================================
 * Shell task body
 * ============================================================ */

static void shell_task(void)
{
    char c;

    uart_puts("");
    uart_puts("Shell ready. Type 'help'.");
    line_reset();

    for (;;) {
        /*
         * Poll one byte at a time; yield when the RX FIFO is empty
         * so other tasks keep running while the user types.
         */
        if (!uart_getc_nonblock(&c)) {
            sched_yield();
            continue;
        }

        if (c == '\r' || c == '\n') {
            execute();
        } else if (c == 0x7F || c == '\b') {
            line_backspace();
        } else if (c >= 0x20 && c < 0x7F) {
            line_accept_char(c);
        }
        /* other control chars ignored */
    }
}

int shell_start(void *stack, unsigned stack_len)
{
    return sched_spawn("shell", shell_task, stack, stack_len);
}
