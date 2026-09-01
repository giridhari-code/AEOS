//! IPC channel implementation.

use crate::{IpcError, MAX_MESSAGES_PER_CHANNEL, Message};
use alloc::collections::VecDeque;

/// Unique channel identifier.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct ChannelId(pub usize);

/// Channel configuration.
#[derive(Debug, Clone, Copy)]
pub struct ChannelConfig {
    /// Maximum messages in queue.
    pub max_messages: usize,
    /// Maximum message size.
    pub max_message_size: usize,
    /// Whether channel is bounded.
    pub bounded: bool,
}

impl Default for ChannelConfig {
    fn default() -> Self {
        Self {
            max_messages: MAX_MESSAGES_PER_CHANNEL,
            max_message_size: crate::MAX_MESSAGE_SIZE,
            bounded: true,
        }
    }
}

/// An IPC channel.
#[derive(Debug)]
pub struct Channel {
    /// Channel ID.
    pub id: ChannelId,
    /// Channel name.
    pub name: &'static str,
    /// Configuration.
    pub config: ChannelConfig,
    /// Message queue.
    queue: VecDeque<Message>,
    /// Number of messages sent.
    pub messages_sent: usize,
    /// Number of messages received.
    pub messages_received: usize,
    /// Whether channel is closed.
    pub closed: bool,
}

impl Channel {
    /// Create a new channel.
    pub fn new(id: ChannelId, name: &'static str, config: ChannelConfig) -> Self {
        Self {
            id,
            name,
            config,
            queue: VecDeque::new(),
            messages_sent: 0,
            messages_received: 0,
            closed: false,
        }
    }

    /// Send a message on this channel.
    pub fn send(&mut self, message: Message) -> Result<(), IpcError> {
        if self.closed {
            return Err(IpcError::ChannelNotFound);
        }

        if message.header.size > self.config.max_message_size {
            return Err(IpcError::MessageTooLarge);
        }

        if self.config.bounded && self.queue.len() >= self.config.max_messages {
            return Err(IpcError::ChannelFull);
        }

        self.queue.push_back(message);
        self.messages_sent += 1;
        Ok(())
    }

    /// Receive a message from this channel.
    pub fn receive(&mut self) -> Result<Message, IpcError> {
        if self.closed {
            return Err(IpcError::ChannelNotFound);
        }

        self.queue
            .pop_front()
            .map(|msg| {
                self.messages_received += 1;
                msg
            })
            .ok_or(IpcError::ChannelEmpty)
    }

    /// Peek at the next message without removing.
    pub fn peek(&self) -> Result<&Message, IpcError> {
        if self.closed {
            return Err(IpcError::ChannelNotFound);
        }

        self.queue.front().ok_or(IpcError::ChannelEmpty)
    }

    /// Close the channel.
    pub fn close(&mut self) {
        self.closed = true;
        self.queue.clear();
    }

    /// Check if channel is empty.
    pub fn is_empty(&self) -> bool {
        self.queue.is_empty()
    }

    /// Get number of pending messages.
    pub fn pending(&self) -> usize {
        self.queue.len()
    }
}
