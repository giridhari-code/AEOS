"""Tests for the FFI loader's graceful fallback behaviour."""

from aeos_sdk.ffi import load_native, open_kernel
from aeos_sdk.mock import MockKernel


def test_load_native_returns_none_without_library(monkeypatch) -> None:
    monkeypatch.delenv("AEOS_KERNEL_LIB", raising=False)
    monkeypatch.chdir("/")
    assert load_native() is None


def test_open_kernel_falls_back_to_mock(monkeypatch, tmp_path) -> None:
    monkeypatch.delenv("AEOS_KERNEL_LIB", raising=False)
    monkeypatch.chdir(tmp_path)
    kernel = open_kernel()
    assert isinstance(kernel, MockKernel)


def test_env_var_points_at_missing_library(monkeypatch, tmp_path) -> None:
    monkeypatch.setenv("AEOS_KERNEL_LIB", str(tmp_path / "missing.so"))
    assert load_native() is None
