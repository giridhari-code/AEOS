/*
 * AEOS x86_64 - Kernel main
 * Portable subsystems (AI/CMT/sched/shell/ramfs/ipc) reused as-is;
 * only the boot/HAL layer is architecture specific.
 */

#include "memory.h"
#include "ai.h"
#include "cmt.h"
#include "device.h"
#include "ramfs.h"
#include "ipc.h"
#include "sched.h"
#include "sync.h"
#include "shell.h"

void x86_kernel_main(uint64_t magic, uint64_t mbi);

/* ============================================================
 * Ring 3 support: TSS + user-mode entry + INT 0x80 syscalls
 * ============================================================ */

typedef struct {
    uint32_t reserved0;
    uint64_t rsp0;           /* kernel stack for interrupts from ring 3 */
    uint64_t rsp1, rsp2;
    uint64_t reserved1;
    uint16_t iomap_base;
} __attribute__((packed)) tss_t;

extern void *vector_table[];
extern void vector_128(void);
extern const char gdt64[];

void idt_set_user_gate(int n, void *handler);

static uint8_t tss_kernel_stack[16384] __attribute__((aligned(16)));
static tss_t tss;

/* Reload GDT (same layout) + patch TSS desc at 0x28 + ltr. */
static void tss_install(void)
{
    uint64_t base = (uint64_t)(uintptr_t)&tss;
    volatile uint64_t *desc = (volatile uint64_t *)(gdt64 + 0x28);

    tss.rsp0 = (uint64_t)tss_kernel_stack + sizeof(tss_kernel_stack);
    tss.iomap_base = sizeof(tss);

    /* 64-bit TSS descriptor: limit=sizeof-1, base split low/high */
    desc[0] = ((base & 0xFFFFFFULL) << 16)
            | (((sizeof(tss) - 1) & 0xFFFFULL))
            | (0x89ULL << 40)
            | (((base >> 24) & 0xFFULL) << 32);
    desc[1] = (base >> 32) & 0xFFFFFFFFULL;

    __asm__ volatile (
        "lgdt gdt64_ptr\n"
        "pushq $0x08\n"
        "leaq 1f(%%rip), %%rax\n"
        "pushq %%rax\n"
        "lretq\n"
        "1:\n"
        "movw $0x10, %%ax\n"
        "movw %%ax, %%ds\n"
        "movw %%ax, %%es\n"
        "movw %%ax, %%ss\n"
        "xorw %%ax, %%ax\n"
        "movw %%ax, %%fs\n"
        "movw %%ax, %%gs\n"
        "movw $0x28, %%ax\n"
        "ltr %%ax\n"
        ::: "rax", "memory");
}

void tss_set_rsp0(uint64_t v)
{
    tss.rsp0 = v;
}

/* ---- INT 0x80 syscall gate ----
 * Stub pushes HIGH-to-LOW: rax first ... r11 last. rsp lands on
 * r11, so indices are MIRRORED vs push order:
 *   [0]r11 [1]r10 [2]r9 [3]r8 [4]rdi [5]rsi [6]rdx [7]rcx [8]rax
 *   [9]vector [10]errcode [11]rip .. [15]ss.
 * Convention: number+result in RAX(frame[8]), arg in RDI(frame[4]). */
#define X86_SYS_PRINT 1
#define X86_SYS_YIELD 3
#define X86_SYS_EXIT  4

void int80_handler_c(uint64_t *frame)
{
    switch (frame[8]) {
    case X86_SYS_PRINT: {
        const char *s = (const char *)(uintptr_t)frame[4];
        unsigned n;
        if ((uintptr_t)s < RAM_START || (uintptr_t)s >= RAM_END)
            break;
        for (n = 0; n < 256 && s[n]; n++)
            uart_putc(s[n]);
        frame[8] = n;
        break;
    }
    case X86_SYS_YIELD:
        sched_yield();                   /* resumes after iretq */
        break;
    case X86_SYS_EXIT:
        sched_exit();                    /* never returns */
        break;
    default:
        frame[8] = (uint64_t)-1;
        break;
    }
}

/* ---- drop to ring 3 ---- */
static void (*user_entry_pending)(void);
static uint64_t user_sp_pending;
static int user_stack_used;
static uint8_t user_stack[4096] __attribute__((aligned(16)));
#define USER_KSTACK_SLOTS 4
static uint8_t user_kstacks[USER_KSTACK_SLOTS][4096]
    __attribute__((aligned(16)));

void arch_tss_set_rsp0(uint64_t v);



static void enter_user_mode(void (*fn)(void), uint64_t sp)
{
    __asm__ volatile (
        "cli\n"
        "mov %1, %%rax\n"                /* user stack top */
        "pushq $0x23\n"                  /* SS = 0x20|3 */
        "pushq %%rax\n"                  /* RSP */
        "pushq $0x202\n"                 /* RFLAGS: IF=1 */
        "pushq $0x1B\n"                  /* CS = 0x18|3 */
        "pushq %0\n"                     /* RIP = fn */
        "iretq\n"
        :: "r"(fn), "r"(sp) : "rax", "memory");
}

