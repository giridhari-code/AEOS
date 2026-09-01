#!/usr/bin/env python3
"""
AEOS System Monitor - Live Dashboard
Real-time system statistics for AEOS kernel

Usage:
    python3 tools/monitor.py                    # Auto-detect kernel
    python3 tools/monitor.py --port /dev/ttyUSB0  # Specify port
    python3 tools/monitor.py --mock             # Use mock data
"""

import sys
import time
import argparse
from dataclasses import dataclass
from typing import Optional


@dataclass
class SystemStats:
    """System statistics from AEOS kernel"""
    uptime_ticks: int = 0
    uptime_seconds: float = 0.0
    cpu_hz: int = 100
    heap_used: int = 0
    heap_free: int = 0
    heap_total: int = 0
    free_pages: int = 0
    total_pages: int = 0
    ai_inferences: int = 0
    ai_errors: int = 0
    ai_action: str = "SUSTAIN"
    ai_hz: int = 100
    cmt_level: int = 0
    task_count: int = 0
    process_count: int = 0
    ipc_queues: int = 0


class MockKernel:
    """Mock kernel for testing without real hardware"""

    def __init__(self):
        self.tick = 0
        self.stats = SystemStats()

    def read_stats(self) -> SystemStats:
        self.tick += 1
        import random

        self.stats.uptime_ticks = self.tick
        self.stats.uptime_seconds = self.tick / 100.0
        self.stats.cpu_hz = 100 + (self.tick % 50)
        self.stats.heap_used = 1024 * (100 + self.tick % 50)
        self.stats.heap_free = 1024 * (500 - self.tick % 50)
        self.stats.heap_total = 1024 * 600
        self.stats.free_pages = 1000 - (self.tick % 200)
        self.stats.total_pages = 1000
        self.stats.ai_inferences = self.tick // 10
        self.stats.ai_errors = self.tick // 100
        self.stats.ai_action = random.choice(["SUSTAIN", "BOOST", "REST"])
        self.stats.ai_hz = self.stats.cpu_hz
        self.stats.cmt_level = random.randint(0, 3)
        self.stats.task_count = 5 + (self.tick % 3)
        self.stats.process_count = 2 + (self.tick % 2)
        self.stats.ipc_queues = 3

        return self.stats


class SerialKernel:
    """Real kernel connection via serial port"""

    def __init__(self, port: str, baud: int = 115200):
        self.port = port
        self.baud = baud
        self.serial = None

    def connect(self):
        try:
            import serial
            self.serial = serial.Serial(self.port, self.baud, timeout=1)
            return True
        except ImportError:
            print("Error: pyserial not installed. Run: pip install pyserial")
            return False
        except Exception as e:
            print(f"Error connecting to {self.port}: {e}")
            return False

    def read_stats(self) -> Optional[SystemStats]:
        if not self.serial:
            return None

        try:
            # Send stats request command
            self.serial.write(b"mem\n")
            time.sleep(0.1)
            mem_line = self.serial.readline().decode().strip()

            self.serial.write(b"ai\n")
            time.sleep(0.1)
            ai_line = self.serial.readline().decode().strip()

            self.serial.write(b"uptime\n")
            time.sleep(0.1)
            uptime_line = self.serial.readline().decode().strip()

            stats = SystemStats()

            # Parse memory info
            if "pmm:" in mem_line:
                parts = mem_line.split()
                for i, p in enumerate(parts):
                    if p == "free":
                        stats.free_pages = int(parts[i-1])
                    elif p == "heap":
                        if "used:" in parts[i+1:]:
                            stats.heap_used = int(parts[i+2])

            # Parse AI info
            if "inferences:" in ai_line:
                parts = ai_line.split()
                for i, p in enumerate(parts):
                    if p == "inferences:":
                        stats.ai_inferences = int(parts[i+1])
                    elif p == "errors:":
                        stats.ai_errors = int(parts[i+1])
                    elif p == "action:":
                        stats.ai_action = parts[i+1]
                    elif p == "policy":
                        stats.ai_hz = int(parts[i+1])

            # Parse uptime
            if "uptime:" in uptime_line:
                parts = uptime_line.split()
                for i, p in enumerate(parts):
                    if p == "ticks":
                        stats.uptime_ticks = int(parts[i-1])
                        stats.uptime_seconds = stats.uptime_ticks / 100.0

            return stats

        except Exception as e:
            print(f"Error reading stats: {e}")
            return None


