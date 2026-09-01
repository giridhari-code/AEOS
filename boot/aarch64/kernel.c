/*
 * AEOS - Kernel
 * Architecture: ARM64 (AArch64)
 * Target: QEMU virt machine
 *
 * kernel_main: memory management init + interrupt bring-up +
 * the in-kernel AI core (perceive-decide-act on timer ticks).
 */

#include "memory.h"
#include "uart.h"
#include "ai.h"
#include "cmt.h"
#include "device.h"
#include "ramfs.h"
#include "ipc.h"
#include "sched.h"
#include "sync.h"
#include "shell.h"
#include "syscall.h"
#include "bootinfo.h"
#include "proc.h"
#include "virtio.h"
#include "afs.h"
#include "smp.h"
#include "net.h"

/* Embedded EL0 program images (generated from the users/ sources
 * at build time; linked at USER_VA_BASE by users/user.ld). */
#include "userprog_ping.h"
#include "userprog_pong.h"

/* Forward declarations */
extern void pmm_init(void);
extern void vmm_init(void);
extern void vmm_enable_mmu(void);
extern void heap_init(void);
extern void irq_init(void);

/* Timer callback for testing */
static volatile int timer_fired = 0;
static void test_timer_callback(void) {
    timer_fired++;
}

/* ============================================================
 * Demo tasks for the scheduler
 * ============================================================ */

static uint8_t stack_a[4096] __attribute__((aligned(16)));
static uint8_t stack_c[4096] __attribute__((aligned(16)));
static uint8_t stack_shell[4096] __attribute__((aligned(16)));
static uint8_t stack_prod[2048] __attribute__((aligned(16)));
static uint8_t stack_cons[2048] __attribute__((aligned(16)));
static uint8_t stack_user[2048] __attribute__((aligned(16)));
static uint8_t stack_reaper[2048] __attribute__((aligned(16)));

/*
 * Reaper: demonstrates blocking waitpid. It parks itself via
 * sched_yield() inside proc_wait() until each child turns zombie,
 * then reaps it (address space + pages released) and reports the
 * exit code the child passed to SYS_EXIT.
 */
static void task_reaper(void)
{
    int code = 0;

    uart_puts("  [reaper] waiting on pid 1 (ping)");
    if (proc_wait(1, &code) == 1) {
        uart_puts_nolf("  [reaper] reaped pid 1, code=");
        uart_dec((unsigned)code);
        uart_puts("");
    }
    uart_puts("  [reaper] waiting on pid 2 (pong)");
    if (proc_wait(2, &code) == 2) {
        uart_puts_nolf("  [reaper] reaped pid 2, code=");
        uart_dec((unsigned)code);
        uart_puts("");
        uart_puts("  [reaper] all children reaped - process model OK");
    }
}

/* USER-MODE TASK: runs at EL0. It cannot touch UART or kernel
 * functions - every kernel interaction goes through SVC syscalls. */
static void user_hello(void)
{
    for (int i = 1; i <= 5; i++) {
        sys_print("  [user] hello from EL0! (syscall print)\n");
        (void)sys_ticks();
        sys_yield();
    }
    sys_print("  [user] work done, exiting via syscall\n");
    sys_exit(0);
}

/* ---- Stage 8 SMP demo: AP worker bodies ----
 * These run on secondary cores OUTSIDE the scheduler - true
 * parallel lanes with zero yields. */
static volatile uint64_t ap_burn_result[2];
static volatile int ap_burn_done[2];

#define MAKE_BURN(n)                                                       \
    static void ap_burn##n(uint64_t id)                                    \
    {                                                                      \
        uint64_t i, acc = 0;                                               \
        for (i = 0; i < 150000UL; i++)                                     \
            acc += i * id;                                                 \
        ap_burn_result[n] = acc;                                           \
        __asm__ volatile ("dsb sy" ::: "memory");                          \
        ap_burn_done[n] = 1;                                               \
    }
