# verify — RISC-V Architectural Certification Tests

This directory contains the infrastructure for running the
[RISC-V Architectural Compatibility Tests (ACT4)](https://github.com/riscv/riscv-arch-test)
against the `riscv64-sim` software simulator.

## Directory layout

```
verify/
├── Makefile                        # Top-level verification targets
├── sail_riscv_sim_wrapper.sh.in    # Template wrapper for the Sail reference model
├── arch-test/                      # riscv-arch-test submodule (pinned commit)
└── conf/
    └── riscv64-sim/
        ├── test_config.yaml        # ACT4 DUT configuration (compiler, ref model, …)
        ├── riscv64-sim.yaml        # UDB YAML: implemented extensions / CSRs
        ├── link.ld                 # Linker script for ACT test ELFs
        ├── rvmodel_macros.h        # DUT-specific test harness macros
        └── sail.json               # Sail reference model configuration
```

## Quick start

### 1. Prerequisites

| Dependency | Purpose |
|------------|---------|
| `riscv-none-elf-gcc` (xPack 15.2+) | Cross-compiler for test ELFs |
| `spike` | Sail/Spike reference model |
| [`uv`](https://github.com/astral-sh/uv) | Python env manager (auto-installed if absent) |
| `git` submodule | `verify/arch-test` checked out |

The toolchain prefix defaults to `/opt/xpack-riscv-none-elf-gcc-15.2.0-1/bin/riscv-none-elf-`.
Override with `RISCV_PREFIX=<prefix>` on the make command line.

### 2. One-time setup

```sh
make -C verify arch-test-setup
```

This initialises the `arch-test` submodule at its pinned commit and creates
a Python virtual environment inside `verify/arch-test/.venv`.

### 3. Run all tests

```sh
make -C verify arch-test-run
```

Or from the repository root:

```sh
make arch-test-run
```

Three phases are executed automatically:

1. **Build** — compiles `build/sim/riscv64-sim`.
2. **Generate** — ACT4 produces self-checking ELFs using Spike as the reference
   model.  Each ELF embeds the expected signature and verifies it at runtime.
3. **Run** — every ELF is executed on `riscv64-sim`; pass/fail is determined
   from the `RVCP-SUMMARY` line written to stdout by each test.

Results are written to `build/arch-test-work/riscv64-sim/`:

```
build/arch-test-work/riscv64-sim/
├── elfs/       # generated self-checking test ELFs
├── logs/       # per-test simulator output
└── summary.log # overall pass/fail summary
```

### 4. Selective runs

```sh
# Only run integer base + multiply extensions
make -C verify arch-test-run EXTENSIONS=I,M

# Use 8 parallel build jobs
make -C verify arch-test-run JOBS=8

# Custom work directory
make -C verify arch-test-run WORKDIR=/tmp/act-work
```

### 5. Clean up

```sh
make -C verify arch-test-clean   # removes build/arch-test-work
```

## Excluded extensions

The following extension groups are excluded from the default run because
`riscv64-sim` is a M-mode-only simulator with no MMU or PMP:

| Excluded | Reason |
|----------|--------|
| `S`, `ExceptionsSm`, `InterruptsSm` | Supervisor mode not implemented |
| `ExceptionsZc` | Assembler macro incompatibility with GCC 15.2 |
| `PMPSm`, `PMPZca`, `PMPmisaligned` | PMP not implemented |
| `Sv`, `Svade`, `Svadu`, `SvaduPMP`, `SvPMP`, `SvZicbo`, `SvPMPZicbo` | Virtual memory not implemented |

## Updating the arch-test submodule

```sh
cd verify/arch-test
git checkout <new-commit>
cd ../..
git add verify/arch-test
git commit -m "verify: update arch-test to <new-commit>"
```

## Reference model

The Spike ISA string used as the reference is:
`rv64imafdcbvh` plus all Z/S extensions.
The simulator is expected to produce identical signatures for all passing tests.
All 130 standard tests pass with the current implementation.
All 128 standard tests pass as of the current pinned commit.