static void user_task_start(void)
{
    enter_user_mode(user_entry_pending, user_sp_pending);
}

int sched_spawn_user(const char *name, void (*entry)(void),
                     void *kstack, unsigned kstack_len)
{
    if (user_stack_used)
        return -1;
    user_stack_used = 1;
    user_entry_pending = entry;
    user_sp_pending = ((uint64_t)user_stack + sizeof(user_stack)) & ~0xFULL;

    {
        int tidx = sched_spawn(name, user_task_start, kstack,
                               kstack_len);

        /* Private rsp0 stack for this task: ring3 interrupt frames
         * land HERE, never on a buffer another suspended task is
         * still using. Switched per-task by arch_pre_switch(). */
        if (tidx >= 0)
            sched_task_set_kstack(
                tidx, (uint64_t)(uintptr_t)user_kstacks[0] +
                          sizeof(user_kstacks[0]));
        return tidx;
    }
}

/* ---- helpers ---- */
static void spin_ms(uint64_t ms)
{
    uint64_t start = timer_get_ms();
    while ((timer_get_ms() - start) < ms)
        ;
}

/* ---- worker tasks ---- */
static uint8_t stack_a[4096] __attribute__((aligned(16)));
static uint8_t stack_c[4096] __attribute__((aligned(16)));
static uint8_t stack_shell[4096] __attribute__((aligned(16)));
static uint8_t stack_prod[4096] __attribute__((aligned(16)));
static uint8_t stack_cons[4096] __attribute__((aligned(16)));
static uint8_t stack_sysinfo[4096] __attribute__((aligned(16)));
static uint8_t stack_ailog[2048] __attribute__((aligned(16)));
static uint8_t stack_devmon[2048] __attribute__((aligned(16)));
static uint8_t stack_user[2048] __attribute__((aligned(16)));
static uint8_t stack_hog[2048] __attribute__((aligned(16)));

/* PREEMPTION PROOF: never yields, never sleeps. Under the old
 * cooperative-only model the watchdog would flag this task as
 * rogue and kill it at the 6s mark while everything else starved.
 * With quantum preemption it just keeps getting time-sliced. */
static volatile unsigned hog_loops;
static void task_hog(void)
{
    for (;;)
        hog_loops++;                   /* pure CPU burn, no yield */
}

/* Ring-3 user task: only INT 0x80 reaches the kernel. */
static const char user_msg[] = "  [user] hello from ring 3!\n";

static void sys_print_user(const char *s)
{
    register long n __asm__("rax") = 1;              /* SYS_PRINT */
    register const char *p __asm__("rdi") = s;
    __asm__ volatile ("int $0x80" : "+r"(n) : "r"(p) : "memory");
}

static void sys_yield_user(void)
{
    __asm__ volatile ("mov $3, %%eax\n int $0x80" :: : "eax", "memory");
}

static void user_hello(void)
{
    for (;;) {
        sys_print_user(user_msg);
        spin_ms(1000);
        sys_yield_user();
    }
}

static void task_a(void)
{
    unsigned n = 0;
    for (;;) {
        uart_puts_nolf("  [A] working #");
        uart_dec(n++);
        uart_puts("");
        spin_ms(400);
        sched_yield();
    }
}

static void task_c(void)
{
    int n = 0;
    for (;;) {
        uart_puts_nolf("          [C] AI check #");
        uart_dec(n++);
        uart_puts_nolf(": ");
        uart_dec(ai_inference_count());
        uart_puts(" inferences");
        spin_ms(1000);
        sched_yield();
    }
}

static mutex_t     data_lock;
static semaphore_t items;
static volatile int shared_item;

static void task_producer(void)
{
    int i;
    for (i = 1; i <= 5; i++) {
        mutex_lock(&data_lock);
        shared_item = i;
        mutex_unlock(&data_lock);
        sem_post(&items);
        uart_puts("  [prod] produced item");
        spin_ms(300);
        sched_yield();
    }
    uart_puts("  [prod] producer done");
    sched_exit();
}

static void task_consumer(void)
{
    int i, v;
    for (i = 0; i < 5; i++) {
        sem_wait(&items);
        mutex_lock(&data_lock);
        v = shared_item;
        mutex_unlock(&data_lock);
        uart_puts_nolf("  [cons] consumed item ");
        uart_dec((unsigned)v);
        uart_puts("");
        sched_yield();
    }
    uart_puts("  [cons] consumer done");
    sched_exit();
}

