//! Panic handler for the kernel.

use core::panic::PanicInfo;

/// Kernel panic handler.
///
/// # Safety
/// This function is called on panic and must not return.
#[panic_handler]
#[no_mangle]
pub extern "C" fn panic(_info: &PanicInfo) -> ! {
    // In a real kernel, we'd:
    // 1. Disable interrupts
    // 2. Print panic message to serial console
    // 3. Dump registers
    // 4. Enter infinite loop or reboot

    loop {
        core::hint::spin_loop();
    }
}