class SystemMonitor:
    """Terminal-based system monitor dashboard"""

    # ANSI colors
    RESET = "\033[0m"
    BOLD = "\033[1m"
    DIM = "\033[2m"
    RED = "\033[31m"
    GREEN = "\033[32m"
    YELLOW = "\033[33m"
    BLUE = "\033[34m"
    MAGENTA = "\033[35m"
    CYAN = "\033[36m"
    WHITE = "\033[37m"
    BG_BLUE = "\033[44m"

    def __init__(self, kernel):
        self.kernel = kernel
        self.running = True
        self.history = []

    def clear_screen(self):
        print("\033[2J\033[H", end="")

    def draw_bar(self, value: int, max_value: int, width: int = 30, color: str = "") -> str:
        if max_value == 0:
            filled = 0
        else:
            filled = int((value / max_value) * width)
        filled = min(filled, width)

        bar = "█" * filled + "░" * (width - filled)
        if color:
            return f"{color}{bar}{self.RESET}"
        return bar

    def draw_header(self):
        print(f"{self.BG_BLUE}{self.WHITE}{self.BOLD}")
        print("╔══════════════════════════════════════════════════════════════════╗")
        print("║                    AEOS SYSTEM MONITOR                         ║")
        print("║              Ajeeb Embodied AI Operating System                 ║")
        print("╚══════════════════════════════════════════════════════════════════╝")
        print(self.RESET)

    def draw_stats(self, stats: SystemStats):
        # Uptime
        hours = int(stats.uptime_seconds // 3600)
        minutes = int((stats.uptime_seconds % 3600) // 60)
        seconds = int(stats.uptime_seconds % 60)

        print(f"\n{self.BOLD}{self.CYAN}┌─ SYSTEM ─────────────────────────────────────────────────────┐{self.RESET}")
        print(f"│  Uptime:  {self.GREEN}{hours:02d}:{minutes:02d}:{seconds:02d}{self.RESET} ({stats.uptime_ticks} ticks)")
        print(f"│  CPU:     {self.YELLOW}{stats.cpu_hz} Hz{self.RESET}")
        print(f"{self.CYAN}└──────────────────────────────────────────────────────────────┘{self.RESET}")

        # Memory
        print(f"\n{self.BOLD}{self.BLUE}┌─ MEMORY ─────────────────────────────────────────────────────┐{self.RESET}")
        heap_pct = (stats.heap_used / stats.heap_total * 100) if stats.heap_total > 0 else 0
        page_pct = ((stats.total_pages - stats.free_pages) / stats.total_pages * 100) if stats.total_pages > 0 else 0

        heap_color = self.GREEN if heap_pct < 70 else (self.YELLOW if heap_pct < 90 else self.RED)
        page_color = self.GREEN if page_pct < 70 else (self.YELLOW if page_pct < 90 else self.RED)

        print(f"│  Heap:    {heap_color}{self.draw_bar(stats.heap_used, stats.heap_total, 25)}{self.RESET} {heap_pct:.1f}%")
        print(f"│           {stats.heap_used:,} / {stats.heap_total:,} bytes")
        print(f"│  Pages:   {page_color}{self.draw_bar(stats.total_pages - stats.free_pages, stats.total_pages, 25)}{self.RESET} {page_pct:.1f}%")
        print(f"│           {stats.free_pages} / {stats.total_pages} free")
        print(f"{self.BLUE}└──────────────────────────────────────────────────────────────┘{self.RESET}")

        # AI Engine
        print(f"\n{self.BOLD}{self.MAGENTA}┌─ AI ENGINE ──────────────────────────────────────────────────┐{self.RESET}")
        action_color = {
            "SUSTAIN": self.GREEN,
            "BOOST": self.YELLOW,
            "REST": self.CYAN
        }.get(stats.ai_action, self.WHITE)

        print(f"│  Inferences: {stats.ai_inferences}")
        print(f"│  Errors:     {stats.ai_errors}")
        print(f"│  Action:     {action_color}{self.BOLD}{stats.ai_action}{self.RESET}")
        print(f"│  Policy:     {stats.ai_hz} Hz")
        print(f"│  CMT Level:  {stats.cmt_level} ({['unconscious', 'minimal', 'aware', 'focused'][stats.cmt_level]})")
        print(f"{self.MAGENTA}└──────────────────────────────────────────────────────────────┘{self.RESET}")

        # Tasks
        print(f"\n{self.BOLD}{self.GREEN}┌─ TASKS ──────────────────────────────────────────────────────┐{self.RESET}")
        print(f"│  Scheduler tasks: {stats.task_count}")
        print(f"│  Processes:       {stats.process_count}")
        print(f"│  IPC queues:      {stats.ipc_queues}")
        print(f"{self.GREEN}└──────────────────────────────────────────────────────────────┘{self.RESET}")

        # History sparkline
        if len(self.history) > 1:
            print(f"\n{self.BOLD}{self.YELLOW}┌─ HISTORY (last {len(self.history)} inferences) ─────────────────────────┐{self.RESET}")
            actions = [h.get("action", "SUSTAIN") for h in self.history[-40:]]
            sparkline = ""
            for a in actions:
                if a == "BOOST":
                    sparkline += f"{self.YELLOW}▲{self.RESET}"
                elif a == "REST":
                    sparkline += f"{self.CYAN}▼{self.RESET}"
                else:
                    sparkline += f"{self.GREEN}─{self.RESET}"
            print(f"│  {sparkline}")
            print(f"{self.YELLOW}└──────────────────────────────────────────────────────────────┘{self.RESET}")

    def draw_footer(self):
        print(f"\n{self.DIM}Press Ctrl+C to exit | Refresh rate: 1 Hz{self.RESET}")

    def update(self, stats: SystemStats):
        self.clear_screen()
        self.draw_header()
        self.draw_stats(stats)
        self.draw_footer()

        # Track history
        self.history.append({
            "tick": stats.uptime_ticks,
            "action": stats.ai_action,
            "hz": stats.ai_hz
        })
        if len(self.history) > 100:
            self.history = self.history[-100:]

    def run(self, refresh_rate: float = 1.0):
        print(f"{self.BOLD}Starting AEOS System Monitor...{self.RESET}")
        time.sleep(1)

        try:
            while self.running:
                stats = self.kernel.read_stats()
                if stats:
                    self.update(stats)
                time.sleep(refresh_rate)
        except KeyboardInterrupt:
            print(f"\n{self.YELLOW}Monitor stopped.{self.RESET}")


def main():
    parser = argparse.ArgumentParser(description="AEOS System Monitor")
    parser.add_argument("--port", help="Serial port (e.g., /dev/ttyUSB0)")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate")
    parser.add_argument("--mock", action="store_true", help="Use mock data")
    parser.add_argument("--refresh", type=float, default=1.0, help="Refresh rate (seconds)")
    args = parser.parse_args()

    if args.mock or not args.port:
        print("Using mock kernel data...")
        kernel = MockKernel()
    else:
        print(f"Connecting to {args.port} at {args.baud} baud...")
        kernel = SerialKernel(args.port, args.baud)
        if not kernel.connect():
            sys.exit(1)

    monitor = SystemMonitor(kernel)
    monitor.run(args.refresh)


if __name__ == "__main__":
    main()
