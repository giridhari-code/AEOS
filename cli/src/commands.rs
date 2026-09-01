//! Shell commands.

use alloc::string::String;

/// Shell command error types.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum CommandError {
    /// Unknown command.
    UnknownCommand,
    /// Invalid arguments.
    InvalidArguments,
    /// Command failed.
    Failed,
}

/// Available shell commands.
#[derive(Debug, Clone, Copy)]
pub enum Command {
    /// Show help.
    Help,
    /// List processes.
    Ps,
    /// Show memory stats.
    Mem,
    /// Show AI status.
    Ai,
    /// Show uptime.
    Uptime,
    /// Reboot system.
    Reboot,
}

impl Command {
    /// Parse a command from string.
    pub fn from_str(s: &str) -> Result<Self, CommandError> {
        match s {
            "help" => Ok(Command::Help),
            "ps" => Ok(Command::Ps),
            "mem" => Ok(Command::Mem),
            "ai" => Ok(Command::Ai),
            "uptime" => Ok(Command::Uptime),
            "reboot" => Ok(Command::Reboot),
            _ => Err(CommandError::UnknownCommand),
        }
    }

    /// Execute the command with given arguments.
    pub fn execute(&self, args: &[&str]) -> Result<String, CommandError> {
        match self {
            Command::Help => Ok(self.help_text()),
            Command::Ps => self.cmd_ps(args),
            Command::Mem => self.cmd_mem(args),
            Command::Ai => self.cmd_ai(args),
            Command::Uptime => self.cmd_uptime(args),
            Command::Reboot => self.cmd_reboot(args),
        }
    }

    /// Get help text.
    fn help_text(&self) -> String {
        String::from(
            "commands:\n  help          this text\n  ps            task table\n  mem           memory stats\n  ai            AI engine report\n  uptime        ticks since boot\n  reboot        reset system"
        )
    }

    /// List processes.
    fn cmd_ps(&self, _args: &[&str]) -> Result<String, CommandError> {
        // Would list tasks from scheduler
        Ok(String::from("  PID  NAME  STATE  PRIORITY\n  0    idle  Ready  0"))
    }

    /// Show memory stats.
    fn cmd_mem(&self, _args: &[&str]) -> Result<String, CommandError> {
        // Would show memory stats
        Ok(String::from("  pmm: 1024/32768 pages free\n  heap used: 4096 bytes"))
    }

    /// Show AI status.
    fn cmd_ai(&self, _args: &[&str]) -> Result<String, CommandError> {
        // Would show AI engine status
        Ok(String::from("  AI healthy: yes\n  AI inferences: 0\n  AI last action: SUSTAIN"))
    }

    /// Show uptime.
    fn cmd_uptime(&self, _args: &[&str]) -> Result<String, CommandError> {
        // Would show uptime
        Ok(String::from("  uptime: 0 ticks (~0 s)"))
    }

    /// Reboot system.
    fn cmd_reboot(&self, _args: &[&str]) -> Result<String, CommandError> {
        // Would reboot system
        Ok(String::from("  rebooting..."))
    }
}
