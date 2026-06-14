# ============================================================================
# File: verify/Makefile
# Project: FreeRTOS-RISCV-SMP Software Simulator
# Description: Verification targets -- RISC-V Architectural Certification Tests
#              (ACT4) for the riscv64-sim software simulator.
#
# Invoke from the repo root via:
#   make -C verify arch-test-setup
#   make -C verify arch-test-run
# ============================================================================

# -- Directories ------------------------------------------------------------------
VERIF_DIR    := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
REPO_ROOT    := $(abspath $(VERIF_DIR)/..)

# -- ACT4 source ------------------------------------------------------------------
# arch-test is a git submodule pinned to a specific commit.
# arch-test-setup checks it out at that pinned commit (no --remote).
# To update: cd verify/arch-test && git checkout <new-commit>
#            cd ../.. && git add verify/arch-test && git commit
ACT_DIR          := $(VERIF_DIR)arch-test
ACT_VENV         := $(ACT_DIR)/.venv
ACT_SENTINEL     := $(ACT_VENV)/.act-setup-done

# -- DUT config -------------------------------------------------------------------
DUT_CONFIG   := $(VERIF_DIR)conf/riscv64-sim
WORKDIR      ?= $(REPO_ROOT)/build/arch-test-work
ARCH_TEST_JOBS ?= $(if $(JOBS),$(JOBS),$(shell command -v nproc >/dev/null 2>&1 && nproc || echo 4))
ARCH_TEST_CONFIG := $(WORKDIR)/test_config.generated.yaml
ARCH_TEST_REF_WRAPPER := $(WORKDIR)/sail_riscv_sim.wrapper.sh
ARCH_TEST_CONFIG_DIR := $(WORKDIR)/riscv64-sim
ARCH_TEST_EXTENSIONS_FILE := $(ARCH_TEST_CONFIG_DIR)/extensions.txt

# -- Simulator (DUT) --------------------------------------------------------------
SIM          := $(REPO_ROOT)/build/sim/riscv64-sim

# -- Toolchain --------------------------------------------------------------------
RISCV_PREFIX ?= /opt/xpack-riscv-none-elf-gcc-15.2.0-1/bin/riscv-none-elf-
# Derive toolchain bin dir from the prefix path (works for both bare names and
# absolute paths like /opt/.../bin/riscv-none-elf-).
RISCV_TOOLCHAIN_BIN ?= $(patsubst %/,%,$(dir $(RISCV_PREFIX)))

# -- Reference model (spike) -----------------------------------------------------
# spike must be on PATH, or set SPIKE to an absolute path.
SPIKE        ?= $(HOME)/opt/riscv/bin/spike
SPIKE_BIN    ?= $(patsubst %/,%,$(dir $(SPIKE)))

# -- uv (Python env manager) ------------------------------------------------------
UV := $(shell command -v uv 2>/dev/null || echo $(HOME)/.local/bin/uv)

# ============================================================================
.PHONY: all arch-test-setup arch-test-run arch-test-clean help

all: help

# Create a local wrapper that delegates to sail_riscv_sim.
$(ARCH_TEST_REF_WRAPPER): $(VERIF_DIR)sail_riscv_sim_wrapper.sh.in $(VERIF_DIR)Makefile
	@mkdir -p "$(WORKDIR)"
	@sail_bin="$$(command -v $(SAIL) 2>/dev/null || echo $(SAIL))"; \
	cp "$(VERIF_DIR)sail_riscv_sim_wrapper.sh.in" "$(ARCH_TEST_REF_WRAPPER)"; \
	sed -i.bak "s|__SAIL_BIN__|$$sail_bin|g" "$(ARCH_TEST_REF_WRAPPER)"; \
	rm -f "$(ARCH_TEST_REF_WRAPPER).bak"; \
	chmod +x "$(ARCH_TEST_REF_WRAPPER)"

# Generate a toolchain-adjusted ACT config for this run.
$(ARCH_TEST_CONFIG): $(DUT_CONFIG)/test_config.yaml $(VERIF_DIR)Makefile
	@mkdir -p "$(WORKDIR)"
	@sed -e 's|^[[:space:]]*compiler_exe:.*|compiler_exe: $(RISCV_PREFIX)gcc|' \
	     -e 's|^[[:space:]]*objdump_exe:.*|objdump_exe: $(RISCV_PREFIX)objdump|' \
	     -e 's|^[[:space:]]*ref_model_exe:.*|ref_model_exe: $(SPIKE)|' \
	     -e 's|^[[:space:]]*udb_config:.*|udb_config: $(DUT_CONFIG)/riscv64-sim.yaml|' \
	     -e 's|^[[:space:]]*linker_script:.*|linker_script: $(DUT_CONFIG)/link.ld|' \
	     -e 's|^[[:space:]]*dut_include_dir:.*|dut_include_dir: $(DUT_CONFIG)|' \
	     "$(DUT_CONFIG)/test_config.yaml" > "$@"

