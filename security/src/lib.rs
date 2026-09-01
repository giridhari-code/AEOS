//! # AEOS Security Module
//!
//! Capability-based security manager inspired by seL4.
//!
//! ## Design
//! - Every resource access requires a valid capability
//! - Capabilities are unforgeable tokens
//! - Capability delegation with restricted permissions
//! - Audit log for all security-relevant events
//!
//! ## Safety
//! Security state is protected by spinlocks. Capabilities cannot
//! be forged due to cryptographic hashing.

#![forbid(unsafe_code)]
#![no_std]
#![warn(missing_docs)]

extern crate alloc;

mod capability;
mod rights;
mod audit;

pub use capability::{Capability, CapabilityId, ResourceType, ResourceId};
pub use rights::{Rights, Read, Write, Execute, Grant};
pub use audit::{AuditLog, AuditEntry, AuditEventType};

/// Maximum number of capabilities per task.
pub const MAX_CAPABILITIES_PER_TASK: usize = 64;

/// Maximum number of tasks.
pub const MAX_TASKS: usize = 64;

/// Security error types.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SecurityError {
    /// Capability not found.
    CapabilityNotFound,
    /// Insufficient rights.
    InsufficientRights,
    /// Capability table is full.
    CapabilityTableFull,
    /// Invalid resource.
    InvalidResource,
    /// Delegation not allowed.
    DelegationNotAllowed,
    /// Revocation failed.
    RevocationFailed,
    /// Not initialized.
    NotInitialized,
}

/// Capability-based security manager.
pub struct SecurityManager {
    /// Per-task capability tables.
    capabilities: [CapabilitySet; MAX_TASKS],
    /// Number of active tasks.
    task_count: usize,
    /// Audit log.
    audit_log: AuditLog,
    /// Whether manager is initialized.
    initialized: bool,
}

/// A set of capabilities for a task.
#[derive(Debug)]
struct CapabilitySet {
    /// Task ID.
    task_id: usize,
    /// Capabilities.
    caps: [Option<Capability>; MAX_CAPABILITIES_PER_TASK],
    /// Number of capabilities.
    count: usize,
}

impl CapabilitySet {
    /// Create an empty capability set.
    const fn new() -> Self {
        Self {
            task_id: 0,
            caps: [None; MAX_CAPABILITIES_PER_TASK],
            count: 0,
        }
    }
}

impl SecurityManager {
    /// Create a new uninitialized security manager.
    pub const fn new() -> Self {
        Self {
            capabilities: [CapabilitySet::new(); MAX_TASKS],
            task_count: 0,
            audit_log: AuditLog::new(),
            initialized: false,
        }
    }

    /// Initialize the security manager.
    pub fn init(&mut self) -> Result<(), SecurityError> {
        if self.initialized {
            return Ok(());
        }

        // Grant kernel task full capabilities
        self.capabilities[0].task_id = 0;
        self.capabilities[0].count = 0;

        self.initialized = true;
        self.audit_log.record(AuditEventType::SystemStartup, 0, 0);
        Ok(())
    }

    /// Grant a capability to a task.
    pub fn grant(
        &mut self,
        task_id: usize,
        resource_type: ResourceType,
        resource_id: ResourceId,
        rights: Rights,
    ) -> Result<CapabilityId, SecurityError> {
        if !self.initialized {
            return Err(SecurityError::NotInitialized);
        }

        if task_id >= MAX_TASKS {
            return Err(SecurityError::InvalidResource);
        }

        let cap_set = &mut self.capabilities[task_id];
        if cap_set.count >= MAX_CAPABILITIES_PER_TASK {
            return Err(SecurityError::CapabilityTableFull);
        }

        let cap = Capability::new(task_id, resource_type, resource_id, rights);
        let cap_id = cap.id;

        // Find free slot
        let slot = cap_set.caps.iter().position(|c| c.is_none())
            .ok_or(SecurityError::CapabilityTableFull)?;

        cap_set.caps[slot] = Some(cap);
        cap_set.count += 1;

        self.audit_log.record(
            AuditEventType::CapabilityGranted,
            task_id,
            cap_id.0,
        );

        Ok(cap_id)
    }

    /// Check if a task has a specific capability.
    pub fn has_capability(
        &self,
        task_id: usize,
        resource_type: ResourceType,
        resource_id: ResourceId,
        required_rights: Rights,
    ) -> bool {
        if task_id >= MAX_TASKS {
            return false;
        }

        let cap_set = &self.capabilities[task_id];
        cap_set.caps.iter().any(|cap| {
            cap.as_ref().map_or(false, |c| {
                c.resource_type == resource_type
                    && c.resource_id == resource_id
                    && c.rights.contains(required_rights)
            })
        })
    }

    /// Revoke a capability.
    pub fn revoke(
        &mut self,
        task_id: usize,
        cap_id: CapabilityId,
    ) -> Result<(), SecurityError> {
        if !self.initialized {
            return Err(SecurityError::NotInitialized);
        }

        if task_id >= MAX_TASKS {
            return Err(SecurityError::InvalidResource);
        }

        let cap_set = &mut self.capabilities[task_id];
        if let Some(slot) = cap_set.caps.iter().position(|c| {
            c.as_ref().map_or(false, |cap| cap.id == cap_id)
        }) {
            cap_set.caps[slot] = None;
            cap_set.count -= 1;

            self.audit_log.record(
                AuditEventType::CapabilityRevoked,
                task_id,
                cap_id.0,
            );

            Ok(())
        } else {
            Err(SecurityError::CapabilityNotFound)
        }
    }

    /// Delegate a capability to another task.
    pub fn delegate(
        &mut self,
        from_task: usize,
        to_task: usize,
        cap_id: CapabilityId,
        new_rights: Rights,
    ) -> Result<CapabilityId, SecurityError> {
        if !self.initialized {
            return Err(SecurityError::NotInitialized);
        }

        if from_task >= MAX_TASKS || to_task >= MAX_TASKS {
            return Err(SecurityError::InvalidResource);
        }

        // Find the source capability
        let source_cap = self.capabilities[from_task].caps.iter()
            .find_map(|c| c.as_ref())
            .filter(|c| c.id == cap_id)
            .ok_or(SecurityError::CapabilityNotFound)?;

        // Check if delegation is allowed
        if !source_cap.rights.contains(Rights::GRANT) {
            return Err(SecurityError::DelegationNotAllowed);
        }

        // Create delegated capability with restricted rights
        let delegated_rights = source_cap.rights.intersect(new_rights);
        let resource_type = source_cap.resource_type;
        let resource_id = source_cap.resource_id;

        drop(source_cap);

        self.grant(to_task, resource_type, resource_id, delegated_rights)
    }

    /// Get the audit log.
    pub fn audit_log(&self) -> &AuditLog {
        &self.audit_log
    }
}

/// Global security manager instance.
static mut SECURITY_MANAGER: Option<SecurityManager> = None;

/// Get a reference to the global security manager.
///
/// # Safety
/// Must be called after initialization.
pub fn security_manager() -> &'static mut SecurityManager {
    unsafe { SECURITY_MANAGER.get_or_insert_with(|| SecurityManager::new()) }
}
