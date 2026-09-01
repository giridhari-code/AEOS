//! Security audit log.

/// Audit event types.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum AuditEventType {
    /// System startup.
    SystemStartup,
    /// Capability granted.
    CapabilityGranted,
    /// Capability revoked.
    CapabilityRevoked,
    /// Capability delegation.
    CapabilityDelegated,
    /// Access denied.
    AccessDenied,
    /// Security violation.
    SecurityViolation,
    /// Task created.
    TaskCreated,
    /// Task terminated.
    TaskTerminated,
}

/// A single audit log entry.
#[derive(Debug, Clone, Copy)]
pub struct AuditEntry {
    /// Event type.
    pub event_type: AuditEventType,
    /// Task ID (if applicable).
    pub task_id: usize,
    /// Resource ID (if applicable).
    pub resource_id: usize,
    /// Timestamp.
    pub timestamp: u64,
}

/// Circular audit log.
pub struct AuditLog {
    /// Log entries.
    entries: [AuditEntry; 256],
    /// Write index.
    write_idx: usize,
    /// Total entries (may exceed buffer size).
    total_entries: usize,
}

impl AuditLog {
    /// Create a new empty audit log.
    pub const fn new() -> Self {
        Self {
            entries: [AuditEntry {
                event_type: AuditEventType::SystemStartup,
                task_id: 0,
                resource_id: 0,
                timestamp: 0,
            }; 256],
            write_idx: 0,
            total_entries: 0,
        }
    }

    /// Record an audit event.
    pub fn record(&mut self, event_type: AuditEventType, task_id: usize, resource_id: usize) {
        let entry = AuditEntry {
            event_type,
            task_id,
            resource_id,
            timestamp: 0, // Would use timer in real implementation
        };

        self.entries[self.write_idx] = entry;
        self.write_idx = (self.write_idx + 1) % 256;
        self.total_entries += 1;
    }

    /// Get the most recent entries.
    pub fn recent(&self, count: usize) -> &[AuditEntry] {
        let available = count.min(self.total_entries).min(256);
        let start = if self.total_entries > 256 {
            self.write_idx
        } else {
            0
        };
        &self.entries[start..start + available]
    }

    /// Get total number of entries recorded.
    pub fn total_entries(&self) -> usize {
        self.total_entries
    }
}