# Pre-generate implemented extension list directly from DUT UDB YAML.
# If present and up-to-date, ACT skips invoking UDB tooling (Ruby/Bundler).
$(ARCH_TEST_EXTENSIONS_FILE): $(DUT_CONFIG)/riscv64-sim.yaml $(VERIF_DIR)Makefile
	@mkdir -p "$(ARCH_TEST_CONFIG_DIR)"
	@sed -nE 's|.*name:[[:space:]]*([^,}[:space:]]+).*|\1|p' "$(DUT_CONFIG)/riscv64-sim.yaml" > "$@"
	@ext_count=$$(wc -l < "$@" | tr -d '[:space:]'); \
	  echo "[arch-test] Prepared $$ext_count implemented extensions in $(ARCH_TEST_EXTENSIONS_FILE)"

# ============================================================================
# arch-test-setup: init riscv-arch-test submodule and install Python venv
# ============================================================================
$(ACT_SENTINEL):
	@# -- 1. Ensure uv is available -----------------------------------------------
	@if ! command -v $(UV) >/dev/null 2>&1; then \
	    echo "[arch-test] Installing uv..."; \
	    curl -LsSf https://astral.sh/uv/install.sh | sh; \
	fi
	@# -- 2. Checkout arch-test at the pinned commit ------------------------------
	@# Use git submodule if the gitlink is tracked; otherwise clone directly.
	@if git -C $(REPO_ROOT) ls-files --stage verify/arch-test 2>/dev/null | grep -q '^160000'; then \
	    echo "[arch-test] Checking out pinned arch-test submodule..."; \
	    git -C $(REPO_ROOT) submodule sync -- verify/arch-test; \
	    git -C $(REPO_ROOT) submodule update --init verify/arch-test; \
	elif [ -d "$(ACT_DIR)/.git" ]; then \
	    echo "[arch-test] Updating existing arch-test clone..."; \
	    git -C $(ACT_DIR) fetch --quiet; \
	    git -C $(ACT_DIR) checkout --quiet HEAD; \
	else \
	    echo "[arch-test] Cloning riscv-arch-test (main)..."; \
	    git clone --depth 1 https://github.com/riscv/riscv-arch-test "$(ACT_DIR)"; \
	fi
	@# Remove stale untracked generated files left by any previous run.
	@if [ -d "$(ACT_DIR)/.git" ]; then \
	    git -C $(ACT_DIR) clean -fdq -- tests/ coverpoints/unpriv/ external/ 2>/dev/null || true; \
	fi
	@# -- 3. Bootstrap Python venv via uv -----------------------------------------
	@echo "[arch-test] Installing Python environment (uv sync)..."
	@cd $(ACT_DIR) && $(UV) sync --quiet
	@# Configure Bundler to install UDB gems locally (avoids needing root for /var/lib/gems)
	@cd $(ACT_DIR) && bundle config set --local path vendor/bundle 2>/dev/null || true
	@touch $(ACT_SENTINEL)
	@echo "[arch-test] Setup complete. venv: $(ACT_VENV)"

arch-test-setup: $(ACT_SENTINEL)

