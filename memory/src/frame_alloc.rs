//! Physical frame allocator.
//!
//! Bitmap-based allocator for 4KB physical frames. Supports alloc/free
//! with O(1) amortized time using a free-stack.

use crate::{InitError, PAGE_SIZE};

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

/// Maximum number of bitmap words (supports up to 512MB RAM).
/// 512MB / 4KB = 131072 frames / 64 = 2048 words.
const BITMAP_WORDS: usize = 2048;

/// Maximum free stack entries.
const FREE_STACK_SIZE: usize = 8192;

/// Bitmap-based physical frame allocator.
///
/// Tracks which 4KB frames are free/allocated using a bitmap.
/// Supports a free-stack for O(1) allocation.
pub struct FrameAllocator {
    /// Bitmap: bit=1 means frame is free, bit=0 means allocated.
    bitmap: [u64; BITMAP_WORDS],
    /// Total number of frames tracked.
    total_frames: usize,
    /// Number of free frames.
    free_count: usize,
    /// Physical memory base address.
    phys_base: usize,
    /// Physical memory size in bytes.
    phys_size: usize,
    /// Free stack for O(1) allocation (stores frame numbers).
    free_stack: [usize; FREE_STACK_SIZE],
    /// Top of free stack.
    free_top: usize,
}

impl FrameAllocator {
    /// Create a new frame allocator (must be initialized via `init`).
    pub const fn new() -> Self {
        Self {
            bitmap: [0u64; BITMAP_WORDS],
            total_frames: 0,
            free_count: 0,
            phys_base: 0,
            phys_size: 0,
            free_stack: [0; FREE_STACK_SIZE],
            free_top: 0,
        }
    }

    /// Initialize the allocator with physical memory parameters.
    ///
    /// Marks frames 0..reserved_end as allocated (kernel, boot structures),
    /// then marks remaining frames as free in the bitmap and pushes them
    /// onto the free stack.
    ///
    /// `reserved_end` is the physical address above which frames are free.
    /// Frames below `reserved_end` are considered in-use by the kernel.
    pub fn init(
        &mut self,
        phys_base: usize,
        phys_size: usize,
        reserved_end: usize,
    ) -> Result<(), InitError> {
        self.phys_base = phys_base;
        self.phys_size = phys_size;
        self.total_frames = phys_size / PAGE_SIZE;
        self.free_top = 0;

        let bitmap_words = (self.total_frames + 63) / 64;
        if bitmap_words > BITMAP_WORDS {
            return Err(InitError::InsufficientMemory);
        }

        // Zero the bitmap (all frames marked allocated)
        for word in self.bitmap.iter_mut().take(bitmap_words) {
            *word = 0;
        }

        // Calculate first free frame number
        let reserved_frames = (reserved_end + PAGE_SIZE - 1) / PAGE_SIZE;
        let first_free = reserved_frames.max(1); // Always reserve frame 0

        // Mark free frames in bitmap and push to free stack
        self.free_count = 0;
        for frame_num in first_free..self.total_frames {
            let word_idx = frame_num / 64;
            let bit = frame_num % 64;
            self.bitmap[word_idx] |= 1u64 << bit;
            self.free_count += 1;

            // Push to free stack if there's room
            if self.free_top < FREE_STACK_SIZE {
                self.free_stack[self.free_top] = frame_num;
                self.free_top += 1;
            }
        }

        Ok(())
    }

