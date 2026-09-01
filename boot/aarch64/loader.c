/*
 * AEOS-LDR - Stage 2 Boot Loader
 *
 * Runs after boot.S has CPU state ready (EL1, stack, BSS clear).
 * Responsibilities:
 *   1. Early console (UART is already up from stage 1's driver)
 *   2. Boot banner
 *   3. Firmware handoff capture: x0 = DTB pointer, sniffed + parsed
 *      for the /memory node so ram_size reflects real hardware
 *   4. Kernel image sanity validation before the kernel runs
 *   5. Fill bootinfo_t and hand it to kernel_main()
 *
 * Hardware note: nothing here touches QEMU-specific state. On a
 * real board any firmware that honors the ARM Linux boot protocol
 * (x0 = DTB) works unchanged.
 */

#include "loader.h"
#include "bootinfo.h"
#include "memory.h"
#include "uart.h"

/* DTB magic, big-endian on the wire: 0xd00dfeed */
#define DTB_MAGIC_BE 0xD00DFEEDUL

/* Sanity floor for a real kernel image */
#define KERNEL_MIN_SIZE 4096u

/* Fallback RAM size when no DTB is present (QEMU virt default) */
#define DEFAULT_RAM_SIZE (128u * 1024u * 1024u)

/*
 * Acceptance window for a firmware DTB pointer. The real RAM top is
 * unknown before the DTB is parsed (QEMU places the blob near the top
 * of RAM, above our conservative 128MB RAM_END), so this is a wide
 * sanity bound against garbage x0, not an architectural limit.
 */
#define DTB_PTR_LIMIT (RAM_START + (16ull << 30))

static uint32_t be32(uint32_t v)
{
    return ((v & 0xFFUL) << 24) | ((v & 0xFF00UL) << 8) |
           ((v >> 8) & 0xFF00UL) | ((v >> 24) & 0xFFUL);
}

/* Aligned-safe big-endian load (DTB is 4/8-aligned but be explicit). */
static uint32_t rd32(const void *p)
{
    uint32_t v;

    __builtin_memcpy(&v, p, sizeof v);
    return be32(v);
}

/* strlen/strcmp are not in our freestanding runtime; local versions. */
static uint32_t lstrlen(const char *s, const char *end)
{
    const char *p = s;

    while (p < end && *p != '\0')
        p++;
    return (uint32_t)(p - s);
}

