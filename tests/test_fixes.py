#!/usr/bin/env python3
"""
AEOS Unit Tests - Verification of Bug Fixes
Tests to ensure the fixes work correctly

Run: python3 tests/test_fixes.py
"""

import sys
import os

# Add SDK to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'sdk', 'py', 'src'))


class TestResults:
    """Track test results"""
    def __init__(self):
        self.passed = 0
        self.failed = 0
        self.errors = []

    def check(self, name: str, condition: bool, msg: str = ""):
        if condition:
            self.passed += 1
            print(f"  ✓ {name}")
        else:
            self.failed += 1
            self.errors.append(f"{name}: {msg}")
            print(f"  ✗ {name} - {msg}")

    def summary(self):
        total = self.passed + self.failed
        print(f"\n{'='*60}")
        print(f"Results: {self.passed}/{total} passed, {self.failed} failed")
        if self.errors:
            print(f"\nFailed tests:")
            for e in self.errors:
                print(f"  - {e}")
        return self.failed == 0


def test_fd_table_per_process():
    """Test 1: File descriptor table is per-process"""
    print("\n[Test 1] Per-process FD table")
    results = TestResults()

    # Read the proc.h file to verify FD_MAX is defined
    proc_h_path = os.path.join(os.path.dirname(__file__), '..', 'kernel', 'proc.h')
    if os.path.exists(proc_h_path):
        with open(proc_h_path, 'r') as f:
            content = f.read()
            results.check("PROC_FD_MAX defined",
                         "PROC_FD_MAX" in content,
                         "PROC_FD_MAX not found in proc.h")
            results.check("proc_fd_alloc declared",
                         "proc_fd_alloc" in content,
                         "proc_fd_alloc not declared")
            results.check("proc_fd_get declared",
                         "proc_fd_get" in content,
                         "proc_fd_get not declared")
            results.check("proc_fd_close declared",
                         "proc_fd_close" in content,
                         "proc_fd_close not declared")
    else:
        results.check("proc.h exists", False, "File not found")

    return results


def test_ai_perceive_before_decide():
    """Test 2: ai_perceive() called before ai_decide()"""
    print("\n[Test 2] AI perceive before decide")
    results = TestResults()

    ai_api_path = os.path.join(os.path.dirname(__file__), '..', 'kernel', 'ai_api.c')
    if os.path.exists(ai_api_path):
        with open(ai_api_path, 'r') as f:
            content = f.read()
            # Check that ai_perceive is called before ai_decide
            perceive_pos = content.find('ai_perceive(x)')
            decide_pos = content.find('ai_decide(x)')

            results.check("ai_perceive exists",
                         perceive_pos != -1,
                         "ai_perceive(x) not found")
            results.check("ai_decide exists",
                         decide_pos != -1,
                         "ai_decide(x) not found")
            results.check("ai_perceive before ai_decide",
                         perceive_pos < decide_pos if perceive_pos != -1 and decide_pos != -1 else False,
                         "ai_perceive should be called before ai_decide")
    else:
        results.check("ai_api.c exists", False, "File not found")

    return results


def test_cmt_bounds_check():
    """Test 3: CMT level bounds check"""
    print("\n[Test 3] CMT level bounds check")
    results = TestResults()

    cmt_api_path = os.path.join(os.path.dirname(__file__), '..', 'kernel', 'cmt_api.c')
    if os.path.exists(cmt_api_path):
        with open(cmt_api_path, 'r') as f:
            content = f.read()
            results.check("Bounds check exists",
                         "if (level >= 4)" in content or "if(level >= 4)" in content,
                         "Bounds check for level >= 4 not found")
            results.check("Level clamped to 3",
                         "level = 3" in content,
                         "Level should be clamped to 3")
    else:
        results.check("cmt_api.c exists", False, "File not found")

    return results


def test_pit_reprogramming():
    """Test 4: PIT timer reprogramming"""
    print("\n[Test 4] PIT timer reprogramming")
    results = TestResults()

    irq_path = os.path.join(os.path.dirname(__file__), '..', 'boot', 'x86_64', 'irq.c')
    if os.path.exists(irq_path):
        with open(irq_path, 'r') as f:
            content = f.read()
            # Check that timer_set_hz reprograms PIT
            results.check("outb in timer_set_hz",
                         "outb(PIT_CMD" in content and "outb(PIT_CH0" in content,
                         "PIT reprogramming not found in timer_set_hz")
    else:
        results.check("irq.c exists", False, "File not found")

    return results


def test_ipc_memcpy():
    """Test 5: IPC uses memcpy instead of byte-by-byte"""
    print("\n[Test 5] IPC memcpy optimization")
    results = TestResults()

    ipc_path = os.path.join(os.path.dirname(__file__), '..', 'kernel', 'ipc.c')
    if os.path.exists(ipc_path):
        with open(ipc_path, 'r') as f:
            content = f.read()
            results.check("memcpy in ipc_send",
                         "memcpy(msg->data, src" in content,
                         "memcpy not found in ipc_send")
            results.check("memcpy in ipc_recv",
                         "memcpy(dst, msg->data" in content,
                         "memcpy not found in ipc_recv")
            results.check("No byte-by-byte copy",
                         "i / 4" not in content and "i%4" not in content,
                         "Byte-by-byte copy still present")
    else:
        results.check("ipc.c exists", False, "File not found")

    return results


