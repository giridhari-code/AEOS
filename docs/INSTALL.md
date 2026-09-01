# Running AEOS on real hardware & hypervisors

AEOS ka boot stack industry-standard technologies use karta hai —
koi QEMU lock-in nahi hai:

| Component | Standard | Real hardware examples |
|-----------|----------|------------------------|
| Serial console | PL011 / ns16550 (DTB-discovered) | Raspberry Pi 4, ARM SoCs, any PC COM port (x86) |
| Interrupts | GICv2 (ARM) / PIC+PIT (x86) | Most Cortex-A SoCs, every PC |
| Storage | virtio-blk (industry standard — AWS/GCP clouds ise hi use karte hain) | Cloud VMs, virtio-capable boards |
| Power/boot firmware interface | PSCI (ARM spec) | Every modern ARM board firmware |
| Boot protocols | Multiboot v1, El Torito, MBR/GPT | GRUB-compatible loaders, har PC/laptop |

## x86_64 PC / laptop (legacy BIOS)

```bash
make build-gos          # ya build-iso
sudo dd if=build/ajeeb.gos of=/dev/sdX bs=4M status=progress   # USB stick
```
USB se boot karo (legacy/CSM mode). `aeos.iso` ko CD/DVD ya
Ventoy ke through bhi boot kar sakte ho.

## Any hypervisor (VirtualBox / VMware / Proxmox / libvirt)

### VirtualBox (x86 machine pe)

AEOS ka output **COM1 serial** pe aata hai — VM banate waqt serial
enable karna zaroori hai:

```bash
# 1) CD se boot (sabse aasan)
VBoxManage createvm --name AEOS --register
VBoxManage modifyvm AEOS --memory 256     --uart1 0x3F8 4 --uartmode1 file /tmp/aeos-com.log     --audio none --usb off
VBoxManage storagectl AEOS --name IDE --add ide
VBoxManage storageattach AEOS --storagectl IDE --port 1 --device 0     --type dvddrive --medium build/aeos.iso
VBoxManage startvm AEOS --type headless

# dusre terminal se live dekho:
tail -f /tmp/aeos-com.log

# 2) Disk image (.gos) se boot — raw ko VDI bana ke attach karo
VBoxManage convertfromraw build/ajeeb.gos aeos.vdi
VBoxManage storagectl AEOS --name SATA --add sata
VBoxManage storageattach AEOS --storagectl SATA --port 0 --device 0 \
    --type hdd --medium aeos.vdi
```

GUI mode mein bhi chalega — bas serial settings waise hi rakho.
(VGA text driver vgacon code mein ready hai — `-DAEOS_VGA_MIRROR`
flag se enable hota hai. Ek known preempt-race ki wajah se abhi
default off hai; serial output guaranteed hai.)

### VMware / Proxmox / libvirt

Same concept: `ajeeb.gos` = raw disk, `aeos.iso` = CDROM, COM1
serial enable. Koi QEMU-specific cheez image mein nahi hai.

## aarch64 boards

Board firmware jo PSCI + DTB provide karta hai aur PL011 UART
discoverable ho — `ajeeb.gos` ko disk ke roop mein attach karo
(AEOS-BIOS image ke through) ya apne loader se kernel.bin load karo.
UART base DTB se automatically milta hai — hardcoded board address
nahi hai.

## Ek command launcher: `./ajeeb`

Terminal se VM boot karne ka sabse aasan tareeka:

```bash
./ajeeb                # ajeeb.gos ko VM mein boot karo (KVM auto-detect)
./ajeeb --log          # serial log print karke exit
./ajeeb --iso          # CDROM image se boot
sudo ./ajeeb --flash /dev/sdX   # REAL HARDWARE (USB) pe likho
./ajeeb --smp 2        # 2 CPUs ke saath
```

## Development/testing

QEMU sirf ek **test harness** hai — jaise Linux kernel developers
bhi qemu pe test karte hain. OS itself kisi emulator se dependent
nahi hai:

```bash
make run        # quick dev loop (QEMU)
make run-log    # terminal-agnostic serial log
make build-gos  # distribution image
```

## Known limits (roadmap)

- x86: VGA/text framebuffer output abhi serial-first hai
- ARM: RPi mailbox-GPU, non-PL011 UARTs (8250 MMIO variants) baaki hain
- SMP: AP workers live; per-core scheduler scheduling next phase
