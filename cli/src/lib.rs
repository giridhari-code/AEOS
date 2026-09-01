//! # AEOS CLI Shell
//!
//! Interactive command-line interface for the kernel.
//!
//! ## Commands
//! - `help` - Show available commands
//! - `ps` - List processes
//! - `mem` - Show memory stats
//! - `ai` - Show AI engine status
//! - `uptime` - Show uptime
//! - `reboot` - Reboot system

#![forbid(unsafe_code)]
#![no_std]
#![warn(missing_docs)]

extern crate alloc;

mod commands;

use commands::{Command, CommandError};

/// Maximum line length.
const MAX_LINE: usize = 64;

/// Shell state.
pub struct Shell {
    /// Input buffer.
    buffer: [u8; MAX_LINE],
    /// Current buffer position.
    position: usize,
    /// Whether shell is initialized.
    initialized: bool,
}

impl Shell {
    /// Create a new shell.
    pub const fn new() -> Self {
        Self {
            buffer: [0u8; MAX_LINE],
            position: 0,
            initialized: false,
        }
    }

    /// Initialize the shell.
    pub fn init(&mut self) -> Result<(), ShellError> {
        if self.initialized {
            return Ok(());
        }

        self.initialized = true;
        self.print_prompt();
        Ok(())
    }

    /// Process a character input.
    pub fn process_char(&mut self, c: u8) -> Option<ShellEvent> {
        if !self.initialized {
            return None;
        }

        match c {
            b'\r' | b'\n' => {
                let line = self.get_line();
                self.clear_line();
                self.print_prompt();

                if line.is_empty() {
                    return None;
                }

                self.execute_line(line)
            }
            0x7F | b'\b' => {
                if self.position > 0 {
                    self.position -= 1;
                    Some(ShellEvent::Backspace)
                } else {
                    None
                }
            }
            0x20..=0x7E => {
                if self.position < MAX_LINE - 1 {
                    self.buffer[self.position] = c;
                    self.position += 1;
                    Some(ShellEvent::Char(c))
                } else {
                    None
                }
            }
            _ => None,
        }
    }

    /// Get the current line as a string.
    fn get_line(&self) -> &str {
        core::str::from_utf8(&self.buffer[..self.position]).unwrap_or("")
    }

    /// Clear the input buffer.
    fn clear_line(&mut self) {
        self.position = 0;
        self.buffer = [0u8; MAX_LINE];
    }

    /// Print the shell prompt.
    fn print_prompt(&self) {
        // Would print "aeos> " to serial console
    }

    /// Execute a command line.
    fn execute_line(&self, line: &str) -> Option<ShellEvent> {
        let parts: alloc::vec::Vec<&str> = line.split_whitespace().collect();
        if parts.is_empty() {
            return None;
        }

        let cmd = parts[0];
        let args = &parts[1..];

        match Command::from_str(cmd) {
            Ok(command) => {
                match command.execute(args) {
                    Ok(output) => Some(ShellEvent::Output(output)),
                    Err(e) => Some(ShellEvent::Error(e)),
                }
            }
            Err(_) => Some(ShellEvent::Error(CommandError::UnknownCommand)),
        }
    }
}

/// Shell events.
#[derive(Debug, Clone)]
pub enum ShellEvent {
    /// Character input.
    Char(u8),
    /// Backspace.
    Backspace,
    /// Command output.
    Output(alloc::string::String),
    /// Error message.
    Error(CommandError),
}

/// Shell errors.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ShellError {
    /// Shell not initialized.
    NotInitialized,
    /// Input buffer full.
    BufferFull,
}

/// Global shell instance.
static mut SHELL: Option<Shell> = None;

/// Get a reference to the global shell.
///
/// # Safety
/// Must be called after initialization.
pub fn shell() -> &'static mut Shell {
    unsafe { SHELL.get_or_insert_with(|| Shell::new()) }
}
