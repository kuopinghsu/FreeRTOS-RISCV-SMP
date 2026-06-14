# FreeRTOS RISC-V SMP

FreeRTOS V11.3.0 running on a software-simulated RV64IMAC multi-core (SMP) processor.
Supports 1–16 cores via a single codebase; all concurrency constructs (mutexes, semaphores, notifications, event groups, queues) are exercised by the included test suite.

## Repository layout

```
FreeRTOS-Kernel/        FreeRTOS V11.3.0 kernel (git submodule / cloned)
Demo/
  common/               crt0.S, linker script, syscall stubs
  conf/                 FreeRTOSConfig.h
  port/RISC-V/          Custom RV64 SMP port (port.c, portASM.S, portmacro.h)
  examples/
    all_in_one/         Minimal "hello world" FreeRTOS demo
    freertos_test/      6-test SMP validation suite (scales 1–8 cores)
sim/                    RV64IMAC software simulator (C++17, CLINT, HTIF, GDB stub)
verify/                 RISC-V Architecture Tests (riscv-arch-test)
```

## Prerequisites

| Tool | Tested version |
|------|---------------|
| xPack RISC-V toolchain | `riscv-none-elf-gcc 15.2.0-1` |
| Host C++17 compiler | `g++ 12` or newer |
| GNU Make | 4.x |

Install the xPack toolchain to `/opt/xpack-riscv-none-elf-gcc-15.2.0-1/` or override `RISCV_PREFIX`:

```bash
make RISCV_PREFIX=/path/to/riscv-none-elf- freertos_test
```

## Quick start

```bash
# Clone + fetch FreeRTOS kernel
git clone <this-repo> && cd FreeRTOS-RISCV-SMP
make              # builds simulator + Demo ELFs

# Run the test suite on 1 / 2 / 4 / 8 cores
make CORES=1 freertos_test
make CORES=2 freertos_test
make CORES=4 freertos_test
make CORES=8 freertos_test
```

Expected output (CORES=2):

```
freertos_test: starting
  cores=2   workers=6    sem_slots=2   iter_fast=50   iter_slow=20
test 1: context-switch stress       ... pass
test 2: mutex contention            ... pass
test 3: counting-sem + mutex        ...   (sem_slots=2, workers=6)
pass
test 4: task notifications          ... pass
test 5: event group                 ... pass
test 6: queue stress                ... pass
freertos_test: all tests passed
```

## Test suite — `freertos_test`

All 6 tests share the same sizing formula so they scale automatically with `CORES`:

| Constant | Formula | CORES=1 | CORES=2 | CORES=4 | CORES=8 |
|----------|---------|---------|---------|---------|---------|
| `NUM_WORKERS` | `2×CORES+2` | 4 | 6 | 10 | 18 |
| `SEM_SLOTS` | `CORES` | 1 | 2 | 4 | 8 |
| `Q_HALF` | `NUM_WORKERS/2` | 2 | 3 | 5 | 9 |
| `EV_WORKERS` | `min(NUM_WORKERS,24)` | 4 | 6 | 10 | 18 |

### Test descriptions

1. **Context-switch stress** — `NUM_WORKERS` tasks at equal priority each call `taskYIELD()` `ITER_FAST` (50) times.  Forces intra-core and cross-core context switches; verifies per-task iteration counts.

2. **Mutex contention** — All workers race for one mutex and increment a shared counter `ITER_FAST` times each.  Final counter must equal `NUM_WORKERS × ITER_FAST`.

3. **Counting-sem + mutex** (pattern from `trace_test.c`) — Each worker acquires a counting semaphore (limit = `SEM_SLOTS`) before entering the shared area, then acquires a mutex for the single-writer critical section.  Demonstrates bounded concurrency: at most `CORES` tasks are active in the shared area simultaneously.

4. **Task notifications** — `NUM_WORKERS` notifier tasks each send `ITER_FAST` notifications to a single collector via `xTaskNotifyGive` / `ulTaskNotifyTake`.  Verifies the exact total across all cores.

5. **Event group** — `EV_WORKERS` tasks each set one unique bit after a yield loop; the runner blocks on `xEventGroupWaitBits` for all bits simultaneously.

6. **Queue stress** — `Q_HALF` producer tasks and `Q_HALF` consumer tasks, queue depth = `Q_HALF`.  The queue fills quickly, exercising cross-core block/unblock wakeup chains.

## SMP port notes

The RISC-V SMP port lives in `Demo/port/RISC-V/`.  Key implementation details:

- **Recursive task lock** — `vPortGetTaskLock` / `vPortReleaseTaskLock` use a per-core owner + depth counter so that `vTaskSuspendAll → xTaskResumeAll` re-entry on the same hart never deadlocks.
- **Timer tick wrapper** — `xPortTimerTickHandler` in `port.c` acquires the task lock and enters an ISR critical section before calling `xTaskIncrementTick`, satisfying the `prvYieldForTask` assertion that `portGET_CRITICAL_NESTING_COUNT > 0`.
- **portASM.S** — Hart 0 calls `xPortTimerTickHandler`; all harts call `vTaskSwitchContext` when a switch is required.

## Simulator

`sim/` contains a single-file RV64IMAC interpreter (`simulator.cpp`) with:

- CLINT (mtime / mtimecmp per hart) for tick generation
- HTIF `tohost` / `fromhost` for `putchar` and exit
- Optional GDB RSP stub (`--gdb <port>`)

```
Usage: riscv64-sim [--cores N] [--gdb PORT] <elf>
```

## Make targets

| Target | Description |
|--------|-------------|
| `make` / `make all` | Build simulator + all demo ELFs (CORES=2 default) |
| `make run` | Build and run the Demo |
| `make [CORES=N] freertos_test` | Build and run `freertos_test` with N cores |
| `make [CORES=N] all_in_one` | Build and run minimal demo with N cores |
| `make arch-test-setup` | Init arch-test submodule and install Python venv |
| `make arch-test-run` | Run RISC-V architectural certification tests (130 tests) |
| `make help` | Print this target summary |
| `make clean` | Remove `build/` |
| `make distclean` | Remove `build/` and cloned `FreeRTOS-Kernel/` |

Variables accepted by any target: `CORES=N`, `RISCV_PREFIX=<prefix>`, `SPIKE=<path>`.

## License

FreeRTOS Kernel — MIT (see `FreeRTOS-Kernel/LICENSE.md`).
Port, simulator, and demo code — MIT.
