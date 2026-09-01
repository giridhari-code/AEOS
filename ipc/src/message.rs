//! IPC message types.

/// Message type identifier.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum MessageType {
    /// Data message.
    Data,
    /// Control message.
    Control,
    /// Synchronization message.
    Sync,
    /// Event notification.
    Event,
}

/// Message header.
#[derive(Debug, Clone, Copy)]
pub struct MessageHeader {
    /// Message type.
    pub msg_type: MessageType,
    /// Sender task ID.
    pub sender: usize,
    /// Message size in bytes.
    pub size: usize,
    /// Message flags.
    pub flags: u32,
}

/// An IPC message.
#[derive(Debug, Clone)]
pub struct Message {
    /// Message header.
    pub header: MessageHeader,
    /// Message data.
    pub data: [u8; 64],
}

impl Message {
    /// Create a new message.
    pub fn new(msg_type: MessageType, sender: usize, data: &[u8]) -> Self {
        let mut msg = Self {
            header: MessageHeader {
                msg_type,
                sender,
                size: data.len().min(64),
                flags: 0,
            },
            data: [0u8; 64],
        };
        msg.data[..msg.header.size].copy_from_slice(data);
        msg
    }

    /// Get the message type.
    pub fn msg_type(&self) -> MessageType {
        self.header.msg_type
    }

    /// Get the sender.
    pub fn sender(&self) -> usize {
        self.header.sender
    }

    /// Get the data slice.
    pub fn data(&self) -> &[u8] {
        &self.data[..self.header.size]
    }

    /// Get the data as a string (if valid UTF-8).
    pub fn data_str(&self) -> Option<&str> {
        core::str::from_utf8(self.data()).ok()
    }
}
