//! System call interface.

/// System call numbers.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(usize)]
pub enum Syscall {
    /// Print a message.
    Print = 1,
    /// Get timer ticks.
    Ticks = 2,
    /// Yield CPU.
    Yield = 3,
    /// Exit task.
    Exit = 4,
    /// Read from device.
    Read = 5,
    /// Write to device.
    Write = 6,
    /// Open device.
    Open = 7,
    /// Close device.
    Close = 8,
    /// Device control.
    Ioctl = 9,
    /// Create file.
    Create = 10,
    /// Delete file.
    Delete = 11,
    /// List files.
    List = 12,
    /// Spawn task.
    TaskSpawn = 13,
    /// Exit task with code.
    TaskExit = 14,
    /// Print AI report.
    AiReport = 15,
    /// Create IPC channel.
    IpcCreate = 16,
    /// Send IPC message.
    IpcSend = 17,
    /// Receive IPC message.
    IpcRecv = 18,
    /// Destroy IPC channel.
    IpcDestroy = 19,
    /// Get process ID.
    GetPid = 20,
    /// Wait for process.
    WaitPid = 21,
}

/// System call error types.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SyscallError {
    /// Invalid system call number.
    InvalidSyscall,
    /// Invalid argument.
    InvalidArgument,
    /// Permission denied.
    PermissionDenied,
    /// Resource not found.
    NotFound,
    /// Resource full.
    ResourceFull,
    /// Operation failed.
    OperationFailed,
}

/// System call result type.
pub type SyscallResult = Result<usize, SyscallError>;

/// Handle a system call.
pub fn handle_syscall(number: usize, arg0: usize, arg1: usize, arg2: usize, arg3: usize) -> SyscallResult {
    let syscall = Syscall::try_from(number).map_err(|_| SyscallError::InvalidSyscall)?;

    match syscall {
        Syscall::Print => {
            // Would print message from arg0
            Ok(0)
        }
        Syscall::Ticks => {
            // Would return timer ticks
            Ok(0)
        }
        Syscall::Yield => {
            aeos_scheduler::scheduler().schedule();
            Ok(0)
        }
        Syscall::Exit => {
            let task_id = aeos_scheduler::scheduler().current_task()
                .ok_or(SyscallError::OperationFailed)?;
            aeos_scheduler::scheduler().exit_task(task_id, arg0 as i32)
                .map_err(|_| SyscallError::OperationFailed)?;
            Ok(0)
        }
        Syscall::Read => {
            // Would read from device
            Ok(0)
        }
        Syscall::Write => {
            // Would write to device
            Ok(0)
        }
        Syscall::Open => {
            // Would open device
            Ok(0)
        }
        Syscall::Close => {
            // Would close device
            Ok(0)
        }
        Syscall::Ioctl => {
            // Would perform device control
            Ok(0)
        }
        Syscall::Create => {
            // Would create file
            Ok(0)
        }
        Syscall::Delete => {
            // Would delete file
            Ok(0)
        }
        Syscall::List => {
            // Would list files
            Ok(0)
        }
        Syscall::TaskSpawn => {
            // Would spawn new task
            Ok(0)
        }
        Syscall::TaskExit => {
            // Would exit task with code
            Ok(0)
        }
        Syscall::AiReport => {
            // Would print AI report
            Ok(0)
        }
        Syscall::IpcCreate => {
            // Would create IPC channel
            Ok(0)
        }
        Syscall::IpcSend => {
            // Would send IPC message
            Ok(0)
        }
        Syscall::IpcRecv => {
            // Would receive IPC message
            Ok(0)
        }
        Syscall::IpcDestroy => {
            // Would destroy IPC channel
            Ok(0)
        }
        Syscall::GetPid => {
            let task_id = aeos_scheduler::scheduler().current_task()
                .ok_or(SyscallError::OperationFailed)?;
            Ok(task_id.0)
        }
        Syscall::WaitPid => {
            // Would wait for process
            Ok(0)
        }
    }
}

impl TryFrom<usize> for Syscall {
    type Error = ();

    fn try_from(value: usize) -> Result<Self, Self::Error> {
        match value {
            1 => Ok(Syscall::Print),
            2 => Ok(Syscall::Ticks),
            3 => Ok(Syscall::Yield),
            4 => Ok(Syscall::Exit),
            5 => Ok(Syscall::Read),
            6 => Ok(Syscall::Write),
            7 => Ok(Syscall::Open),
            8 => Ok(Syscall::Close),
            9 => Ok(Syscall::Ioctl),
            10 => Ok(Syscall::Create),
            11 => Ok(Syscall::Delete),
            12 => Ok(Syscall::List),
            13 => Ok(Syscall::TaskSpawn),
            14 => Ok(Syscall::TaskExit),
            15 => Ok(Syscall::AiReport),
            16 => Ok(Syscall::IpcCreate),
            17 => Ok(Syscall::IpcSend),
            18 => Ok(Syscall::IpcRecv),
            19 => Ok(Syscall::IpcDestroy),
            20 => Ok(Syscall::GetPid),
            21 => Ok(Syscall::WaitPid),
            _ => Err(()),
        }
    }
}
