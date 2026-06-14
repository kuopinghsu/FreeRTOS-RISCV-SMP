/* rvmodel_macros.h — RVMODEL macro definitions for riscv64-sim RV64IMAC
 * SPDX-License-Identifier: Apache-2.0
 *
 * riscv64-sim memory map (relevant):
 *   0x02000000  CLINT MSIP     — software interrupt pending (4 bytes/hart)
 *   0x02004000  mtimecmp (lo)  — timer compare register
 *   0x0200BFF8  mtime          — real-time counter
 *   0x83FFF000  tohost         — HTIF request register (64-bit, ELF symbol)
 *   0x83FFF008  fromhost       — HTIF response register (64-bit, ELF symbol)
 *
 * HTIF protocol (tohost encoding):
 *   bits[63:56] = dev   (0 = syscall, 1 = console)
 *   bits[55:48] = cmd   (0 = syscall/exit, 1 = putchar)
 *   bits[47: 0] = payload
 *
 * Exit (dev=0, cmd=0):
 *   payload[0] = 1  →  exit, exit_code = payload >> 1
 *   Write 1 to tohost  →  exit(0)  →  PASS
 *   Write 3 to tohost  →  exit(1)  →  FAIL
 *
 * Console putchar (dev=1, cmd=1):
 *   payload[7:0] = ASCII character
 *   tohost value = 0x0101_0000_0000_00CC  (CC = character byte)
 */

#ifndef _RVMODEL_MACROS_H
#define _RVMODEL_MACROS_H

#define RVMODEL_DATA_SECTION

##### STARTUP #####

/*
 * riscv64-sim resets with mstatus.MIE = 0 (interrupts disabled).
 * No special boot sequence required.
 */
#define RVMODEL_BOOT

/*
 * STANDARD_SM_SUPPORTED: tell the ACT framework that our DUT provides a
 * standard M-mode with writable mtvec, mscratch, mepc, mcause, mtval.
 * This enables RVTEST_TRAP_PROLOG M in rvmodel_boot, which sets mtvec
 * to the generated trampoline so that ecall/ebreak are handled correctly.
 */
#define STANDARD_SM_SUPPORTED

/*
 * Address that is guaranteed to be unmapped and cause an access fault.
 * 0x00000000 is below the lowest mapped region (CLINT at 0x02000000).
 */
#define RVMODEL_ACCESS_FAULT_ADDRESS 0x00000000

##### TERMINATION #####

/*
 * RVMODEL_HALT_PASS — terminate test with exit code 0 (PASS).
 * HTIF: write 1 to tohost  →  payload = 1  →  exit_code = 1 >> 1 = 0
 */
#define RVMODEL_HALT_PASS                               \
  la   t0, tohost                                      ;\
  li   t1, 1                                           ;\
  sd   t1, 0(t0)                                       ;\
rvmodel_halt_pass_loop:                                ;\
  j    rvmodel_halt_pass_loop                          ;\

/*
 * RVMODEL_HALT_FAIL — terminate test with exit code 1 (FAIL).
 * HTIF: write 3 to tohost  →  payload = 3  →  exit_code = 3 >> 1 = 1
 */
#define RVMODEL_HALT_FAIL                               \
  la   t0, tohost                                      ;\
  li   t1, 3                                           ;\
  sd   t1, 0(t0)                                       ;\
rvmodel_halt_fail_loop:                                ;\
  j    rvmodel_halt_fail_loop                          ;\

##### IO #####

/* No I/O device initialisation required. */
#define RVMODEL_IO_INIT(_R1, _R2, _R3)

/*
 * RVMODEL_IO_WRITE_STR — print a null-terminated string via HTIF console.
 *
 * Writes each character to tohost as a HTIF console putchar request:
 *   tohost = 0x0101_0000_0000_00CC  (CC = ASCII character byte)
 *
 * _R1      — scratch: built tohost value (char | console header)
 * _R2      — scratch: console header  (0x0101_0000_0000_0000)
 * _R3      — scratch: tohost address
 * _STR_PTR — pointer to the null-terminated string (incremented in place)
 */
#define RVMODEL_IO_WRITE_STR(_R1, _R2, _R3, _STR_PTR)       \
  li   _R2, 0x0101                                          ;\
  slli _R2, _R2, 48                                         ;\
1:                                                          ;\
  lbu  _R1, 0(_STR_PTR)                                     ;\
  beqz _R1, 2f                                              ;\
  or   _R1, _R1, _R2                                        ;\
  la   _R3, tohost                                          ;\
  sd   _R1, 0(_R3)                                          ;\
  addi _STR_PTR, _STR_PTR, 1                                ;\
  j    1b                                                   ;\
2:

##### Interrupt Latency #####

#define RVMODEL_INTERRUPT_LATENCY 10

/*
 * RVMODEL_TIMER_INT_SOON_DELAY — number of instructions to spin before a "soon"
 * timer interrupt is expected to arrive.  The simulator increments mtime every
 * instruction, so a value of 100 gives the interrupt handler enough slack.
 */
#define RVMODEL_TIMER_INT_SOON_DELAY 100

##### Timer Addresses #####

/*
 * riscv64-sim CLINT memory map (SiFive layout):
 *   0x0200BFF8  mtime     — free-running real-time counter
 *   0x02004000  mtimecmp  — per-hart timer compare (hart 0)
 */
#define RVMODEL_MTIME_ADDRESS    0x0200BFF8
#define RVMODEL_MTIMECMP_ADDRESS 0x02004000

##### Machine Interrupts #####

/*
 * riscv64-sim has no external interrupt controller; MEXT is not testable.
 * Define both SET and CLR as no-ops so the framework compiles cleanly.
 */
#define RVMODEL_SET_MEXT_INT(_R1, _R2)
#define RVMODEL_CLR_MEXT_INT(_R1, _R2)

/*
 * M-mode software interrupt via CLINT MSIP register (hart 0 at 0x02000000).
 * Write 1 to assert, write 0 to deassert.
 */
#define RVMODEL_SET_MSW_INT(_R1, _R2)   \
  li   _R1, 1                          ;\
  li   _R2, 0x02000000                 ;\
  sw   _R1, 0(_R2)                     ;

#define RVMODEL_CLR_MSW_INT(_R1, _R2)   \
  li   _R2, 0x02000000                 ;\
  sw   zero, 0(_R2)                    ;

##### Supervisor Interrupts #####

/*
 * riscv64-sim is M-mode only; supervisor-mode interrupts do not exist.
 * Define all S-mode interrupt macros as no-ops so the framework compiles.
 */
#define RVMODEL_SET_SEXT_INT(_R1, _R2)
#define RVMODEL_CLR_SEXT_INT(_R1, _R2)
#define RVMODEL_SET_SSW_INT(_R1, _R2)
#define RVMODEL_CLR_SSW_INT(_R1, _R2)

#endif /* _RVMODEL_MACROS_H */
