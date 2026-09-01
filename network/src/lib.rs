//! # AEOS Network Module
//!
//! TCP/IP stack MVP for AEOS.
//!
//! ## Design
//! - Minimal TCP/IP implementation
//! - ARP, IPv4, ICMP, UDP, TCP
//! - Socket abstraction via capabilities
//!
//! ## Status
//! Phase 0: Stub implementation

#![forbid(unsafe_code)]
#![no_std]
#![warn(missing_docs)]

/// Network error types.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum NetworkError {
    /// Not initialized.
    NotInitialized,
    /// Socket not found.
    SocketNotFound,
    /// Connection failed.
    ConnectionFailed,
    /// Send failed.
    SendFailed,
    /// Receive failed.
    ReceiveFailed,
}

/// Initialize the network stack.
pub fn init() -> Result<(), NetworkError> {
    // Phase 0: stub
    Ok(())
}