static void task_sysinfo(void)
{
    const char *info = "AEOS x86_64 v1.0";
    uint32_t up;

    uart_puts("  [sysinfo] Writing system info...");
    ramfs_write("sys/info", info, 17, 0);
    up = (uint32_t)timer_get_ticks();
    ramfs_write("sys/uptime", &up, 4, 0);
    uart_puts("  [sysinfo] Done");
    sched_exit();
}

static void task_ailog(void)
{
    int last = -1;

    uart_puts("  [ailog] AI decision logger started");
    for (;;) {
        int action = ai_last_action();
        if (action != last) {
            uart_puts_nolf("  [ailog] Action changed to: ");
            uart_puts(ai_action_name(action));
            last = action;
        }
        uint8_t lvl = (uint8_t)cmt_consciousness_level();
        ramfs_write("cmt/state", &lvl, 1, 0);
        spin_ms(500);
        sched_yield();
    }
}

static void task_devmon(void)
{
    int count = 0;

    uart_puts("  [devmon] Device monitor started");
    while (count < 5) {
        uint32_t files = 0;
        files += ramfs_exists("sys/info") ? 1 : 0;
        files += ramfs_exists("sys/uptime") ? 1 : 0;
        uart_puts_nolf("  [devmon] Files: ");
        uart_dec(files);
        uart_puts_nolf("  IPC queues: ");
        uart_dec(ipc_queue_count());
        uart_puts("");
        count++;
        spin_ms(800);
        sched_yield();
    }
    uart_puts("  [devmon] Monitor finished");
    sched_exit();
}

void x86_kernel_main(uint64_t magic, uint64_t mbi)
{

    extern void vgacon_init(void);
    vgacon_init();   /* screen mirrors serial from here on */
    (void)mbi;

    uart_init();
    uart_puts("");
    uart_puts("=========================================");
    uart_puts("  AEOS - Ajeeb Embodied AI OS  [x86_64]");
    uart_puts("=========================================");
    uart_puts("AEOS kernel booted");
    uart_puts_nolf("  multiboot magic=0x");
    uart_hex(magic);
    uart_puts("");

    /* ---- AI + CMT ---- */
    uart_puts("[1/5] Initializing AI engine...");
    ai_init();

    uart_puts("[2/5] Initializing CMT engine...");
    cmt_init();

    /* ---- devices / fs / ipc ---- */
    uart_puts("[3/5] Device framework...");
    device_init();
    ramfs_init();
    ramfs_create("sys/info");
    ramfs_create("sys/uptime");
    ramfs_create("cmt/state");
    ipc_init();
    ipc_create(0);

    /* ---- IRQ + timer ---- */
    uart_puts("[4/5] IRQ + PIT timer (100Hz)...");
    irq_init();

    /* Ring 3: TSS + INT 0x80 gate for user tasks.
     * NOTE: direct symbol — vector_table[] only covers 0..32. */
    tss_install();
    idt_set_user_gate(128, vector_128);
    uart_puts("      Ring 3 ready (TSS + int80 gate)");

    /* ---- live demo window ---- */
    uart_puts("[5/5] AI + CMT live (1.5s)...");
    ai_set_verbose(1);
    cmt_set_verbose(1);
    spin_ms(1500);
    ai_set_verbose(0);
    cmt_set_verbose(0);

    /* ---- scheduler ---- */
    uart_puts("");
    uart_puts("=== Scheduler + Shell ===");
    sched_init("main");

    mutex_init(&data_lock);
    sem_init(&items, 0);

    sched_spawn("sysinfo", task_sysinfo, stack_sysinfo, sizeof(stack_sysinfo));
    sched_spawn("ailog",   task_ailog,   stack_ailog,   sizeof(stack_ailog));
    sched_spawn("devmon",  task_devmon,  stack_devmon,  sizeof(stack_devmon));
    sched_spawn("worker-a", task_a,      stack_a,       sizeof(stack_a));
    sched_spawn("ai-mon",  task_c,       stack_c,       sizeof(stack_c));
    shell_start(stack_shell, sizeof(stack_shell));
    sched_spawn("producer", task_producer, stack_prod,  sizeof(stack_prod));
    sched_spawn("consumer", task_consumer, stack_cons, sizeof(stack_cons));
    uart_puts_nolf("  Spawn user task (ring 3): ");
    uart_dec(sched_spawn_user("user-hello", user_hello,
                              stack_user, sizeof(stack_user)));
    uart_puts("");
    uart_puts_nolf("  Spawn hog (never yields): ");
    uart_dec(sched_spawn("hog", task_hog, stack_hog, sizeof(stack_hog)));
    uart_puts("");

    {
        unsigned round = 0;
        while (round < 3) {
            uart_puts_nolf("[main] round ");
            uart_dec(round++);
            uart_puts("");
            spin_ms(1200);
            sched_yield();
        }

        uart_puts("");
        uart_puts("--- System Report ---");
        sched_report();
        ai_report();

        uart_puts("");
        uart_puts("Main exiting; shell + workers keep running...");
        sched_exit();
    }
}
