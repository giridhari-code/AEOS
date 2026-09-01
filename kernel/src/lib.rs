//! # AEOS Kernel Core
//!
//! The main kernel module that ties all subsystems together.
//!
//! ## Architecture
//! - **Memory**: Frame allocator + virtual memory paging
//! - **Scheduler**: Preemptive round-robin with priority
//! - **IPC**: Message channels for inter-process communication
//! - **Security**: Capability-based access control
//!
//! ## Boot Sequence
//! 1. Architecture-specific boot (assembly)
//! 2. Memory initialization (frame allocator)
//! 3. Scheduler initialization
//! 4. IPC initialization
//! 5. Security initialization
//! 6. Start idle task
//! 7. Enable interrupts and context switch

#![deny(unsafe_code)]
#![no_std]
#![warn(missing_docs)]

extern crate alloc;

mod arch;
mod panic;
mod syscall;

pub use arch::*;
pub use syscall::{Syscall, SyscallError, SyscallResult};

/// Kernel version.
pub const VERSION: &str = env!("CARGO_PKG_VERSION");

/// Kernel name.
pub const NAME: &str = "AEOS";

/// Global kernel state.
pub struct Kernel {
    /// Whether kernel is initialized.
    initialized: bool,
    /// Boot time tick.
    boot_tick: u64,
}

impl Kernel {
    /// Create a new kernel instance.
    pub const fn new() -> Self {
        Self {
            initialized: false,
            boot_tick: 0,
        }
    }

    /// Initialize the kernel.
    pub fn init(&mut self) -> Result<(), KernelError> {
        if self.initialized {
            return Ok(());
        }

        // Initialize subsystems in order
        // Reserve first 18MB for kernel image, boot structures, and stack
        let reserved_end = 0x4000_0000 + 18 * 1024 * 1024;
        aeos_memory::init(0x4000_0000, 128 * 1024 * 1024, reserved_end)
            .map_err(|_| KernelError::MemoryInitFailed)?;

        {
            let mut sched = aeos_scheduler::scheduler();
            let s = sched.get_or_insert_with(aeos_scheduler::Scheduler::new);
            s.init().map_err(|_| KernelError::SchedulerInitFailed)?;
        }

        {
            let mut ipc = aeos_ipc::ipc_state();
            let i = ipc.get_or_insert_with(aeos_ipc::IpcState::new);
            i.init().map_err(|_| KernelError::IpcInitFailed)?;
        }

        {
            let mut sec = aeos_security::security_manager();
            let s = sec.get_or_insert_with(aeos_security::SecurityManager::new);
            s.init().map_err(|_| KernelError::SecurityInitFailed)?;
        }

        self.initialized = true;
        Ok(())
    }

    /// Main kernel loop.
    pub fn run(&mut self) -> ! {
        loop {
            // Yield to scheduler
            core::hint::spin_loop();
        }
    }

    /// Handle a timer tick.
    pub fn timer_tick(&mut self) {
        self.boot_tick += 1;

        // Tick scheduler
        let mut sched = aeos_scheduler::scheduler();
        if let Some(s) = sched.as_mut() {
            if let Some(next_task) = s.tick() {
                // Context switch to next task (architecture-specific)
                drop(sched);
                arch::context_switch(next_task);
            }
        }
    }

    /// Get boot tick count.
    pub fn boot_tick(&self) -> u64 {
        self.boot_tick
    }
}

/// Kernel error types.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum KernelError {
    /// Memory initialization failed.
    MemoryInitFailed,
    /// Scheduler initialization failed.
    SchedulerInitFailed,
    /// IPC initialization failed.
    IpcInitFailed,
    /// Security initialization failed.
    SecurityInitFailed,
    /// Architecture init failed.
    ArchInitFailed,
}

/// Global kernel instance.
static KERNEL: spin::Mutex<Option<Kernel>> = spin::Mutex::new(None);

/// Get a reference to the global kernel.
pub fn kernel() -> spin::MutexGuard<'static, Option<Kernel>> {
    KERNEL.lock()
}

/// Entry point for the kernel.
#[cfg(not(test))]
#[allow(unsafe_code)]
#[unsafe(no_mangle)]
pub extern "C" fn _start() -> ! {
    // Initialize kernel
    let mut guard = kernel();
    let k = guard.get_or_insert_with(Kernel::new);
    if let Err(e) = k.init() {
        // Handle initialization error
        drop(guard);
        loop {
            core::hint::spin_loop();
        }
    }

    // Run kernel
    k.run()
}
