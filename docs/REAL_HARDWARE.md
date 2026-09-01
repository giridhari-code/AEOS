# AEOS - Real Hardware Boot Guide

AEOS ko QEMU ke bina real machine pe chalane ke liye ye guide follow karo.

## Kya files hain

| File | Architecture | Use |
|------|--------------|-----|
| `boot/aeos-x86-boot.elf` | x86_64 | PC/laptop (GRUB multiboot) |
| `boot/aarch64/build/kernel.bin` | ARM64 | Raspberry Pi / ARM board |

## Option A: Linux PC pe GRUB se (recommended)

Requirements: koi bhi Linux with GRUB (Ubuntu/Fedora/Arch sab chalega).

```bash
# 1. Kernel copy karo
sudo cp boot/aeos-x86-boot.elf /boot/aeos-x86.elf

# 2. Custom menu entry banao
sudo nano /etc/grub.d/40_custom
```

Ye lines add karo:

```
menuentry "AEOS - Ajeeb Embodied AI OS" {
    insmod part_gpt
    multiboot /boot/aeos-x86.elf
    boot
}
```

```bash
# 3. GRUB update + reboot
sudo update-grub
sudo reboot
```

Reboot ke baad GRUB menu mein "AEOS" dikhega. Select karo - serial console
(COM1 / ttyS0) pe AEOS shell aa jayega.

## Option B: Bootable USB stick

Kisi Linux machine pe:

```bash
# USB mount karo (X apne device ke hisaab se - dhyan se!)
sudo mkfs.ext4 /dev/sdX1
sudo mount /dev/sdX1 /mnt/usb
sudo mkdir -p /mnt/usb/boot/grub
sudo cp aeos-x86-boot.elf /mnt/usb/boot/aeos-x86.elf

# GRUB install
sudo grub-install --target=i386-pc \
    --boot-directory=/mnt/usb/boot /dev/sdX

# Config
sudo tee /mnt/usb/boot/grub/grub.cfg << 'EOF'
set timeout=3
menuentry "AEOS" {
    multiboot /boot/aeos-x86.elf
    boot
}
EOF

sync && sudo umount /mnt/usb
```

USB se boot karo. Output dekhne ke liye:
- Desktop PC: COM port (serial cable) ya VGA screen
- Laptop: serial console

## Option C: Raspberry Pi (ARM64)

1. Raspberry Pi OS wali SD card lo
2. `config.txt` mein add karo: `kernel=kernel8.img`
3. AEOS kernel copy karo:
   ```bash
   cp boot/aarch64/build/kernel.bin /media/sd/kernel8.img
   ```
4. Pi boot karo - UART pins (GPIO 14/15) pe 115200 baud console milega

**Note**: Pi support experimental hai - QEMU virt aur Pi ka hardware
(PL011 UART mapping) thoda alag hai. GIC vs local interrupt controller
ka difference bhi hai. Basic bring-up ke liye boot.S mein Pi-specific
base addresses chahiye honge.

## Console access

AEOS output serial (COM1 / PL011) pe aata hai:

```bash
# Dusra machine se:
screen /dev/ttyUSB0 115200
# ya
minicom -D /dev/ttyS0 -b 115200
```

## Kya expect karna hai

Boot pe ye sequence dikhega:

```
=========================================
  AEOS - Ajeeb Embodied AI OS  [LDR v1.0]
=========================================
[LDR] AEOS-LDR stage-2 v1.0
[LDR] Kernel OK: XXXXX bytes @ 0x40080000 / 1MB
...
Shell ready. Type 'help'.
aeos>
```

Phir `help`, `ps`, `mem`, `ai`, `uptime` commands chala sakte ho.
Tasks background mein chalte rahenge (preemptive scheduling ke saath).
