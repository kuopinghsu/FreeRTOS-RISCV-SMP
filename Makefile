FREERTOS_VER  ?= V11.3.0
RISCV_PREFIX  ?= /opt/xpack-riscv-none-elf-gcc-15.2.0-1/bin/riscv-none-elf-
SPIKE         ?= $(HOME)/opt/riscv/bin/spike
CORES         ?= 2
export RISCV_PREFIX
export SPIKE
export CORES

# Example names mirrored from Demo/examples/Makefile.
# Running `make <example>` (e.g. `make all_in_one` or `make freertos_test`)
# builds the simulator + the ELF and runs it.
# Pass CORES=N to choose the core count:
#   make CORES=1 freertos_test
#   make CORES=4 freertos_test
EXAMPLES := all_in_one freertos_test

.PHONY: all run check arch-test-run arch-test-setup clean distclean help $(EXAMPLES)

all: check
	$(MAKE) -C Demo
	$(MAKE) -C sim

run: all
	$(MAKE) -C Demo run

check:
	[ -d FreeRTOS-Kernel ] || git clone -b ${FREERTOS_VER} https://github.com/FreeRTOS/FreeRTOS-Kernel.git FreeRTOS-Kernel

# Delegate `make [CORES=N] <example>` to Demo/examples/Makefile which knows
# how to build the simulator, compile the ELF, and launch the simulation.
$(EXAMPLES): check
	$(MAKE) -C Demo/examples $@

arch-test-run:
	$(MAKE) -C verify arch-test-run

arch-test-setup:
	$(MAKE) -C verify arch-test-setup

clean:
	-rm -rf build

distclean:
	-rm -rf FreeRTOS-Kernel

help:
	@echo "FreeRTOS-RISCV-SMP -- top-level targets"
	@echo ""
	@echo "Usage: make [CORES=N] [RISCV_PREFIX=...] [SPIKE=...] <target>"
	@echo ""
	@echo "Targets:"
	@echo "  all                 Build Demo + simulator (default)"
	@echo "  run                 Build and run the Demo"
	@echo "  all_in_one          Build simulator + all_in_one example ELF and run it"
	@echo "  freertos_test       Build simulator + freertos_test ELF and run it"
	@echo "  arch-test-setup     Init arch-test submodule and install Python venv"
	@echo "  arch-test-run       Run RISC-V architectural certification tests"
	@echo "  clean               Remove build/ directory"
	@echo "  distclean           Remove build/ and FreeRTOS-Kernel/"
	@echo ""
	@echo "Variables:"
	@echo "  CORES=N             SMP core count (default: 2)"
	@echo "  RISCV_PREFIX=...    Toolchain prefix (default: riscv-none-elf-)"
	@echo "  SPIKE=...           Path to spike binary"

