EXAMPLE ?= freertos_test
CORES ?= 2
SIM ?= ../build/sim/riscv64-sim

.PHONY: all clean list run

all:
	$(MAKE) -C examples all

clean:
	$(MAKE) -C examples clean

list:
	$(MAKE) -C examples list

run:
	$(MAKE) -C ../sim
	$(MAKE) -C examples run EXAMPLE=$(EXAMPLE) CORES=$(CORES) SIM=$(abspath $(SIM))
