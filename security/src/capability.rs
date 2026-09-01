//! Capability types and management.

/// Unique capability identifier.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct CapabilityId(pub usize);

/// Resource types that can be controlled by capabilities.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ResourceType {
    /// Physical memory frames.
    Memory,
    /// IPC channels.
    IpcChannel,
    /// Device I/O ports.
    Device,
    /// File system nodes.
    FileSystem,
    /// Network sockets.
    Network,
    /// Task control.
    Task,
    /// Timer access.
    Timer,
}

/// Resource identifier (unique within resource type).
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct ResourceId(pub usize);

/// A capability token.
#[derive(Debug, Clone, Copy)]
pub struct Capability {
    /// Unique capability ID.
    pub id: CapabilityId,
    /// Owning task.
    pub task_id: usize,
    /// Resource type.
    pub resource_type: ResourceType,
    /// Resource identifier.
    pub resource_id: ResourceId,
    /// Granted rights.
    pub rights: Rights,
}

bitflags::bitflags! {
    /// Rights granted by a capability.
    #[derive(Debug, Clone, Copy, PartialEq, Eq)]
    pub struct Rights: u32 {
        /// Read access.
        const READ = 1 << 0;
        /// Write access.
        const WRITE = 1 << 1;
        /// Execute access.
        const EXECUTE = 1 << 2;
        /// Grant/delegate access.
        const GRANT = 1 << 3;
        /// Revoke access.
        const REVOKE = 1 << 4;
    }
}

/// Global capability ID counter.
static mut NEXT_CAP_ID: usize = 0;

impl Capability {
    /// Create a new capability.
    pub fn new(
        task_id: usize,
        resource_type: ResourceType,
        resource_id: ResourceId,
        rights: Rights,
    ) -> Self {
        let id = unsafe {
            let id = CapabilityId(NEXT_CAP_ID);
            NEXT_CAP_ID += 1;
            id
        };

        Self {
            id,
            task_id,
            resource_type,
            resource_id,
            rights,
        }
    }

    /// Check if this capability grants specific rights.
    pub fn grants(&self, required: Rights) -> bool {
        self.rights.contains(required)
    }

    /// Create a restricted copy of this capability.
    pub fn restrict(&self, new_rights: Rights) -> Self {
        let mut cap = *self;
        cap.rights = self.rights.intersect(new_rights);
        cap
    }
}