# ============================================================================
# arch-test-run: generate self-checking ELFs and run them on riscv64-sim
#
# Requires arch-test-setup to have been run first.
# Requires DUT config files in $(DUT_CONFIG)/.
#
# Phase 1: builds riscv64-sim
# Phase 2: generates self-checking ELFs via ACT4 (Sail reference model)
#          -- each ELF embeds the expected signature and compares at runtime
# Phase 3: runs every ELF on riscv64-sim; pass/fail is determined from the
#          RVCP-SUMMARY line printed to stdout by each test
#
# Variables:
#   EXTENSIONS   -- comma-separated list (default: all enabled in UDB config)
#   JOBS         -- parallel build jobs (default: nproc)
#   WORKDIR      -- build/output dir (default: build/arch-test-work)
#
# EXCLUDE_EXTENSIONS rationale:
#   S               -- supervisor mode; riscv64-sim is M-mode only
#   ExceptionsSm    -- supervisor-mode exception tests
#   InterruptsSm    -- supervisor interrupt tests
#   ExceptionsZc           -- Zc priv-mode exception tests; assembler macro incompatibility with GCC 15.2
#   PMPSm/PMPZca/PMPmisaligned -- PMP tests; simulator has no PMP
#   Sv/Svade/...    -- virtual memory tests; simulator has no MMU
# ============================================================================
arch-test-run: $(ACT_SENTINEL) $(ARCH_TEST_CONFIG) $(ARCH_TEST_EXTENSIONS_FILE)
	@set -e; \
	export PATH="$(RISCV_TOOLCHAIN_BIN):$(SPIKE_BIN):$$PATH"; \
	export BUNDLE_PATH="$(ACT_DIR)/vendor/bundle"; \
	stty_state="$$(stty -g 2>/dev/null || true)"; \
	restore_tty() { \
	  if [ -n "$$stty_state" ]; then \
	    stty "$$stty_state" 2>/dev/null || stty sane 2>/dev/null || true; \
	  else \
	    stty sane 2>/dev/null || true; \
	  fi; \
	}; \
	trap 'restore_tty' EXIT INT TERM; \
	mkdir -p "$(WORKDIR)"; \
	echo "=========================================="; \
	echo "[arch-test] Work directory: $(WORKDIR)"; \
	echo "[arch-test] Phase 1/3: building riscv64-sim"; \
	echo "=========================================="; \
	$(MAKE) -C $(REPO_ROOT)/sim --no-print-directory; \
	echo "=========================================="; \
	echo "[arch-test] Phase 2/3: generating self-checking ELFs (spike reference)"; \
	echo "=========================================="; \
	IN_UDB_CONTAINER=1 $(MAKE) -C $(ACT_DIR) \
	    CONFIG_FILES=$(ARCH_TEST_CONFIG) \
	    WORKDIR=$(WORKDIR) \
        EXCLUDE_EXTENSIONS=S,ExceptionsSm,InterruptsSm,ExceptionsZc,PMPSm,PMPZca,PMPmisaligned,Sv,Svade,Svadu,SvaduPMP,SvPMP,SvZicbo,SvPMPZicbo \
	    $(if $(EXTENSIONS),EXTENSIONS=$(EXTENSIONS),) \
	    --jobs $(ARCH_TEST_JOBS); \
	elf_count=$$(find "$(WORKDIR)/riscv64-sim/elfs" -name '*.elf' 2>/dev/null | wc -l | tr -d '[:space:]'); \
	echo "=========================================="; \
	echo "[arch-test] Phase 3/3: running $$elf_count ELFs on riscv64-sim"; \
	echo "[arch-test] Logs:    $(WORKDIR)/riscv64-sim/logs"; \
	echo "[arch-test] Summary: $(WORKDIR)/riscv64-sim/summary.log"; \
	echo "=========================================="; \
	cd $(ACT_DIR) && $(ACT_VENV)/bin/python run_tests.py \
	    "$(SIM) {debug:--trace}" \
	    "$(WORKDIR)/riscv64-sim/elfs"

# ============================================================================
# arch-test-%: pass-through any other target to the ACT4 Makefile
# ============================================================================
arch-test-%: $(ACT_SENTINEL) $(ARCH_TEST_CONFIG) $(ARCH_TEST_EXTENSIONS_FILE)
	@PATH="$(RISCV_TOOLCHAIN_BIN):$(SPIKE_BIN):$$PATH" $(MAKE) -C $(ACT_DIR) \
	    CONFIG_FILES=$(ARCH_TEST_CONFIG) \
	    WORKDIR=$(WORKDIR) \
	    $*

# ============================================================================
# arch-test-clean: remove generated artifacts (keeps submodule and venv)
# ============================================================================
arch-test-clean:
	@rm -rf $(WORKDIR)
	@echo "[arch-test] Work directory removed: $(WORKDIR)"

# ============================================================================
# help
# ============================================================================
help:
	@echo "verify/Makefile -- riscv64-sim Architectural Certification Test targets"
	@echo ""
	@echo "Usage: make -C verify <target>"
	@echo ""
	@echo "Targets:"
	@echo "  arch-test-setup    Init arch-test submodule and install Python venv"
	@echo "  arch-test-run      Generate self-checking ELFs and run on riscv64-sim"
	@echo "  arch-test-<tgt>    Forward <tgt> to the ACT4 Makefile"
	@echo "  arch-test-clean    Remove work directory"
	@echo ""
	@echo "Variables:"
	@echo "  EXTENSIONS=<list>  Comma-separated extension list (default: all)"
	@echo "  JOBS=<n>           Parallel build jobs (default: nproc)"
	@echo "  WORKDIR=<dir>      Build/output dir (default: build/arch-test-work)"
