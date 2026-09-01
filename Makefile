.PHONY: setup build build-release build-iso build-gos test test-rust test-c \
        test-py test-boot bench lint lint-rust lint-c lint-py boundary-check \
        docs ci clean

# ---- Environment ------------------------------------------------------------
CARGO := cargo
UV    := uv
CMAKE := cmake

# ---- Setup ------------------------------------------------------------------
setup: ## Verify/install toolchains: rustup components, uv, cmake
	tools/build/setup.sh

# ---- Builds -----------------------------------------------------------------
build: ## Host debug build of Rust + C + Python (no image)
	$(CARGO) build --workspace --locked
	cmake -S hal -B build/hal -DCMAKE_BUILD_TYPE=Debug && cmake --build build/hal || true
	cmake -S drivers -B build/drivers -DCMAKE_BUILD_TYPE=Debug && cmake --build build/drivers || true
	cmake -S device -B build/device -DCMAKE_BUILD_TYPE=Debug && cmake --build build/device || true
	$(UV) sync --locked

build-release: ## Optimized, LTO, stripped
	$(CARGO) build --release --workspace --locked

build-iso: ## Build the real bootable x86_64 ISO (own El Torito loader)
	tools/build/make_iso.sh

build-gos: ## Build ajeeb.gos - the AEOS distribution image (x86 MBR + AEOS-FS)
	python3 tools/build/make_img.py --out build/ajeeb.gos

# ---- Tests ------------------------------------------------------------------
test: test-rust test-c test-py ## All test layers
test-rust:
	$(CARGO) test --workspace --locked
test-c:
	ctest --test-dir build/hal && ctest --test-dir build/drivers && ctest --test-dir build/device
test-py:
	$(UV) run pytest
test-boot: ## Boot smoke tests (auto-skips without qemu)
	bash tests/boot/smoke_aarch64.sh
	bash tests/boot/bios_smoke.sh
	bash tests/boot/selfboot_smoke.sh
	bash tests/boot/iso_smoke.sh
	bash tests/boot/gpt_smoke.sh
	bash tests/boot/tui_smoke.sh
	bash tests/boot/smp_smoke.sh
	bash tests/boot/net_smoke.sh

bench: ## Benchmark suites with regression thresholds
	$(CARGO) bench --workspace --locked
	$(UV) run pytest tests/ai -m bench --benchmark-autosave

# ---- Lint / quality gates ----------------------------------------------------
lint: lint-rust lint-c lint-py boundary-check
lint-rust:
	$(CARGO) fmt --all --check
	$(CARGO) clippy --workspace --all-targets -- -D warnings
	$(CARGO) deny check
lint-c:
	clang-format --dry-run --Werror $$(find hal drivers device -name '*.[ch]')
	clang-tidy $$(find hal/src drivers/src device/src -name '*.c') 2>/dev/null
lint-py:
	$(UV) run ruff check .
	$(UV) run ruff format --check .
	$(UV) run mypy
	$(UV) run bandit -r sdk perception vision audio planning reasoning learning simulation robotics communication

boundary-check: ## Assembly only in boot/ and kernel/src/arch/; no cross-FDI
	tools/check/boundaries.sh

# ---- Docs & CI ---------------------------------------------------------------
docs: ## Build the documentation site (strict)
	scripts/docs.sh

ci: lint test build-iso docs ## The full gate (mirrors .github/workflows/ci.yaml)

clean:
	rm -rf build target .pytest_cache .mypy_cache .ruff_cache .coverage* site docs/_root docs/index.md
	find . -name __pycache__ -prune -exec rm -rf {} +
