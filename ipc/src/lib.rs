//! # AEOS IPC Module
//!
//! Inter-Process Communication via message channels.
//!
//! ## Design
//! - Bounded message queues (FIFO)
//! - Zero-copy via shared memory rings
//! - Per-channel capability checks
//! - Non-blocking send/receive with timeout support
//!
//! ## Safety
//! IPC uses spinlocks for thread safety. Message data is copied
//! from sender to receiver address space.

#![forbid(unsafe_code)]
#![no_std]
#![warn(missing_docs)]

extern crate alloc;

mod channel;
mod message;
mod envelope;

pub use channel::{Channel, ChannelId, ChannelError, ChannelConfig};
pub use message::{Message, MessageHeader, MessageType};
pub use envelope::{Envelope, EnvelopeType, EventCatalog};

/// Maximum number of IPC channels.
pub const MAX_CHANNELS: usize = 32;

/// Maximum message size in bytes.
pub const MAX_MESSAGE_SIZE: usize = 4096;

/// Maximum messages per channel.
pub const MAX_MESSAGES_PER_CHANNEL: usize = 64;

/// IPC error types.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum IpcError {
    /// Channel does not exist.
    ChannelNotFound,
    /// Channel is full.
    ChannelFull,
    /// Channel is empty.
    ChannelEmpty,
    /// Message is too large.
    MessageTooLarge,
    /// Invalid message.
    InvalidMessage,
    /// Permission denied.
    PermissionDenied,
    /// Channel already exists.
    ChannelAlreadyExists,
    /// Operation timed out.
    Timeout,
    /// Not initialized.
    NotInitialized,
}

/// Global IPC state.
pub struct IpcState {
    /// Active channels.
    channels: [Option<Channel>; MAX_CHANNELS],
    /// Number of active channels.
    channel_count: usize,
    /// Whether IPC is initialized.
    initialized: bool,
}

impl IpcState {
    /// Create a new uninitialized IPC state.
    pub const fn new() -> Self {
        Self {
            channels: [None; MAX_CHANNELS],
            channel_count: 0,
            initialized: false,
        }
    }

    /// Initialize IPC.
    pub fn init(&mut self) -> Result<(), IpcError> {
        if self.initialized {
            return Ok(());
        }
        self.initialized = true;
        Ok(())
    }

    /// Create a new channel.
    pub fn create_channel(
        &mut self,
        name: &str,
        config: ChannelConfig,
    ) -> Result<ChannelId, IpcError> {
        if !self.initialized {
            return Err(IpcError::NotInitialized);
        }

        if self.channel_count >= MAX_CHANNELS {
            return Err(IpcError::ChannelFull);
        }

        // Find free slot
        let slot = self.channels.iter().position(|c| c.is_none())
            .ok_or(IpcError::ChannelFull)?;

        let channel = Channel::new(ChannelId(slot), name, config);
        self.channels[slot] = Some(channel);
        self.channel_count += 1;

        Ok(ChannelId(slot))
    }

    /// Send a message on a channel.
    pub fn send(
        &mut self,
        channel_id: ChannelId,
        message: Message,
    ) -> Result<(), IpcError> {
        let channel = self.get_channel_mut(channel_id)?;
        channel.send(message)
    }

    /// Receive a message from a channel.
    pub fn receive(
        &mut self,
        channel_id: ChannelId,
    ) -> Result<Message, IpcError> {
        let channel = self.get_channel_mut(channel_id)?;
        channel.receive()
    }

    /// Get a channel reference.
    fn get_channel_mut(&mut self, id: ChannelId) -> Result<&mut Channel, IpcError> {
        self.channels
            .get_mut(id.0)
            .and_then(|c| c.as_mut())
            .ok_or(IpcError::ChannelNotFound)
    }

    /// Get the number of active channels.
    pub fn channel_count(&self) -> usize {
        self.channel_count
    }
}

/// Global IPC state instance.
static mut IPC_STATE: Option<IpcState> = None;

/// Get a reference to the global IPC state.
///
/// # Safety
/// Must be called after initialization and with interrupts disabled.
pub fn ipc_state() -> &'static mut IpcState {
    unsafe { IPC_STATE.get_or_insert_with(|| IpcState::new()) }
}
