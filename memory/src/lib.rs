//! # AEOS Memory Module
//!
//! Physical frame allocator and virtual memory paging for AEOS.
//!
//! ## Design
//! - **Frame allocator**: bitmap-based, supports alloc/free of 4KB pages
//! - **Paging**: 4-level page tables (PML4 → PDPT → PD → PT) for x86_64
//! - **No heap**: all allocations are static or frame-based
//!
//! ## Safety
//! This module uses `#![forbid(unsafe_code)]` except for the low-level
//! page table manipulation functions which are marked `unsafe` and
//! documented with preconditions.

#![forbid(unsafe_code)]
#![no_std]
#![warn(missing_docs)]

extern crate alloc;

mod frame_alloc;
mod page_table;
mod region;

pub use frame_alloc::{FrameAllocator, FrameAllocatorError, PhysFrame};
pub use page_table::{PageTable, PageTableFlags, VirtAddr, PhysAddr};
pub use region::{MemoryRegion, MemoryRegionType};

/// Size of a single page in bytes.
pub const PAGE_SIZE: usize = 4096;

/// Shift for page size (log2 of PAGE_SIZE).
pub const PAGE_SHIFT: usize = 12;

/// Align address up to page boundary.
pub const fn page_align_up(addr: usize) -> usize {
    (addr + PAGE_SIZE - 1) & !(PAGE_SIZE - 1)
}

/// Align address down to page boundary.
pub const fn page_align_down(addr: usize) -> usize {
    addr & !(PAGE_SIZE - 1)
}

/// Calculate number of frames needed for a given size.
pub const fn frames_needed(size: usize) -> usize {
    (size + PAGE_SIZE - 1) / PAGE_SIZE
}

/// Memory initialization error.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum InitError {
    /// Memory map is invalid or too small.
    InvalidMemoryMap,
    /// Not enough memory for frame allocator bitmap.
    InsufficientMemory,
    /// Page table allocation failed.
    PageTableAllocationFailed,
}

/// Initialize the memory subsystem with the given memory map.
///
/// # Safety
/// - `phys_memory_start` must be the physical base address of RAM.
/// - `phys_memory_size` must be the size of physical RAM in bytes.
/// - This must be called exactly once, before any other memory functions.
pub fn init(phys_memory_start: usize, phys_memory_size: usize) -> Result<(), InitError> {
    frame_init(phys_memory_start, phys_memory_size)
}

/// Initialize the frame allocator.
fn frame_init(start: usize, size: usize) -> Result<(), InitError> {
    frame_alloc::init(start, size)
}

/// Global frame allocator instance.
static mut FRAME_ALLOC: Option<FrameAllocator> = None;

/// Get a reference to the global frame allocator.
///
/// # Safety
/// Must be called after `init()` and from a single thread.
pub fn frame_allocator() -> &'static mut FrameAllocator {
    unsafe { FRAME_ALLOC.get_or_insert_with(|| FrameAllocator::new()) }
}
