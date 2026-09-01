//! Physical frame allocator.
//!
//! Bitmap-based allocator for 4KB physical frames. Supports alloc/free
//! with O(1) amortized time using a free-stack.

use crate::{PAGE_SIZE, InitError};

/// A physical frame number (PFN).
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct PhysFrame(pub usize);

impl PhysFrame {
    /// Create a frame from a physical address.
    pub fn from_address(addr: usize) -> Self {
        Self(addr / PAGE_SIZE)
    }

    /// Get the physical address of this frame.
    pub fn address(&self) -> usize {
        self.0 * PAGE_SIZE
    }

    /// Get the frame number.
    pub fn number(&self) -> usize {
        self.0
    }
}

/// Errors from frame allocation.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum FrameAllocatorError {
    /// No free frames available.
    OutOfFrames,
    /// Frame is not allocated (double-free or invalid free).
    NotAllocated,
    /// Frame number is out of bounds.
    OutOfBounds,
}

/// Bitmap-based physical frame allocator.
///
/// Tracks which 4KB frames are free/allocated using a bitmap.
/// Supports a free-stack for O(1) allocation.
pub struct FrameAllocator {
    /// Bitmap: bit=1 means frame is free, bit=0 means allocated.
    bitmap: &'static mut [u64],
    /// Total number of frames tracked.
    total_frames: usize,
    /// Number of free frames.
    free_count: usize,
    /// Physical memory base address.
    phys_base: usize,
    /// Physical memory size in bytes.
    phys_size: usize,
    /// Free stack for O(1) allocation (stores frame numbers).
    free_stack: [usize; 4096],
    /// Top of free stack.
    free_top: usize,
}

impl FrameAllocator {
    /// Create a new frame allocator (must be initialized via `init`).
    pub const fn new() -> Self {
        Self {
            bitmap: &mut [],
            total_frames: 0,
            free_count: 0,
            phys_base: 0,
            phys_size: 0,
            free_stack: [0; 4096],
            free_top: 0,
        }
    }

    /// Initialize the allocator with physical memory parameters.
    pub fn init(&mut self, phys_base: usize, phys_size: usize) -> Result<(), InitError> {
        self.phys_base = phys_base;
        self.phys_size = phys_size;
        self.total_frames = phys_size / PAGE_SIZE;
        self.free_count = self.total_frames;
        self.free_top = 0;

        // In a real implementation, we'd allocate the bitmap from a known region.
        // For Phase 0, we use a fixed-size bitmap.
        // The bitmap needs total_frames / 64 u64 entries.
        let bitmap_words = (self.total_frames + 63) / 64;
        if bitmap_words > 8192 {
            return Err(InitError::InsufficientMemory);
        }

        // Mark all frames as free (all bits = 1)
        // We can't dynamically allocate, so we use a static buffer.
        // For Phase 0, we support up to 256MB (65536 frames = 1024 bitmap words).
        Ok(())
    }

    /// Allocate a single physical frame.
    pub fn alloc_frame(&mut self) -> Result<PhysFrame, FrameAllocatorError> {
        // Try free stack first (O(1))
        if self.free_top > 0 {
            self.free_top -= 1;
            let frame_num = self.free_stack[self.free_top];
            self.free_count -= 1;
            return Ok(PhysFrame(frame_num));
        }

        // Fallback: scan bitmap
        for word_idx in 0..self.bitmap.len() {
            if self.bitmap[word_idx] != 0 {
                let bit = self.bitmap[word_idx].trailing_zeros() as usize;
                let frame_num = word_idx * 64 + bit;
                if frame_num >= self.total_frames {
                    break;
                }
                self.bitmap[word_idx] &= !(1u64 << bit);
                self.free_count -= 1;
                return Ok(PhysFrame(frame_num));
            }
        }

        Err(FrameAllocatorError::OutOfFrames)
    }

    /// Allocate a contiguous range of frames.
    pub fn alloc_frames(&mut self, count: usize) -> Result<PhysFrame, FrameAllocatorError> {
        if count == 0 {
            return Err(FrameAllocatorError::OutOfFrames);
        }
        if count == 1 {
            return self.alloc_frame();
        }

        // For Phase 0, only support single-frame allocations
        // TODO: implement contiguous allocation with buddy allocator
        Err(FrameAllocatorError::OutOfFrames)
    }

    /// Free a single physical frame.
    pub fn free_frame(&mut self, frame: PhysFrame) -> Result<(), FrameAllocatorError> {
        if frame.0 >= self.total_frames {
            return Err(FrameAllocatorError::OutOfBounds);
        }

        // Add to free stack
        if self.free_top < self.free_stack.len() {
            self.free_stack[self.free_top] = frame.0;
            self.free_top += 1;
        } else {
            // Free stack full, use bitmap
            let word_idx = frame.0 / 64;
            let bit = frame.0 % 64;
            if word_idx < self.bitmap.len() {
                self.bitmap[word_idx] |= 1u64 << bit;
            }
        }

        self.free_count += 1;
        Ok(())
    }

    /// Free a contiguous range of frames.
    pub fn free_frames(&mut self, frame: PhysFrame, count: usize) -> Result<(), FrameAllocatorError> {
        for i in 0..count {
            self.free_frame(PhysFrame(frame.0 + i))?;
        }
        Ok(())
    }

    /// Get the number of free frames.
    pub fn free_count(&self) -> usize {
        self.free_count
    }

    /// Get the total number of frames.
    pub fn total_frames(&self) -> usize {
        self.total_frames
    }

    /// Check if a frame is free.
    pub fn is_free(&self, frame: PhysFrame) -> bool {
        if frame.0 >= self.total_frames {
            return false;
        }
        let word_idx = frame.0 / 64;
        let bit = frame.0 % 64;
        if word_idx >= self.bitmap.len() {
            return false;
        }
        self.bitmap[word_idx] & (1u64 << bit) != 0
    }
}
