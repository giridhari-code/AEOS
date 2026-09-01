# AEOS x86_64 build
BUILD = build
CC      = x86_64-linux-gnu-gcc
AS      = x86_64-linux-gnu-gcc
LD      = x86_64-linux-gnu-ld
OBJCOPY = x86_64-linux-gnu-objcopy

CFLAGS  = -Wall -Wextra -Werror -O2 -ffreestanding -nostdlib \
          -mno-red-zone -mcmodel=kernel -mno-mmx -mno-sse -mno-sse2 \
          -fno-pie -fno-stack-protector -DAEOS_TSS_SWAP -I. -I../../kernel
ASMFLAGS = $(CFLAGS)

KSRC = kernel_x86.c uart.c vgacon.c irq.c mm.c stubs.c
KSRC += ../../kernel/ai_core.c ../../kernel/ai_api.c ../../kernel/ai_learn.c
KSRC += ../../kernel/ai_debug.c ../../kernel/cmt_core.c ../../kernel/cmt_api.c
KSRC += ../../kernel/sched.c ../../kernel/sync.c ../../kernel/shell.c
KSRC += ../../kernel/ramfs.c ../../kernel/ipc.c ../../kernel/device.c ../../kernel/tui.c

AOBJ = $(BUILD)/boot.o $(BUILD)/switch.o $(BUILD)/irqs.o
COBJ = $(patsubst %.c,$(BUILD)/%.o,$(notdir $(KSRC)))

all: $(BUILD)/stage0.elf
	@echo "Build complete! Run: make run"

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/boot.o: boot.asm | $(BUILD)
	$(AS) $(ASMFLAGS) -x assembler -c $< -o $@

$(BUILD)/switch.o: switch.S | $(BUILD)
	$(AS) $(ASMFLAGS) -c $< -o $@

$(BUILD)/irqs.o: irqs.S | $(BUILD)
	$(AS) $(ASMFLAGS) -c $< -o $@

$(BUILD)/kernel_x86.o: kernel_x86.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/uart.o: uart.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/irq.o: irq.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/mm.o: mm.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/vgacon.o: vgacon.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/stubs.o: stubs.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

VPATH = ../../kernel
$(BUILD)/%.o: ../../kernel/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/aeos-x86.elf: $(AOBJ) $(COBJ) linker.ld
	$(LD) -m elf_x86_64 -T linker.ld -nostdlib --gc-sections \
	    $(AOBJ) $(COBJ) -o $@

# 32-bit multiboot stage0 embedding the 64-bit kernel
$(BUILD)/aeos-x86.bin: $(BUILD)/aeos-x86.elf
	$(OBJCOPY) -O binary $< $@

$(BUILD)/stage0.o: stage0.S $(BUILD)/aeos-x86.bin | $(BUILD)
	x86_64-linux-gnu-as --32 -g $< -o $@

$(BUILD)/stage0.elf: $(BUILD)/stage0.o stage0.ld
	$(LD) -m elf_i386 -T stage0.ld -nostdlib $< -o $@
	cp $@ ../aeos-x86-boot.elf

clean:
	rm -rf $(BUILD)

.PHONY: all clean
