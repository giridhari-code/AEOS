# ============================================================
# AEOS-BIOS Makefile
# Firmware image for QEMU virt (-bios), boots AEOS from disk
# ============================================================

CC      = aarch64-linux-gnu-gcc
AS      = aarch64-linux-gnu-as
LD      = aarch64-linux-gnu-ld
OBJCOPY = aarch64-linux-gnu-objcopy

ASFLAGS = -g
CFLAGS  = -Wall -Wextra -Werror -O2 -std=c11 -ffreestanding -nostdlib \
          -mgeneral-regs-only -DAEOS_HAS_VMM -mstrict-align \
          -I. -I../aarch64 -I../../kernel -g

# Reuse the kernel's drivers/subsystems directly.
SUBSYSTEM_DIR = ../../kernel
ARCH_DIR      = ../aarch64
VPATH = $(ARCH_DIR) $(SUBSYSTEM_DIR)

BUILD_DIR = obj
BIOS_ELF  = $(BUILD_DIR)/aeos-bios.elf
BIOS_BIN  = $(BUILD_DIR)/aeos-bios.bin

ASM_SRC = start.S
C_SRC   = bios_main.c uart.c utils.c virtio.c pmm.c afs.c

ASM_OBJ = $(addprefix $(BUILD_DIR)/, $(ASM_SRC:.S=.o))
C_OBJ   = $(addprefix $(BUILD_DIR)/, $(C_SRC:.c=.o))

.PHONY: all clean size

all: $(BIOS_BIN)
	@echo "AEOS-BIOS built: $(BIOS_BIN)"

$(BIOS_ELF): $(ASM_OBJ) $(C_OBJ) linker.ld
	@mkdir -p $(BUILD_DIR)
	$(LD) -T linker.ld -nostdlib --gc-sections \
	      -z max-page-size=4096 $(ASM_OBJ) $(C_OBJ) -o $@

$(BIOS_BIN): $(BIOS_ELF)
	$(OBJCOPY) -O binary $< $@
	@ls -l $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(BUILD_DIR)
	$(AS) $(ASFLAGS) $< -o $@

size: $(BIOS_ELF)
	aarch64-linux-gnu-size $<

clean:
	rm -rf $(BUILD_DIR)