static int lstrcmp(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

static int lstrcmp_n(const char *a, const char *b, uint32_t n)
{
    while (n > 0 && *a != '\0' && *a == *b) {
        a++;
        b++;
        n--;
    }
    if (n == 0)
        return 0;
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

static int dtb_sniff(uint64_t ptr, uint32_t *size_out)
{
    const volatile uint32_t *hdr;

    if (ptr < RAM_START || ptr >= DTB_PTR_LIMIT || (ptr & 0x7UL))
        return 0;

    hdr = (const volatile uint32_t *)ptr;
    if (be32(hdr[0]) != DTB_MAGIC_BE)
        return 0;

    *size_out = be32(hdr[1]);
    if (*size_out == 0 || *size_out > (uint32_t)(DTB_PTR_LIMIT - ptr))
        return 0;

    return 1;
}

/* --- Minimal FDT structure-block walker ----------------------------- */

#define FDT_BEGIN_NODE 0x1u
#define FDT_END_NODE   0x2u
#define FDT_PROP       0x3u
#define FDT_NOP        0x4u
#define FDT_END        0x9u

typedef struct {
    uint32_t magic;             /* big-endian on the wire */
    uint32_t totalsize;
    uint32_t off_dt_struct;
    uint32_t off_dt_strings;
    uint32_t off_mem_rsvmap;
    uint32_t version;
    uint32_t last_comp_version;
    uint32_t boot_cpuid_phys;
    uint32_t size_dt_strings;
    uint32_t size_dt_struct;
} fdt_header_t;

static const char *fdt_string(const char *base, const char *end,
                              uint32_t off)
{
    const char *s = base + off;

    if (s >= end || lstrlen(s, end) == (uint32_t)(end - s))
        return NULL;
    return s;
}


/*
 * Find the first PL011-style UART in the DTB: any root-level node
 * whose name starts with "serial" or "uart", taking its reg[0].
 * This is how one kernel binary runs on QEMU virt, real boards,
 * and anything else that describes its hardware honestly.
 */
static uint64_t dtb_find_uart(uint64_t dtb, uint32_t dtb_size)
{
    const volatile fdt_header_t *h =
        (const volatile fdt_header_t *)(uintptr_t)dtb;
    uint32_t struct_off   = be32(h->off_dt_struct);
    uint32_t strings_off  = be32(h->off_dt_strings);
    uint32_t struct_size  = be32(h->size_dt_struct);
    const char *sp, *send, *sb, *sbend;
    int depth = 0;
    int candidate = 0;
    uint64_t found = 0;
    uint32_t addr_cells = 2u;

    if (struct_size == 0 ||
        struct_off > dtb_size || strings_off > dtb_size ||
        struct_size > dtb_size - struct_off)
        return 0;

    sp    = (const char *)(uintptr_t)(dtb + struct_off);
    send  = sp + struct_size;
    sb    = (const char *)(uintptr_t)(dtb + strings_off);
    sbend = sb + be32(h->size_dt_strings);

    while (sp + 4 <= send) {
        uint32_t tok = rd32(sp);

        sp += 4;
        if (tok == FDT_END) break;
        if (tok == FDT_NOP) continue;

        if (tok == FDT_BEGIN_NODE) {
            int parent_depth = depth;
            const char *name = sp;
            uint32_t nlen = lstrlen(name, send);

            depth++;
            sp += (nlen + 1u + 3u) & ~3u;
            if (sp > send) return 0;
            /* serial@ / uart@ / pl011@ - root children only */
            candidate =
                parent_depth == 1 &&
                ((nlen >= 7 && lstrcmp_n(name, "serial@", 7) == 0) ||
                 (nlen >= 5 && lstrcmp_n(name, "uart@", 5) == 0) ||
                 (nlen >= 6 && lstrcmp_n(name, "pl011@", 6) == 0) ||
                 (nlen >= 7 && lstrcmp_n(name, "ns16550", 7) == 0 &&
                  (nlen == 7 || name[7] == '@')));
        } else if (tok == FDT_END_NODE) {
            if (depth > 0) depth--;
            candidate = 0;
        } else if (tok == FDT_PROP) {
            uint32_t len, nameoff;
            const char *val, *pname;

            if (sp + 8 > send) return 0;
            len     = rd32(sp);
            nameoff = rd32(sp + 4);
            sp += 8;
            val = sp;
            sp += (len + 3u) & ~3u;
            if (val + len > send || sp > send) return 0;
            pname = fdt_string(sb, sbend, nameoff);
            if (!pname) continue;

            if (depth == 1 && !candidate && len == 4 &&
                lstrcmp(pname, "#address-cells") == 0) {
                addr_cells = rd32(val);
            } else if (candidate && !found &&
                       lstrcmp(pname, "reg") == 0) {
                uint64_t base = 0;
                uint32_t i;

                if (addr_cells < 1u || addr_cells > 2u)
                    return 0;
                for (i = 0; i < addr_cells; i++)
                    base = (base << 32) | rd32(val + 4u * i);
                found = base;
            }
        } else {
            return 0;
        }
    }
    return found;
}


/* Resolve a string-block offset; NULL if out of bounds/unterminated. */
/*
 * Find the standard /memory node ("memory" or "memory@<addr>", root
 * children only) and decode its reg = <base size> pair.
 * Cell counts of 1..2 are supported - this covers QEMU's generated
 * DTB and every mainstream firmware. Returns 1 + fills outputs, or 0.
 */
static int dtb_memory(uint64_t dtb, uint32_t dtb_size,
                      uint64_t *base_out, uint64_t *size_out)
{
    const volatile fdt_header_t *h =
        (const volatile fdt_header_t *)(uintptr_t)dtb;
    uint32_t struct_off   = be32(h->off_dt_struct);
    uint32_t strings_off  = be32(h->off_dt_strings);
    uint32_t struct_size  = be32(h->size_dt_struct);
    const char *sp, *send, *sb, *sbend;
    int depth = 0;
    int mem_node = 0;           /* current node is a /memory node */
    uint32_t addr_cells = 2u;   /* DT spec defaults */
    uint32_t size_cells = 1u;

    if (struct_size == 0 ||
        struct_off > dtb_size || strings_off > dtb_size ||
        struct_size > dtb_size - struct_off ||
        be32(h->size_dt_strings) > dtb_size - strings_off)
        return 0;

    sp   = (const char *)(uintptr_t)(dtb + struct_off);
    send = sp + struct_size;
    sb   = (const char *)(uintptr_t)(dtb + strings_off);
    sbend = sb + be32(h->size_dt_strings);

    while (sp + 4 <= send) {
        uint32_t tok = rd32(sp);

        sp += 4;
        if (tok == FDT_END)
            break;
        if (tok == FDT_NOP)
            continue;

        if (tok == FDT_BEGIN_NODE) {
            int parent_depth = depth;
            const char *name = sp;
            uint32_t nlen = lstrlen(name, send);

            depth++;
            /* name + NUL, padded to 4 bytes */
            sp += (nlen + 1u + 3u) & ~3u;
            if (sp > send)
                return 0;
            mem_node = parent_depth == 1 && nlen >= 6 &&
                       lstrcmp_n(name, "memory", 6) == 0 &&
                       (nlen == 6 || name[6] == '@');
        } else if (tok == FDT_END_NODE) {
            if (depth > 0)
                depth--;
            mem_node = 0;
        } else if (tok == FDT_PROP) {
            uint32_t len, nameoff;
            const char *val, *pname;

            if (sp + 8 > send)
                return 0;
            len     = rd32(sp);
            nameoff = rd32(sp + 4);
            sp += 8;
            val = sp;
            sp += (len + 3u) & ~3u;
            if (val + len > send || sp > send)
                return 0;
            pname = fdt_string(sb, sbend, nameoff);
            if (!pname)
                continue;

            if (mem_node && lstrcmp(pname, "reg") == 0) {
                uint64_t base = 0, size = 0;
                uint32_t i;

                if (addr_cells < 1u || addr_cells > 2u ||
                    size_cells < 1u || size_cells > 2u ||
                    len != (addr_cells + size_cells) * 4u)
                    return 0;
                for (i = 0; i < addr_cells; i++)
                    base = (base << 32) | rd32(val + 4u * i);
                for (i = 0; i < size_cells; i++)
                    size = (size << 32) | rd32(val + 4u * addr_cells +
                                               4u * i);
                *base_out = base;
                *size_out = size;
                return 1;
            } else if (!mem_node && depth == 1 && len == 4) {
                /* Root properties: real cell counts override defaults */
                if (lstrcmp(pname, "#address-cells") == 0)
                    addr_cells = rd32(val);
                else if (lstrcmp(pname, "#size-cells") == 0)
                    size_cells = rd32(val);
            }
        } else {
            return 0;   /* unknown token -> treat blob as corrupt */
        }
    }
    return 0;
}

extern uint8_t _kernel_start[];
extern uint8_t _kernel_end[];
extern uint8_t _kernel_image_end[];   /* raw binary content end */

static int validate_kernel(void)
{
    uint64_t size = (uint64_t)(_kernel_end - _kernel_start);

    if (_kernel_start < (uint8_t *)KERNEL_BASE)
        return 0;
    if (size < KERNEL_MIN_SIZE)
        return 0;
    if ((uint64_t)_kernel_end > RAM_END)
        return 0;

    return 1;
}

static void banner(void)
{
    uart_puts("");
    uart_puts("=========================================");
    uart_puts("  AEOS - Ajeeb Embodied AI OS  [LDR v1.1]");
    uart_puts("=========================================");
}

uint64_t loader_main(uint64_t dtb_ptr_raw)
{
    static bootinfo_t bi;   /* static: survives handoff, no frame games */
    uint32_t dtb_size = 0;
    int dtb_ok;
    uint64_t ram_size = DEFAULT_RAM_SIZE;
    int ram_measured = 0;
    uint32_t flags = 0;
    uint64_t bi_uart_base = UART_BASE;

    banner();

    uart_puts("[LDR] AEOS-LDR stage-2 v1.1");

    /* --- Kernel image validation --- */
    if (!validate_kernel()) {
        uart_puts("[LDR] FATAL: kernel image invalid");
        uart_puts_nolf("[LDR]   start=");
        uart_hex((unsigned long long)(uintptr_t)_kernel_start);
        uart_puts_nolf(" end=");
        uart_hex((unsigned long long)(uintptr_t)_kernel_end);
        uart_puts("");
        return 0;
    }
    uart_puts_nolf("[LDR] Kernel OK: ");
    uart_dec((unsigned)(_kernel_end - _kernel_start));
    uart_puts_nolf(" bytes @ ");
    uart_hex((unsigned long long)(uintptr_t)_kernel_start);
    uart_puts("");

    /* --- DTB handoff sniff --- */
    dtb_ok = dtb_sniff(dtb_ptr_raw, &dtb_size);
    if (dtb_ok) {
        uint64_t mem_base = 0, mem_size = 0;

        uart_puts_nolf("[LDR] DTB found @ ");
        uart_hex((unsigned long long)dtb_ptr_raw);
        uart_puts_nolf(" (");
        uart_dec(dtb_size);
        uart_puts(" bytes)");
        flags |= BI_FLAG_DTB_VALID;

        /* --- UART base from /serial or /uart --- */
        {
            uint64_t ubase = dtb_find_uart(dtb_ptr_raw, dtb_size);

            if (ubase) {
                bi_uart_base = ubase;
                flags |= BI_FLAG_UART_VALID;
                uart_puts_nolf("[LDR] UART: ");
                uart_hex(ubase);
                uart_puts(" (from DTB)");
            }
        }

        /* --- Real RAM size from /memory --- */
        if (dtb_memory(dtb_ptr_raw, dtb_size, &mem_base, &mem_size) &&
            mem_base == RAM_START && mem_size >= KERNEL_MIN_SIZE &&
            mem_size <= (4ull << 30)) {
            ram_size     = mem_size;
            ram_measured = 1;
            flags |= BI_FLAG_RAM_MEASURED;
        }
    } else {
        uart_puts("[LDR] No DTB - using built-in defaults");
    }

    uart_puts_nolf("[LDR] RAM: ");
    uart_dec((unsigned)(ram_size >> 20));
    uart_puts_nolf(" MB (");
    uart_puts(ram_measured ? "measured from DTB)" : "built-in default)");

    /* --- Fill boot info --- */
    bi.magic       = BOOTINFO_MAGIC;
    bi.version     = LOADER_VERSION;
    bi.ram_base    = RAM_START;
    bi.ram_size    = ram_size;
    bi.uart_base   = bi_uart_base;
    bi.dtb_ptr     = dtb_ok ? dtb_ptr_raw : 0;
    bi.dtb_size    = dtb_ok ? dtb_size : 0;
    /* Size of the LOADED BINARY (what a disk image carries), not
     * the bss span - the self-update path re-writes exactly this
     * many bytes back to kernel.bin. */
    bi.kernel_size = (uint32_t)(_kernel_image_end - _kernel_start);
    bi.flags       = flags;
    bi.reserved    = 0;

    uart_puts("[LDR] Handing off to kernel...");
    uart_puts("");

    /* Returned in x0; boot.S passes it straight into kernel_main. */
    return (uint64_t)(uintptr_t)&bi;
}
