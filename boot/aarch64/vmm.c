/*
 * AEOS - Virtual Memory Manager (VMM)
 * Architecture: ARM64 (AArch64)
 *
 * 2MB block mapping at PMD level. Identity VA == PA.
 *
 * Page table: PGD -> PUD -> PMD (2MB blocks, no PTE level)
 * VA 0x40000000: PGD[0x80] -> PUD[0] -> PMD[0] -> 2MB block
 */

#include "memory.h"

static page_table_t kernel_pgd __attribute__((aligned(4096)));
static page_table_t kernel_pud __attribute__((aligned(4096)));
static page_table_t kernel_pmd __attribute__((aligned(4096)));

#define MAIR_VALUE ((0x00UL << 0) | (0xFFUL << 8) | (0x44UL << 16))

/* ============================================================
 * Per-process address spaces
 * ============================================================ */

/*
 * A user address space owns:
 *   - its own PGD (top-level entries value-copied from the kernel
 *     PGD, so the identity-mapped kernel image is visible without
 *     duplicating the lower tables), and
 *   - private PUD/PMD/PTE pages for the user VA window.
 *
 * All table memory comes from the PMM and is tracked in `tables[]`
 * so as_destroy() can give it back. USER_MAX_PAGES caps the tracked
 * user data pages; processes are small in Phase 1.
 */
#define AS_MAX_TABLES 16

struct addr_space {
    page_table_t *pgd;
    uintptr_t     tables[AS_MAX_TABLES];   /* PA of owned table pages */
    int           table_count;
    uintptr_t     user_region;             /* PA of the 2MB user block */
    addr_space_t *self;                    /* self-check for bad frees */
};

/* Active TTBR0 payload: NULL encodes the kernel address space. */
static addr_space_t *as_active;

static page_table_t *as_alloc_table(addr_space_t *as)
{
    uintptr_t pa = pmm_alloc_page();
    page_table_t *t;

    if (!pa || as->table_count >= AS_MAX_TABLES)
        return (page_table_t *)0;
    t = (page_table_t *)pa;
    memset(t, 0, sizeof(page_table_t));
    as->tables[as->table_count++] = pa;
    return t;
}

addr_space_t *as_create(void)
{
    addr_space_t *meta;
    page_table_t *pgd;
    int i;

    /* Meta block itself lives on the kernel heap. */
    meta = kmalloc(sizeof(addr_space_t));
    if (!meta)
        return (addr_space_t *)0;
    memset(meta, 0, sizeof(*meta));
    meta->self = meta;

    pgd = as_alloc_table(meta);
    if (!pgd) {
        kfree(meta);
        return (addr_space_t *)0;
    }
    meta->pgd = pgd;

    /* Share the kernel mappings by copying top-level descriptors:
     * they point at the same kernel PUD/PMD tables. */
    for (i = 0; i < 512; i++)
        pgd->entries[i] = kernel_pgd.entries[i];

    return meta;
}

void as_destroy(addr_space_t *as)
{
    int i;

    if (!as || as->self != as)
        return;

    if (as->user_region)
        pmm_free_pages(as->user_region, 512);   /* the 2MB user block */
    for (i = 0; i < as->table_count; i++)
        pmm_free_page(as->tables[i]);

    if (as_active == as) {
        as_switch((addr_space_t *)0);
    }
    as->self = (addr_space_t *)0;
    kfree(as);
}

addr_space_t *as_current(void)
{
    return as_active;
}

void as_switch(addr_space_t *as)
{
    uint64_t ttbr;

    if (as == as_active)
        return;

    ttbr = as ? (uint64_t)(uintptr_t)as->pgd : (uint64_t)&kernel_pgd;
    __asm__ volatile ("msr ttbr0_el1, %0" :: "r"(ttbr) : "memory");
    __asm__ volatile ("dsb sy" ::: "memory");
    /* No ASIDs in Phase 1: flush everything on every switch. */
    __asm__ volatile ("tlbi vmalle1");
    __asm__ volatile ("dsb sy" ::: "memory");
    __asm__ volatile ("isb" ::: "memory");

    as_active = as;
}

/*
 * Map one 2MB block at `va` (2MB-aligned) backed by physical `pa`.
 * The block descriptor is the shape proven by the kernel identity
 * map; see vmm.h for why Phase 1 avoids L3 pages.
 */
int as_map_block(addr_space_t *as, uint64_t va, uintptr_t pa,
                 uint64_t attrs)
{
    uint64_t pgd_i = (va >> 39) & 0x1FF;
    uint64_t pud_i = (va >> 30) & 0x1FF;
    uint64_t pmd_i = (va >> 21) & 0x1FF;
    page_table_t *pud, *pmd;

    if (!as || as->self != as || (va & 0x1FFFFFUL) ||
        ((uintptr_t)pa & 0x1FFFFFUL))
        return -1;

    if (as->table_count >= AS_MAX_TABLES - 2)
        return -1;

    pud = (page_table_t *)(uintptr_t)(as->pgd->entries[pgd_i] & ~0xFFFUL);
    if (!(as->pgd->entries[pgd_i] & PTE_TABLE)) {
        pud = as_alloc_table(as);
        if (!pud)
            return -1;
        as->pgd->entries[pgd_i] =
            ((uint64_t)(uintptr_t)pud & ~0xFFFUL) | PTE_TABLE;
    }

    pmd = (page_table_t *)(uintptr_t)(pud->entries[pud_i] & ~0xFFFUL);
    if (!(pud->entries[pud_i] & PTE_TABLE)) {
        pmd = as_alloc_table(as);
        if (!pmd)
            return -1;
        pud->entries[pud_i] =
            ((uint64_t)(uintptr_t)pmd & ~0xFFFUL) | PTE_TABLE;
    }

    pmd->entries[pmd_i] = ((uint64_t)pa & ~0x1FFFFFUL) | attrs | PTE_VALID;

    vmm_flush_tlb_all();
    return 0;
}