MAKE_BURN(0)
MAKE_BURN(1)
#undef MAKE_BURN

/* Busy-wait ~ms without yielding (short spans only). */
static void spin_ms(uint64_t ms)
{
    uint64_t start = timer_get_ms();
    while ((timer_get_ms() - start) < ms)
        ;
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
    unsigned n = 0;
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

/* ============================================================
 * System Tasks - useful work during boot
 * ============================================================ */

/* Task: Write system info to RAMFS */
static uint8_t stack_sysinfo[1024] __attribute__((aligned(16)));
static void task_sysinfo(void)
{
    uart_puts("  [sysinfo] Writing system info...");
    
    /* Write system info */
    const char *info = "AEOS v1.0 - Ajeeb Embodied AI OS";
    ramfs_write("sys/info", info, 33, 0);
    
    /* Write uptime (simulated) */
    uint32_t uptime = timer_get_ticks() / 100;
    ramfs_write("sys/uptime", &uptime, 4, 0);
    
    /* Write AI weights info */
    const char *ai_info = "AI: 3-8-3 MLP, Q16.16";
    ramfs_write("ai/weights", ai_info, 22, 0);
    
    /* Write CMT state */
    uint8_t cmt_level = (uint8_t)cmt_consciousness_level();
    ramfs_write("cmt/state", &cmt_level, 1, 0);
    
    uart_puts("  [sysinfo] Done: sys/info, sys/uptime, ai/weights, cmt/state");
    sched_exit();
}

/* Task: AI Decision Logger */
static uint8_t stack_ailog[1024] __attribute__((aligned(16)));
static void task_ailog(void)
{
    uart_puts("  [ailog] AI decision logger started");
    
    int last_action = -1;
    int count = 0;
    
    while (count < 10) {
        int action = ai_last_action();
        uint32_t inferences = ai_inference_count();
        
        /* Log if action changed */
        if (action != last_action) {
            uart_puts_nolf("  [ailog] Action changed to: ");
            uart_puts(ai_action_name(action));
            uart_puts_nolf("  [ailog] Inferences: ");
            uart_dec(inferences);
            uart_puts("");
            last_action = action;
        }
        
        /* Update CMT state in RAMFS */
        uint8_t cmt_level = (uint8_t)cmt_consciousness_level();
        ramfs_write("cmt/state", &cmt_level, 1, 0);
        
        count++;
        spin_ms(500);
        sched_yield();
    }
    
    uart_puts("  [ailog] Logger finished");
    sched_exit();
}

/* Task: Device Monitor */
static uint8_t stack_devmon[1024] __attribute__((aligned(16)));
static void task_devmon(void)
{
    uart_puts("  [devmon] Device monitor started");
    
    int count = 0;
    while (count < 5) {
        /* Check RAMFS */
        uint32_t file_count = ramfs_exists("sys/info") ? 1 : 0;
        file_count += ramfs_exists("sys/uptime") ? 1 : 0;
        file_count += ramfs_exists("ai/weights") ? 1 : 0;
        file_count += ramfs_exists("cmt/state") ? 1 : 0;
        file_count += ramfs_exists("user/data") ? 1 : 0;
        
        uart_puts_nolf("  [devmon] Files: ");
        uart_dec(file_count);
        uart_puts("/5");

        /* Check IPC */
        int q_count = ipc_queue_count();
        uart_puts_nolf("  [devmon] IPC queues: ");
        uart_dec(q_count);
        uart_puts("");
        
        count++;
        spin_ms(800);
        sched_yield();
    }
    
    uart_puts("  [devmon] Monitor finished");
    sched_exit();
}

/* Producer/consumer: mutex-protected shared data + semaphore
 * signaling - the classic sync primitives demo. */
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

void kernel_main(uint64_t bi_raw)
{
    const bootinfo_t *bi = (const bootinfo_t *)(uintptr_t)bi_raw;
    extern uint64_t exception_vector;

    /* Install the vector table immediately: any early fault (incl.
     * the debug blocks below) must land in real handlers, not at
     * a stale VBAR. */
    __asm__ volatile ("msr vbar_el1, %0"
                      :: "r"((uint64_t)&exception_vector) : "memory");

    /* Adopt the loader's DTB-discovered UART before any output:
     * this is what frees the kernel from a fixed board address. */
    if (bi->flags & BI_FLAG_UART_VALID)
        uart_set_base((uintptr_t)bi->uart_base);

    uart_init();
    uart_puts("Hello from AEOS");
    uart_puts("ARM64 Kernel Started Successfully!");

    /* Boot protocol: info handed over by AEOS-LDR (stage 2) */
    if (bi && bi->magic == BOOTINFO_MAGIC) {
        uart_puts_nolf("  [boot] loader v");
        uart_dec(bi->version >> 16);
        uart_puts_nolf(".");
        uart_dec(bi->version & 0xFFFF);
        uart_puts(" handoff OK");
        uart_puts_nolf("  [boot] kernel image: ");
        uart_dec(bi->kernel_size);
        uart_puts(" bytes");
        uart_puts_nolf("  [boot] RAM: ");
        uart_dec((unsigned)(bi->ram_size / (1024 * 1024)));
        uart_puts(" MB");
        if (bi->flags & BI_FLAG_DTB_VALID) {
            uart_puts_nolf("  [boot] DTB @ 0x");
            uart_hex(bi->dtb_ptr);
            uart_puts("");
        }
    } else {
        uart_puts("  [boot] no bootinfo - legacy boot");
    }

    /* Boot sanity: confirm we reached EL1 (boot.S drops from EL2) */
    {
        uint64_t cel;
        __asm__ volatile ("mrs %0, CurrentEL" : "=r"(cel));
        if ((cel >> 2) != 0x1) {
            uart_puts("FATAL: kernel not running at EL1, halting.");
            for (;;)
                __asm__ volatile ("wfe");
        }
    }

    uart_puts("");
    uart_puts("=== Stage 2: Memory Management ===");

    /* [1] PMM */
    uart_puts("");
    uart_puts("[1/4] Initializing PMM...");
    pmm_init();
    uart_puts_nolf("  Total pages: "); uart_dec(pmm_get_total_pages()); uart_puts("");
    uart_puts_nolf("  Free pages:  "); uart_dec(pmm_get_free_pages()); uart_puts("");
    uart_puts_nolf("  Free memory: "); uart_dec(pmm_get_free_pages() * 4); uart_puts(" KB");

    /* [2] VMM */
    uart_puts("");
    uart_puts("[2/4] Initializing VMM...");
    uintptr_t tp = pmm_alloc_page();
    uart_puts_nolf("  Test page before VMM: "); uart_hex(tp); uart_puts("");
    pmm_free_page(tp);
    vmm_init();
    uart_puts("  Page tables created (identity mapping)");

    /* [3] MMU */
    uart_puts("");
    uart_puts("[3/4] Enabling MMU...");
    vmm_enable_mmu();
    uart_puts("  MMU enabled!");

    /* [4] Heap */
    uart_puts("");
    uart_puts("[4/4] Initializing heap...");
    heap_init();
    uart_puts("  Heap: 16MB at 0x41000000");

    /* Tests */
    uart_puts("");
    uart_puts("--- Memory Tests ---");

    uart_puts("");
    uart_puts("[Test 1] Physical page allocation");
    uintptr_t p1 = pmm_alloc_page();
    uintptr_t p2 = pmm_alloc_page();
    uintptr_t p3 = pmm_alloc_page();
    uart_puts_nolf("  Page 1: "); uart_hex(p1); uart_puts("");
    uart_puts_nolf("  Page 2: "); uart_hex(p2); uart_puts("");
    uart_puts_nolf("  Page 3: "); uart_hex(p3); uart_puts("");

    uart_puts("");
    uart_puts("[Test 2] VA -> PA translation");
    uint64_t pa = vmm_get_phys(0x40000000);
    uart_puts_nolf("  VA 0x40000000 -> PA "); uart_hex(pa); uart_puts("");

    uart_puts("");
    uart_puts("[Test 3] Kernel heap allocation");
    void *h1 = kmalloc(128);
    void *h2 = kmalloc(256);
    void *h3 = kmalloc(1024);
    uart_puts_nolf("  Alloc 128B: "); uart_hex((uint64_t)h1); uart_puts("");
    uart_puts_nolf("  Alloc 256B: "); uart_hex((uint64_t)h2); uart_puts("");
    uart_puts_nolf("  Alloc 1KB:  "); uart_hex((uint64_t)h3); uart_puts("");

    uart_puts("");
    uart_puts("[Test 4] memset/memcpy/memcmp");
    memset(h1, 0xAA, 128);
    memcpy(h2, h1, 128);
    uart_puts_nolf("  Memcmp (should be 0): ");
    uart_dec(memcmp(h1, h2, 128)); uart_puts("");

    uart_puts("");
    uart_puts("[Test 5] Free physical pages");
    pmm_free_page(p1); pmm_free_page(p2); pmm_free_page(p3);
    uart_puts_nolf("  Free pages after release: ");
    uart_dec(pmm_get_free_pages()); uart_puts("");

    uart_puts("");
    uart_puts("=== Memory Management Working! ===");
    uart_puts("");
    uart_puts_nolf("  Physical pages: "); uart_dec(pmm_get_total_pages()); uart_puts(" total");
    uart_puts_nolf("  Physical free:  "); uart_dec(pmm_get_free_pages()); uart_puts(" pages");
    uart_puts_nolf("  Heap used:      "); uart_dec(heap_get_used()); uart_puts(" bytes");
    uart_puts_nolf("  Heap free:      "); uart_dec(heap_get_free()); uart_puts(" bytes");

    /* ============================================================
     * Stage 3: Interrupt Handling + AI Engine
     * ============================================================ */

    uart_puts("");
    uart_puts("=== Stage 3: Interrupts + AI Engine ===");

    /* Install the vector table before any exception source is live */
    {
        extern uint64_t exception_vector;
        __asm__ volatile ("msr vbar_el1, %0" :: "r"(&exception_vector) : "memory");
    }

    uart_puts("");
    uart_puts("[1/5] Initializing AI engine...");
    ai_init();
    uart_puts("  Fixed-point MLP ready (3-8-3, Q16.16)");
    uart_puts("  Subscribed to timer ticks (perceive-decide-act)");

    uart_puts("");
    uart_puts("[2/5] Initializing CMT engine...");
    cmt_init();
    uart_puts("  Consciousness Manifold Theory (6-12-3, Q16.16)");
    uart_puts("  C = E_f : 6 inputs -> IM -> T -> E_f -> C");
    uart_puts("  Subscribed to timer ticks (perceive-transform-experience-consciously)");

    uart_puts("");
    uart_puts("[3/5] Initializing device framework...");
    device_init();
    uart_puts("  16 device slots available");

    uart_puts("");
    uart_puts("[4/6] Initializing RAMFS...");
    ramfs_init();
    /* Create system files */
    ramfs_create("sys/info");
    ramfs_create("sys/uptime");
    ramfs_create("ai/weights");
    ramfs_create("cmt/state");
    ramfs_create("user/data");
    uart_puts("  32 files, 4KB each");
    uart_puts("  Created: sys/info, sys/uptime, ai/weights, cmt/state, user/data");

    uart_puts("");
    uart_puts("[5/6] Initializing IPC...");
    ipc_init();
    /* Create IPC queues for tasks */
    ipc_create(0);    /* main -> workers */
    ipc_create(1);    /* AI -> logger */
    ipc_create(2);   /* debugger -> main */
    uart_puts("  8 message queues, 16 msgs each");
    uart_puts("  Created queues: main, ai, debug");

    uart_puts("");
    uart_puts("[6/6] Initializing IRQ subsystem...");
    irq_init();
    uart_puts("  GICv2 initialized");
    uart_puts("  Exception vector installed (VBAR_EL1)");
    uart_puts("  Timer enabled at 100Hz - AI + CMT engines live");

    uart_puts("");
    uart_puts("[3/4] Registering demo callback...");
    timer_set_callback(test_timer_callback);
    uart_puts("  Callback registered");

    uart_puts("");
    uart_puts("[4/4] CMT consciousness demo (5s, live)...");
    cmt_set_verbose(1);

    /* ============================================================
     * Stage 4: AI Demo Window
     * ============================================================ */

    uart_puts("");
    uart_puts("=== Stage 4: AI + CMT perceive-decide-act (1.5s, live) ===");
    ai_set_verbose(1);
    cmt_set_verbose(1);

    {
        uint64_t start = timer_get_ms();
        while ((timer_get_ms() - start) < 1500)
            ;
    }

    ai_set_verbose(0);
    cmt_set_verbose(0);

    /* ============================================================
     * Stage 5: Scheduler - multitasking demo
     * ============================================================ */

    uart_puts("");
    uart_puts("=== Stage 5: System Tasks + Scheduler + Shell ===");

    sched_init("main");
    sched_watchdog_set(600);

    mutex_init(&data_lock);
    sem_init(&items, 0);

    /* System tasks - useful work */
    uart_puts_nolf("  Spawn sysinfo: "); uart_dec(sched_spawn("sysinfo",
        task_sysinfo, stack_sysinfo, sizeof(stack_sysinfo))); uart_puts("");
    uart_puts_nolf("  Spawn ailog: "); uart_dec(sched_spawn("ailog",
        task_ailog, stack_ailog, sizeof(stack_ailog))); uart_puts("");
    uart_puts_nolf("  Spawn devmon: "); uart_dec(sched_spawn("devmon",
        task_devmon, stack_devmon, sizeof(stack_devmon))); uart_puts("");
    
    /* Demo tasks */
    uart_puts_nolf("  Spawn worker-a: "); uart_dec(sched_spawn("worker-a",
        task_a, stack_a, sizeof(stack_a))); uart_puts("");
    uart_puts_nolf("  Spawn ai-monitor: "); uart_dec(sched_spawn("ai-mon",
        task_c, stack_c, sizeof(stack_c))); uart_puts("");
    uart_puts_nolf("  Spawn shell: "); uart_dec(shell_start(stack_shell,
        sizeof(stack_shell))); uart_puts("");
    uart_puts_nolf("  Spawn producer: "); uart_dec(sched_spawn("producer",
        task_producer, stack_prod, sizeof(stack_prod))); uart_puts("");
    uart_puts_nolf("  Spawn consumer: "); uart_dec(sched_spawn("consumer",
        task_consumer, stack_cons, sizeof(stack_cons))); uart_puts("");
    uart_puts_nolf("  Spawn user task (EL0): "); uart_dec(sched_spawn_user(
        "user-hello", user_hello, stack_user, sizeof(stack_user)));
    uart_puts("");

    /* ============================================================
     * Stage 6: Processes - EL0 address spaces + exit/wait
     * ============================================================ */
    uart_puts("");
    uart_puts("=== Stage 6: Processes (EL0 address spaces) ===");
    {
        int pid1 = proc_spawn_image("ping", user_prog_ping,
                                    user_prog_ping_len, 0);
        int pid2 = proc_spawn_image("pong", user_prog_pong,
                                    user_prog_pong_len, 0);
        (void)pid1; (void)pid2;
    }
    uart_puts_nolf("  Spawn reaper: "); uart_dec(sched_spawn("reaper",
        task_reaper, stack_reaper, sizeof(stack_reaper))); uart_puts("");

    /* ============================================================
     * Stage 7: Persistent storage - virtio-blk + AEOS-FS
     * ============================================================ */
    uart_puts("");
    uart_puts("=== Stage 7: Storage (virtio-blk + AEOS-FS) ===");
    if (vblk_init()) {
        if (!afs_mount())
            afs_format();

        /* Persistent boot counter: survives reboots because it
         * lives on the disk the BIOS boots from. */
        {
            char buf[16];
            unsigned count = 0;
            int n = afs_read("bootcount.txt", buf, sizeof(buf) - 1);
            int i;

            if (n > 0) {
                buf[n] = '\0';
                for (i = 0; buf[i] >= '0' && buf[i] <= '9'; i++)
                    count = count * 10 + (unsigned)(buf[i] - '0');
            }
            count++;
            uart_puts_nolf("[self] boot #");
            uart_dec(count);
            uart_puts("");

            {
                char out[12];
                int len = 0;

                if (count == 0)
                    out[len++] = '0';
                {
                    unsigned v = count;
                    char rev[12];
                    int r = 0;

                    while (v) {
                        rev[r++] = (char)('0' + (v % 10));
                        v /= 10;
                    }
                    while (r > 0)
                        out[len++] = rev[--r];
                }
                out[len] = '\0';
                afs_write("bootcount.txt", out, (size_t)len);
            }
        }

        {
            static const char msg[] =
                "AEOS-FS: hello from persistent storage!\n";
            char rbuf[64];

            if (afs_write("hello.txt", msg, sizeof(msg)) > 0) {
                int n = afs_read("hello.txt", rbuf, sizeof(rbuf) - 1);

                if (n > 0) {
                    rbuf[n] = '\0';
                    uart_puts_nolf("  [fs] read back: ");
                    uart_puts_nolf(rbuf);
                    uart_puts("");
                    uart_puts("  [fs] write+read verify OK");
                }
            }
        }
    } else {
        uart_puts("  [fs] no block device - storage disabled");
    }

    /* ============================================================
     * Stage 8: SMP - secondary cores as parallel workers
     * ============================================================ */
    uart_puts("");
    uart_puts("=== Stage 8: SMP (secondary cores) ===");
    smp_init();
    if (smp_online_count() > 0) {
        smp_submit(ap_burn0, 1000);
        smp_submit(ap_burn1, 2000);

        {
            uint32_t wait = 0;

            while ((!ap_burn_done[0] || !ap_burn_done[1]) &&
                   wait < 300) {
                spin_ms(10);
                wait++;
                sched_yield();
            }
            uart_puts_nolf("[smp] parallel jobs done: ");
            uart_dec((ap_burn_done[0] ? 1 : 0) +
                     (ap_burn_done[1] ? 1 : 0));
            uart_puts("/2");
            uart_puts_nolf("[smp] results: ");
            uart_dec((unsigned)(ap_burn_result[0] & 0xFFFF));
            uart_puts_nolf(" / ");
            uart_dec((unsigned)(ap_burn_result[1] & 0xFFFF));
            uart_puts("");
        }
    } else {
        uart_puts("  [smp] single-core boot - skipping demo");
    }

    /* ============================================================
     * Stage 9: Networking - ARP + ICMP echo over virtio-net
     * ============================================================ */
    uart_puts("");
    uart_puts("=== Stage 9: Network (virtio-net) ===");
    {
        extern void net_init(void);
        extern int net_ping(uint32_t ip_be, int count);

        net_init();
        {
            uint32_t gw = (10u << 24) | (0u << 16) | (2u << 8) | 2u;
            int ok = net_ping(gw, 3);

            uart_puts_nolf("[net] ping 10.0.2.2: ");
            uart_dec((unsigned)ok);
            uart_puts("/3 replies");
        }
    }

    /* Main becomes a peer task: a few rounds, reports, then exits
     * leaving the shell and workers alive. */
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
        cmt_report();

        uart_puts("");
        uart_puts("Main exiting; shell + workers keep running...");
        uart_puts("(type 'help' at the aeos> prompt)");
        sched_exit();
    }
}