    /// Allocate a single physical frame.
    pub fn alloc_frame(&mut self) -> Result<PhysFrame, FrameAllocatorError> {
        // Try free stack first (O(1))
        if self.free_top > 0 {
            self.free_top -= 1;
            let frame_num = self.free_stack[self.free_top];
            // Clear the bit in bitmap (mark allocated)
            let word_idx = frame_num / 64;
            let bit = frame_num % 64;
            if word_idx < self.bitmap.len() {
                self.bitmap[word_idx] &= !(1u64 << bit);
            }
            self.free_count -= 1;
            return Ok(PhysFrame(frame_num));
        }

        // Fallback: scan bitmap for a free frame
        let bitmap_words = (self.total_frames + 63) / 64;
        for word_idx in 0..bitmap_words {
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

        // Scan bitmap for `count` contiguous free frames
        let bitmap_words = (self.total_frames + 63) / 64;
        for word_idx in 0..bitmap_words {
            if self.bitmap[word_idx] != 0 {
                let start_bit = self.bitmap[word_idx].trailing_zeros() as usize;
                let start_frame = word_idx * 64 + start_bit;

                if start_frame + count > self.total_frames {
                    continue;
                }

                // Check if `count` contiguous frames are free
                let mut contiguous = true;
                for offset in 0..count {
                    let frame_num = start_frame + offset;
                    let w = frame_num / 64;
                    let b = frame_num % 64;
                    if w >= bitmap_words || self.bitmap[w] & (1u64 << b) == 0 {
                        contiguous = false;
                        break;
                    }
                }

                if contiguous {
                    // Mark all frames as allocated
                    for offset in 0..count {
                        let frame_num = start_frame + offset;
                        let w = frame_num / 64;
                        let b = frame_num % 64;
                        self.bitmap[w] &= !(1u64 << b);
                    }
                    self.free_count -= count;
                    return Ok(PhysFrame(start_frame));
                }
            }
        }

        Err(FrameAllocatorError::OutOfFrames)
    }

    /// Free a single physical frame.
    pub fn free_frame(&mut self, frame: PhysFrame) -> Result<(), FrameAllocatorError> {
        if frame.0 >= self.total_frames {
            return Err(FrameAllocatorError::OutOfBounds);
        }

        // Check if already free (double-free detection)
        let word_idx = frame.0 / 64;
        let bit = frame.0 % 64;
        if word_idx < self.bitmap.len() && self.bitmap[word_idx] & (1u64 << bit) != 0 {
            return Err(FrameAllocatorError::NotAllocated);
        }

        // Mark as free in bitmap
        if word_idx < self.bitmap.len() {
            self.bitmap[word_idx] |= 1u64 << bit;
        }

        // Add to free stack
        if self.free_top < FREE_STACK_SIZE {
            self.free_stack[self.free_top] = frame.0;
            self.free_top += 1;
        }

        self.free_count += 1;
        Ok(())
    }

    /// Free a contiguous range of frames.
    pub fn free_frames(
        &mut self,
        frame: PhysFrame,
        count: usize,
    ) -> Result<(), FrameAllocatorError> {
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

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn alloc_returns_distinct_frames() {
        let mut a = FrameAllocator::new();
        // 128MB RAM at 0x4000_0000, reserved up to 18MB (kernel end)
        a.init(0x4000_0000, 128 * 1024 * 1024, 18 * 1024 * 1024)
            .unwrap();
        let f1 = a.alloc_frame().unwrap();
        let f2 = a.alloc_frame().unwrap();
        assert_ne!(f1, f2);
    }

    #[test]
    fn free_and_realloc() {
        let mut a = FrameAllocator::new();
        a.init(0x4000_0000, 128 * 1024 * 1024, 18 * 1024 * 1024)
            .unwrap();
        let f = a.alloc_frame().unwrap();
        a.free_frame(f).unwrap();
        let f2 = a.alloc_frame().unwrap();
        assert_eq!(f, f2);
    }

    #[test]
    fn free_count_decreases_on_alloc() {
        let mut a = FrameAllocator::new();
        a.init(0x4000_0000, 128 * 1024 * 1024, 18 * 1024 * 1024)
            .unwrap();
        let before = a.free_count();
        let _f = a.alloc_frame().unwrap();
        assert_eq!(a.free_count(), before - 1);
    }

    #[test]
    fn free_count_increases_on_free() {
        let mut a = FrameAllocator::new();
        a.init(0x4000_0000, 128 * 1024 * 1024, 18 * 1024 * 1024)
            .unwrap();
        let f = a.alloc_frame().unwrap();
        let before = a.free_count();
        a.free_frame(f).unwrap();
        assert_eq!(a.free_count(), before + 1);
    }

    #[test]
    fn double_free_detected() {
        let mut a = FrameAllocator::new();
        a.init(0x4000_0000, 128 * 1024 * 1024, 18 * 1024 * 1024)
            .unwrap();
        let f = a.alloc_frame().unwrap();
        a.free_frame(f).unwrap();
        assert_eq!(a.free_frame(f), Err(FrameAllocatorError::NotAllocated));
    }

    #[test]
    fn is_free_reflects_state() {
        let mut a = FrameAllocator::new();
        a.init(0x4000_0000, 128 * 1024 * 1024, 18 * 1024 * 1024)
            .unwrap();
        let f = a.alloc_frame().unwrap();
        assert!(!a.is_free(f));
        a.free_frame(f).unwrap();
        assert!(a.is_free(f));
    }

    #[test]
    fn alloc_frames_contiguous() {
        let mut a = FrameAllocator::new();
        a.init(0x4000_0000, 128 * 1024 * 1024, 18 * 1024 * 1024)
            .unwrap();
        let f = a.alloc_frames(4).unwrap();
        // All 4 frames should be allocated (not free)
        for i in 0..4 {
            assert!(!a.is_free(PhysFrame(f.0 + i)));
        }
    }
}