static void map_block(uint64_t va, uint64_t pa, uint64_t attrs)
{
    kernel_pgd.entries[(va >> 39) & 0x1FF] = (uint64_t)&kernel_pud | PTE_TABLE;
    kernel_pud.entries[(va >> 30) & 0x1FF] = (uint64_t)&kernel_pmd | PTE_TABLE;
    kernel_pmd.entries[(va >> 21) & 0x1FF] = (pa & ~0x1FFFFFUL) | attrs;
}

void vmm_init(void)
{
    memset(&kernel_pgd, 0, sizeof(page_table_t));
    memset(&kernel_pud, 0, sizeof(page_table_t));
    memset(&kernel_pmd, 0, sizeof(page_table_t));

    /* Normal RAM: AF + AttrIndx=1 + RW (EL1+EL0) + Inner Shareable.
     * Phase 0: user tasks execute from the same flat image, so RAM
     * is mapped EL0-accessible. Real per-task isolation arrives
     * with per-task page tables. */
    uint64_t ram = PTE_AF | (MT_NORMAL << 2) | AP_RW_ALL | SH_INNER | PTE_BLOCK;
    /* Device: AF + AttrIndx=0 + RW_EL1 + Non-shareable + Block */
    uint64_t dev = PTE_AF | (MT_DEVICE << 2) | AP_RW_EL1 | SH_NONE  | PTE_BLOCK;

    /* Device I/O */
    map_block(0x08000000, 0x08000000, dev);
    map_block(0x09000000, 0x09000000, dev);
    /* virtio-mmio slots 0..31 (0x0a000000 - 0x0a003fff) */
    map_block(0x0A000000, 0x0A000000, dev);

    /* RAM: 0x40000000 - 0x48000000 (128MB = 64 x 2MB blocks) */
    for (uint64_t a = 0x40000000; a < 0x48000000; a += 0x200000)
        map_block(a, a, ram);
}

static void mmu_apply(uint64_t pgd_pa)
{
    uint64_t v;

    v = MAIR_VALUE;
    __asm__ volatile ("msr mair_el1, %0" :: "r"(v) : "memory");

    /* TCR: T0SZ=16, IRGN0=01, ORGN0=01, SH0=11, TG0=00, IPS=010 */
    v = (16UL << 0) | (0UL << 7) | (1UL << 8) | (1UL << 10) |
        (3UL << 12) | (0UL << 14) | (2UL << 32);
    __asm__ volatile ("msr tcr_el1, %0" :: "r"(v) : "memory");

    __asm__ volatile ("msr ttbr0_el1, %0" :: "r"(pgd_pa) : "memory");

    __asm__ volatile ("dsb sy" ::: "memory");
    __asm__ volatile ("tlbi vmalle1");
    __asm__ volatile ("dsb sy" ::: "memory");
    __asm__ volatile ("isb" ::: "memory");

    __asm__ volatile ("mrs %0, sctlr_el1" : "=r"(v));
    v |= (1UL << 0) | (1UL << 2) | (1UL << 12);
    __asm__ volatile ("msr sctlr_el1, %0" :: "r"(v) : "memory");

    __asm__ volatile ("isb" ::: "memory");
}

void vmm_enable_mmu(void)
{
    as_active = (addr_space_t *)0;      /* kernel AS is live */
    mmu_apply((uint64_t)&kernel_pgd);
}

/*
 * Bring a SECONDARY core onto the same flat identity map. Page
 * tables are shared read-only state - only the per-core system
 * registers need configuring here.
 */
void vmm_ap_enable_mmu(void)
{
    mmu_apply((uint64_t)&kernel_pgd);
}

uint64_t vmm_get_phys(uint64_t virt)
{
    size_t pgd_i = (virt >> 39) & 0x1FF;
    size_t pud_i = (virt >> 30) & 0x1FF;
    size_t pmd_i = (virt >> 21) & 0x1FF;

    if (!(kernel_pgd.entries[pgd_i] & PTE_VALID)) return 0;
    page_table_t *pud = (page_table_t *)(kernel_pgd.entries[pgd_i] & ~0xFFFUL);

    if (!(pud->entries[pud_i] & PTE_VALID)) return 0;
    page_table_t *pmd = (page_table_t *)(pud->entries[pud_i] & ~0xFFFUL);

    uint64_t e = pmd->entries[pmd_i];
    if (!(e & PTE_VALID)) return 0;

    /* Block: bits[1:0] = 0b01 */
    if ((e & 0x3) == 0x1)
        return (e & ~0x1FFFFFUL) | (virt & 0x1FFFFF);

    /* Table: walk to PTE */
    page_table_t *pte = (page_table_t *)(e & ~0xFFFUL);
    uint64_t pe = pte->entries[(virt >> 12) & 0x1FF];
    if (!(pe & PTE_VALID)) return 0;
    return (pe & ~0xFFFUL) | (virt & 0xFFF);
}

void vmm_flush_tlb(uint64_t va)
{
    __asm__ volatile ("tlbi vae1, %0" :: "r"(va) : "memory");
    __asm__ volatile ("dsb sy" ::: "memory");
    __asm__ volatile ("isb" ::: "memory");
}

void vmm_flush_tlb_all(void)
{
    __asm__ volatile ("tlbi vmalle1");
    __asm__ volatile ("dsb sy" ::: "memory");
    __asm__ volatile ("isb" ::: "memory");
}