def test_shell_integrity_check():
    """Test 6: Shell self-update integrity check"""
    print("\n[Test 6] Shell integrity check")
    results = TestResults()

    shell_path = os.path.join(os.path.dirname(__file__), '..', 'kernel', 'shell.c')
    if os.path.exists(shell_path):
        with open(shell_path, 'r') as f:
            content = f.read()
            results.check("Checksum function exists",
                         "simple_checksum" in content,
                         "simple_checksum function not found")
            results.check("Checksum before write",
                         "checksum_before" in content,
                         "checksum_before not found")
            results.check("Checksum after write",
                         "checksum_after" in content,
                         "checksum_after not found")
            results.check("Integrity check message",
                         "integrity check" in content.lower(),
                         "Integrity check message not found")
    else:
        results.check("shell.c exists", False, "File not found")

    return results


def test_afs_free_extent():
    """Test 7: AFS free extent tracking"""
    print("\n[Test 7] AFS free extent tracking")
    results = TestResults()

    afs_path = os.path.join(os.path.dirname(__file__), '..', 'kernel', 'afs.c')
    if os.path.exists(afs_path):
        with open(afs_path, 'r') as f:
            content = f.read()
            results.check("Free extent struct",
                         "afs_free_extent_t" in content,
                         "Free extent structure not found")
            results.check("alloc_from_free_list function",
                         "alloc_from_free_list" in content,
                         "alloc_from_free_list not found")
            results.check("add_to_free_list function",
                         "add_to_free_list" in content,
                         "add_to_free_list not found")
            results.check("Uses free list in write",
                         "alloc_from_free_list(nlbas)" in content,
                         "Free list not used in afs_write")
    else:
        results.check("afs.c exists", False, "File not found")

    return results


def test_ramfs_error_code():
    """Test 8: RAMFS new error code"""
    print("\n[Test 8] RAMFS error code")
    results = TestResults()

    ramfs_h_path = os.path.join(os.path.dirname(__file__), '..', 'kernel', 'ramfs.h')
    ramfs_c_path = os.path.join(os.path.dirname(__file__), '..', 'kernel', 'ramfs.c')

    if os.path.exists(ramfs_h_path):
        with open(ramfs_h_path, 'r') as f:
            content = f.read()
            results.check("RAMFS_ERR_INVAL defined",
                         "RAMFS_ERR_INVAL" in content,
                         "RAMFS_ERR_INVAL not defined")
    else:
        results.check("ramfs.h exists", False, "File not found")

    if os.path.exists(ramfs_c_path):
        with open(ramfs_c_path, 'r') as f:
            content = f.read()
            results.check("Uses RAMFS_ERR_INVAL",
                         "RAMFS_ERR_INVAL" in content,
                         "RAMFS_ERR_INVAL not used")
    else:
        results.check("ramfs.c exists", False, "File not found")

    return results


def test_version_sync():
    """Test 9: Version synchronization"""
    print("\n[Test 9] Version synchronization")
    results = TestResults()

    pyproject_path = os.path.join(os.path.dirname(__file__), '..', 'pyproject.toml')
    init_path = os.path.join(os.path.dirname(__file__), '..', 'sdk', 'py', 'src', 'aeos_sdk', '__init__.py')

    pyproject_version = None
    init_version = None

    if os.path.exists(pyproject_path):
        with open(pyproject_path, 'r') as f:
            for line in f:
                if line.startswith('version'):
                    pyproject_version = line.split('=')[1].strip().strip('"')
                    break

    if os.path.exists(init_path):
        with open(init_path, 'r') as f:
            for line in f:
                if '__version__' in line:
                    init_version = line.split('=')[1].strip().strip('"')
                    break

    results.check("pyproject.toml version found",
                 pyproject_version is not None,
                 "Version not found in pyproject.toml")
    results.check("__init__.py version found",
                 init_version is not None,
                 "Version not found in __init__.py")
    results.check("Versions match",
                 pyproject_version == init_version if pyproject_version and init_version else False,
                 f"Mismatch: pyproject={pyproject_version}, init={init_version}")

    return results


def test_native_clamping():
    """Test 10: Native binding output clamping"""
    print("\n[Test 10] Native binding clamping")
    results = TestResults()

    native_path = os.path.join(os.path.dirname(__file__), '..', 'sdk', 'bindings', 'src', 'aeos_kernel.c')
    if os.path.exists(native_path):
        with open(native_path, 'r') as f:
            content = f.read()
            results.check("fp_clamp in ai_decide",
                         "fp_clamp(out[j]" in content,
                         "fp_clamp not found in ai_decide")
    else:
        results.check("aeos_kernel.c exists", False, "File not found")

    return results


def main():
    print("="*60)
    print("AEOS FIX VERIFICATION TESTS")
    print("="*60)

    all_results = []

    all_results.append(test_fd_table_per_process())
    all_results.append(test_ai_perceive_before_decide())
    all_results.append(test_cmt_bounds_check())
    all_results.append(test_pit_reprogramming())
    all_results.append(test_ipc_memcpy())
    all_results.append(test_shell_integrity_check())
    all_results.append(test_afs_free_extent())
    all_results.append(test_ramfs_error_code())
    all_results.append(test_version_sync())
    all_results.append(test_native_clamping())

    # Summary
    total_passed = sum(r.passed for r in all_results)
    total_failed = sum(r.failed for r in all_results)
    total = total_passed + total_failed

    print("\n" + "="*60)
    print("FINAL SUMMARY")
    print("="*60)
    print(f"Total tests: {total}")
    print(f"Passed:      {total_passed} ✓")
    print(f"Failed:      {total_failed} ✗")

    if total_failed == 0:
        print(f"\n{'='*60}")
        print("ALL TESTS PASSED! ✓")
        print(f"{'='*60}")
        return 0
    else:
        print(f"\n{'='*60}")
        print("SOME TESTS FAILED! ✗")
        print(f"{'='*60}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
