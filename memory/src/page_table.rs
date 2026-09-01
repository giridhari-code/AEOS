//! Virtual memory paging.
//!
//! 4-level page table management for x86_64 architecture.
//! Supports mapping virtual addresses to physical frames with
//! configurable permissions.

use bitflags::bitflags;

/// A virtual address.
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct VirtAddr(pub usize);

impl VirtAddr {
    /// Create a virtual address from a usize.
    pub const fn new(addr: usize) -> Self {
        Self(addr)
    }

    /// Get the raw address value.
    pub const fn as_usize(&self) -> usize {
        self.0
    }

    /// Get PML4 index (bits 39-47).
    pub const fn pml4_index(&self) -> usize {
        (self.0 >> 39) & 0x1FF
    }

    /// Get PDPT index (bits 30-38).
    pub const fn pdpt_index(&self) -> usize {
        (self.0 >> 30) & 0x1FF
    }

    /// Get PD index (bits 21-29).
    pub const fn pd_index(&self) -> usize {
        (self.0 >> 21) & 0x1FF
    }

    /// Get PT index (bits 12-20).
    pub const fn pt_index(&self) -> usize {
        (self.0 >> 12) & 0x1FF
    }

    /// Get page offset (bits 0-11).
    pub const fn page_offset(&self) -> usize {
        self.0 & 0xFFF
    }
}

/// A physical address.
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct PhysAddr(pub usize);

impl PhysAddr {
    /// Create a physical address from a usize.
    pub const fn new(addr: usize) -> Self {
        Self(addr)
    }

    /// Get the raw address value.
    pub const fn as_usize(&self) -> usize {
        self.0
    }

    /// Get the page frame number.
    pub const fn frame_number(&self) -> usize {
        self.0 >> 12
    }
}

bitflags! {
    /// Page table entry flags.
    #[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
    pub struct PageTableFlags: u64 {
        /// Entry is present.
        const PRESENT = 1 << 0;
        /// Entry is writable.
        const WRITABLE = 1 << 1;
        /// Entry is user-accessible.
        const USER_ACCESSIBLE = 1 << 2;
        /// Page write-through.
        const WRITE_THROUGH = 1 << 3;
        /// Page is not cached.
        const NO_CACHE = 1 << 4;
        /// Entry was accessed.
        const ACCESSED = 1 << 5;
        /// Entry is dirty (written to).
        const DIRTY = 1 << 6;
        /// Huge page (2MB or 1GB).
        const HUGE_PAGE = 1 << 7;
        /// Global page (not flushed on TLB flush).
        const GLOBAL = 1 << 8;
        /// No execute (NX bit).
        const NO_EXECUTE = 1 << 63;
    }
}

impl PageTableFlags {
    /// Flags for a kernel code page (read-only, executable).
    pub const KERNEL_CODE: Self = Self::PRESENT;
    
    /// Flags for a kernel data page (read-write, non-executable).
    pub const KERNEL_DATA: Self = Self::PRESENT | Self::WRITABLE | Self::NO_EXECUTE;
    
    /// Flags for a user code page (read-only, executable, user-accessible).
    pub const USER_CODE: Self = Self::PRESENT | Self::USER_ACCESSIBLE;
    
    /// Flags for a user data page (read-write, non-executable, user-accessible).
    pub const USER_DATA: Self = Self::PRESENT | Self::WRITABLE | Self::USER_ACCESSIBLE | Self::NO_EXECUTE;
}

/// A page table entry.
#[derive(Debug, Clone, Copy)]
#[repr(transparent)]
pub struct PageTableEntry(u64);

impl PageTableEntry {
    /// Create a new entry.
    pub const fn new() -> Self {
        Self(0)
    }

    /// Set the physical address.
    pub fn set_address(&mut self, addr: PhysAddr) {
        self.0 = (self.0 & 0x000FFFFFFFFFF000) | (addr.0 as u64 & 0x000FFFFFFFFFF000);
    }

    /// Get the physical address.
    pub fn address(&self) -> PhysAddr {
        PhysAddr((self.0 & 0x000FFFFFFFFFF000) as usize)
    }

    /// Set flags.
    pub fn set_flags(&mut self, flags: PageTableFlags) {
        self.0 = (self.0 & 0x0000000000000FFF) | (flags.bits() & 0xFFFFFFFFFFFFF000);
    }

    /// Get flags.
    pub fn flags(&self) -> PageTableFlags {
        PageTableFlags::from_bits_truncate(self.0)
    }

    /// Check if entry is present.
    pub fn is_present(&self) -> bool {
        self.flags().contains(PageTableFlags::PRESENT)
    }
}

/// A page table (512 entries).
#[derive(Debug, Clone, Copy)]
#[repr(C, align(4096))]
pub struct PageTable {
    entries: [PageTableEntry; 512],
}

impl PageTable {
    /// Create a new empty page table.
    pub const fn new() -> Self {
        Self {
            entries: [PageTableEntry(0); 512],
        }
    }

    /// Get an entry by index.
    pub fn entry(&self, index: usize) -> &PageTableEntry {
        &self.entries[index]
    }

    /// Get a mutable entry by index.
    pub fn entry_mut(&mut self, index: usize) -> &mut PageTableEntry {
        &mut self.entries[index]
    }

    /// Map a virtual address to a physical address.
    pub fn map(
        &mut self,
        virt: VirtAddr,
        phys: PhysAddr,
        flags: PageTableFlags,
    ) -> Result<(), MappingError> {
        let pml4_idx = virt.pml4_index();
        let pdpt_idx = virt.pdpt_index();
        let pd_idx = virt.pd_index();
        let pt_idx = virt.pt_index();

        // Get or create PDPT
        let pdpt = self.get_or_create_table(pml4_idx, PageTableFlags::KERNEL_DATA)?;

        // Get or create PD
        let pd = pdpt.get_or_create_table(pdpt_idx, PageTableFlags::KERNEL_DATA)?;

        // Get or create PT
        let pt = pd.get_or_create_table(pd_idx, PageTableFlags::KERNEL_DATA)?;

        // Map the page
        let entry = pt.entry_mut(pt_idx);
        if entry.is_present() {
            return Err(MappingError::AlreadyMapped);
        }

        entry.set_address(phys);
        entry.set_flags(flags | PageTableFlags::PRESENT);

        Ok(())
    }

    /// Get or create a child table.
    fn get_or_create_table(
        &mut self,
        index: usize,
        flags: PageTableFlags,
    ) -> Result<&mut PageTable, MappingError> {
        let entry = self.entry_mut(index);
        
        if entry.is_present() {
            // Table exists, get reference
            let addr = entry.address().0;
            Ok(unsafe { &mut *(addr as *mut PageTable) })
        } else {
            // Create new table (would need frame allocation in real implementation)
            // For Phase 0, return error
            Err(MappingError::PageTableAllocationFailed)
        }
    }
}

/// Errors from page table operations.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum MappingError {
    /// Page is already mapped.
    AlreadyMapped,
    /// Page is not mapped.
    NotMapped,
    /// Page table allocation failed.
    PageTableAllocationFailed,
    /// Invalid address.
    InvalidAddress,
}
