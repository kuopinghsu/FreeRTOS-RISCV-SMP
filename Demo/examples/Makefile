RISCV_PREFIX ?= /opt/xpack-riscv-none-elf-gcc-15.2.0-1/bin/riscv-none-elf-
CC := $(RISCV_PREFIX)gcc
ARCH_FLAGS := -march=rv64imac_zicsr_zaamo_zalrsc -mabi=lp64 -mcmodel=medany
CFLAGS ?= -O2 -g -ffreestanding -ffunction-sections -fdata-sections -Wall -Wextra
CPPFLAGS += -ICommon
LDFLAGS += $(ARCH_FLAGS) -nostartfiles -Wl,--no-relax -Wl,-T,../Common/linker.ld -Wl,--gc-sections
LDLIBS += -lc -lgcc

# Use a per-CORES output directory so that switching CORES always triggers a
# full rebuild: both object files and the linked ELF are keyed on $(CORES).
# This prevents a CORES=2 ELF from masquerading as a CORES=1 ELF and vice
# versa (the shared-OUTDIR problem where the ELF timestamp defeats make).
OUTDIR := ../../build/demo/examples/cores$(CORES)
OBJDIR := ../../build/demo/obj/cores$(CORES)
COMMON_SRCS := ../Common/crt0.S ../Common/syscall.c
COMMON_OBJS := $(OBJDIR)/Common/crt0.o $(OBJDIR)/Common/syscall.o
EXAMPLES := all_in_one freertos_test
TARGETS := $(addprefix $(OUTDIR)/,$(addsuffix .elf,$(EXAMPLES)))

EXAMPLE ?= all_in_one
CORES   ?= 1
SIM     ?= ../../build/sim/riscv64-sim

# ── FreeRTOS kernel paths ────────────────────────────────────────────────────
FREERTOS_KERNEL  := ../../FreeRTOS-Kernel
FREERTOS_MEMMANG := $(FREERTOS_KERNEL)/portable/MemMang
FREERTOS_CONF    := ../conf
FREERTOS_CHIPS   := ../port/RISC-V

# Demo/port/RISC-V/ is self-contained (portmacro.h, portContext.h,
# freertos_risc_v_chip_specific_extensions.h all live there).
# FreeRTOS-Kernel/portable/GCC/RISC-V/ is NOT on the include path.
FREERTOS_CPPFLAGS := \
    -I$(FREERTOS_KERNEL)/include \
    -I$(FREERTOS_CHIPS) \
    -I$(FREERTOS_CONF)

# Pass the core count to every FreeRTOS translation unit so that
# configNUMBER_OF_CORES is set from the command line.
FREERTOS_CPPFLAGS += -DconfigNUMBER_OF_CORES=$(CORES)

# ── Port source selection ───────────────────────────────────────────────────
# Demo/port/RISC-V/port.c and portASM.S handle both single-core and SMP via
# #if configNUMBER_OF_CORES guards; no conditional source selection needed.
FREERTOS_PORT_C    := $(FREERTOS_CHIPS)/port.c
FREERTOS_PORT_ASM  := $(FREERTOS_CHIPS)/portASM.S
FREERTOS_PORT_COBJ := $(OBJDIR)/freertos/port.o
FREERTOS_PORT_OBJ  := $(OBJDIR)/freertos/portASM.o

FREERTOS_C_SRCS := \
    $(FREERTOS_KERNEL)/tasks.c \
    $(FREERTOS_KERNEL)/queue.c \
    $(FREERTOS_KERNEL)/list.c \
    $(FREERTOS_KERNEL)/event_groups.c \
    $(FREERTOS_KERNEL)/timers.c \
    $(FREERTOS_PORT_C) \
    $(FREERTOS_MEMMANG)/heap_4.c

FREERTOS_OBJS := $(patsubst %.c,$(OBJDIR)/freertos/%.o,$(notdir $(FREERTOS_C_SRCS)))

# Tell make where to find the FreeRTOS C sources when building pattern rules.
# $(FREERTOS_CHIPS) provides port.c; upstream GCC/RISC-V/ is not used.
vpath %.c $(FREERTOS_KERNEL) $(FREERTOS_MEMMANG) $(FREERTOS_CHIPS)

.PHONY: all clean list run build-sim $(EXAMPLES)

all: $(TARGETS)

clean:
	rm -rf $(OUTDIR) $(OBJDIR)

list:
	@printf '%s\n' $(EXAMPLES)

# Build the simulator (no-op if already up to date).
build-sim:
	$(MAKE) -C ../../sim

run: $(OUTDIR)/$(EXAMPLE).elf build-sim
	$(SIM) --cores $(CORES) $<

# Shortcut targets: `make [CORES=N] <example>` builds the simulator + ELF then
# runs immediately.
# Examples:
#   make all_in_one              → build sim + ELF, run with default CORES=2
#   make CORES=1 freertos_test  → build sim + ELF, run single-core
#   make CORES=4 freertos_test  → build sim + ELF, run four-core
$(EXAMPLES): %: $(OUTDIR)/%.elf build-sim
	$(SIM) --cores $(CORES) $<

# ── Generic link rule (bare-metal examples) ──────────────────────────────────
$(OUTDIR)/%.elf: $(OBJDIR)/%/main.o $(COMMON_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

# ── freertos_test: link with FreeRTOS kernel objects ─────────────────────────
$(OUTDIR)/freertos_test.elf: \
    $(OBJDIR)/freertos_test/main.o \
    $(COMMON_OBJS) \
    $(FREERTOS_OBJS) \
    $(FREERTOS_PORT_OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

# freertos_test/main.c needs extra include paths for FreeRTOS headers.
$(OBJDIR)/freertos_test/main.o: CPPFLAGS += $(FREERTOS_CPPFLAGS)

# ── Common object compilation rules ──────────────────────────────────────────
$(OBJDIR)/Common/crt0.o: ../Common/crt0.S
	@mkdir -p $(dir $@)
	$(CC) $(ARCH_FLAGS) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/Common/syscall.o: ../Common/syscall.c
	@mkdir -p $(dir $@)
	$(CC) $(ARCH_FLAGS) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/%/main.o: %/main.c
	@mkdir -p $(dir $@)
	$(CC) $(ARCH_FLAGS) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

# ── FreeRTOS C kernel objects (uses vpath to locate sources) ─────────────────
$(OBJDIR)/freertos/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(ARCH_FLAGS) $(CPPFLAGS) $(FREERTOS_CPPFLAGS) $(CFLAGS) -c -o $@ $<

# ── FreeRTOS port C object (explicit rule avoids vpath ambiguity) ────────────
$(FREERTOS_PORT_COBJ): $(FREERTOS_PORT_C)
	@mkdir -p $(dir $@)
	$(CC) $(ARCH_FLAGS) $(CPPFLAGS) $(FREERTOS_CPPFLAGS) $(CFLAGS) -c -o $@ $<

# ── FreeRTOS assembly port object ────────────────────────────────────────────
$(FREERTOS_PORT_OBJ): $(FREERTOS_PORT_ASM)
	@mkdir -p $(dir $@)
	$(CC) $(ARCH_FLAGS) $(CPPFLAGS) $(FREERTOS_CPPFLAGS) $(CFLAGS) -c -o $@ $<
