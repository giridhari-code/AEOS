/*
 * AEOS-LDR - Stage 2 Boot Loader (public interface)
 */

#ifndef AEOS_LOADER_H
#define AEOS_LOADER_H

#include <stdint.h>

/*
 * Called from boot.S with the firmware handoff register x0
 * (DTB pointer under the ARM Linux boot protocol). Returns the
 * bootinfo_t pointer in x0 for boot.S to pass into kernel_main.
 * Returns 0 if validation fails (boot.S parks).
 */
uint64_t loader_main(uint64_t dtb_ptr_raw);

#endif /* AEOS_LOADER_H */
