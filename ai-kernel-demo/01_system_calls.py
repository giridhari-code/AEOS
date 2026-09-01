#!/usr/bin/env python3
"""
AI Framework System Calls Demo
Ye code dikhata hai kaise AI kernel ke through hardware access karta hai
"""

import os
import sys
import resource
import ctypes

def check_gpu_available():
    """GPU available hai ya nahi check karta hai - kernel se puchta hai"""
    print("=" * 60)
    print("1. GPU CHECK (System Call: access to /dev/nvidia*)")
    print("=" * 60)

    nvidia_devices = [
        "/dev/nvidia0",
        "/dev/nvidiactl",
        "/dev/nvidia-uvm"
    ]

    for device in nvidia_devices:
        if os.path.exists(device):
            print(f"  [OK] {device} exists - GPU accessible via kernel")
        else:
            print(f"  [--] {device} not found - No NVIDIA GPU")

def check_memory_info():
    """System memory check - kernel ke through"""
    print("\n" + "=" * 60)
    print("2. MEMORY INFO (System Call: /proc/meminfo)")
    print("=" * 60)

    with open('/proc/meminfo', 'r') as f:
        lines = f.readlines()

    for line in lines[:5]:
        print(f"  {line.strip()}")

    usage = resource.getrusage(resource.RUSAGE_SELF)
    print(f"\n  Current Process Memory: {usage.ru_maxrss} KB")

def show_process_info():
    """Process info - kernel se milta hai"""
    print("\n" + "=" * 60)
    print("3. PROCESS INFO (System Call: getpid, getuid)")
    print("=" * 60)

    pid = os.getpid()
    ppid = os.getppid()
    uid = os.getuid()

    print(f"  PID: {pid} (Process ID)")
    print(f"  PPID: {ppid} (Parent Process ID)")
    print(f"  UID: {uid} (User ID)")
    print(f"  CWD: {os.getcwd()}")

def simulate_ai_memory_allocation():
    """AI memory allocation simulate karta hai"""
    print("\n" + "=" * 60)
    print("4. AI MEMORY ALLOCATION (brk/mmap syscalls)")
    print("=" * 60)

    # Simulate large tensor allocation
    large_tensor = []
    for i in range(1000000):
        large_tensor.append(float(i))

    print(f"  Allocated tensor of size: {sys.getsizeof(large_tensor)} bytes")
    print(f"  Number of elements: {len(large_tensor)}")

    # Cleanup
    del large_tensor

def show_cpu_info():
    """CPU info - kernel provide karta hai"""
    print("\n" + "=" * 60)
    print("5. CPU INFO (System Call: sysconf)")
    print("=" * 60)

    cpu_count = os.cpu_count()
    print(f"  CPU Cores: {cpu_count}")
    print(f"  Platform: {sys.platform}")
    print(f"  Architecture: {sys.maxsize > 2**32 and '64-bit' or '32-bit'}")

if __name__ == "__main__":
    print("AI-KERNEL INTERACTION DEMO")
    print("Ye code dikhata hai kaise AI framework kernel ke saath communicate karta hai\n")

    check_gpu_available()
    check_memory_info()
    show_process_info()
    simulate_ai_memory_allocation()
    show_cpu_info()

    print("\n" + "=" * 60)
    print("CONCLUSION")
    print("=" * 60)
    print("AI framework ne ye sab kernel ke through kiya:")
    print("  - /proc/ filesystem access")
    print("  - Device drivers (/dev/nvidia*)")
    print("  - Memory management (brk/mmap)")
    print("  - Process management (getpid)")
    print("  - System configuration (sysconf)")
