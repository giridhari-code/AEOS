//! Architecture-specific code.

use aeos_scheduler::TaskId;

/// Initialize architecture-specific state.
pub fn init() -> Result<(), super::KernelError> {
    // Architecture-specific initialization
    Ok(())
}

/// Context switch to a new task.
pub fn context_switch(task_id: TaskId) {
    // Architecture-specific context switch
    // This would load the task's registers and stack pointer
    let _ = task_id;
}

/// Enable interrupts.
pub fn enable_interrupts() {
    // Architecture-specific interrupt enable
}

/// Disable interrupts.
pub fn disable_interrupts() {
    // Architecture-specific interrupt disable
}

/// Halt the CPU.
pub fn halt() -> ! {
    loop {
        #[cfg(target_arch = "x86_64")]
        unsafe {
            core::arch::asm!("hlt");
        }
        #[cfg(target_arch = "aarch64")]
        unsafe {
            core::arch::asm!("wfi");
        }
    }
}
