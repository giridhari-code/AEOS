//! Memory region types and management.

/// Type of memory region.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum MemoryRegionType {
    /// Available RAM.
    Available,
    /// Reserved by hardware.
    Reserved,
    /// ACPI data.
    AcpiData,
    /// ACPI NVS (non-volatile storage).
    AcpiNvs,
    /// Bad memory (defective).
    BadMemory,
    /// Kernel code/data.
    Kernel,
    /// Bootloader data.
    Bootloader,
}

/// A contiguous memory region.
#[derive(Debug, Clone, Copy)]
pub struct MemoryRegion {
    /// Physical start address.
    pub start: usize,
    /// Size in bytes.
    pub size: usize,
    /// Region type.
    pub region_type: MemoryRegionType,
}

impl MemoryRegion {
    /// Create a new memory region.
    pub const fn new(start: usize, size: usize, region_type: MemoryRegionType) -> Self {
        Self {
            start,
            size,
            region_type,
        }
    }

    /// Get the end address (exclusive).
    pub const fn end(&self) -> usize {
        self.start + self.size
    }

    /// Check if this region is available for allocation.
    pub const fn is_available(&self) -> bool {
        matches!(self.region_type, MemoryRegionType::Available)
    }

    /// Check if a given address falls within this region.
    pub const fn contains(&self, addr: usize) -> bool {
        addr >= self.start && addr < self.end()
    }

    /// Check if this region overlaps with another.
    pub const fn overlaps(&self, other: &MemoryRegion) -> bool {
        self.start < other.end() && other.start < self.end()
    }

    /// Split region at a given address.
    ///
    /// Returns (before, after) if split is possible.
    pub fn split_at(&self, addr: usize) -> Option<(MemoryRegion, MemoryRegion)> {
        if addr <= self.start || addr >= self.end() {
            return None;
        }

        let before = MemoryRegion::new(self.start, addr - self.start, self.region_type);
        let after = MemoryRegion::new(addr, self.end() - addr, self.region_type);
        Some((before, after))
    }
}

/// Iterator over memory regions from a memory map.
pub struct MemoryRegionIterator {
    regions: &'static [MemoryRegion],
    index: usize,
}

impl MemoryRegionIterator {
    /// Create a new iterator.
    pub fn new(regions: &'static [MemoryRegion]) -> Self {
        Self { regions, index: 0 }
    }
}

impl Iterator for MemoryRegionIterator {
    type Item = MemoryRegion;

    fn next(&mut self) -> Option<Self::Item> {
        if self.index < self.regions.len() {
            let region = self.regions[self.index];
            self.index += 1;
            Some(region)
        } else {
            None
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_region_contains() {
        let region = MemoryRegion::new(0x1000, 0x2000, MemoryRegionType::Available);
        assert!(region.contains(0x1000));
        assert!(region.contains(0x2000));
        assert!(!region.contains(0x0FFF));
        assert!(!region.contains(0x3000));
    }

    #[test]
    fn test_region_overlaps() {
        let r1 = MemoryRegion::new(0x1000, 0x2000, MemoryRegionType::Available);
        let r2 = MemoryRegion::new(0x2000, 0x2000, MemoryRegionType::Available);
        let r3 = MemoryRegion::new(0x4000, 0x1000, MemoryRegionType::Available);

        assert!(r1.overlaps(&r2));
        assert!(!r1.overlaps(&r3));
    }

    #[test]
    fn test_region_split() {
        let region = MemoryRegion::new(0x1000, 0x3000, MemoryRegionType::Available);
        let (before, after) = region.split_at(0x2000).unwrap();

        assert_eq!(before.start, 0x1000);
        assert_eq!(before.size, 0x1000);
        assert_eq!(after.start, 0x2000);
        assert_eq!(after.size, 0x2000);
    }
}
