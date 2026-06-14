OUTPUT_ARCH("riscv")
ENTRY(rvtest_entry_point)

/* riscv64-sim memory map for arch-tests:
 *   RAM:  0x80000000  64 MB  (code + data + signature)
 *   HTIF: 0x83FFF000   4 KB  (tohost / fromhost at top of 64 MB region)
 *
 * tohost and fromhost are resolved by the simulator via ELF symbol lookup.
 * All writes to the tohost address are intercepted by the HTIF handler.
 *
 * The 64 MB test region is well within the simulator's default 128 MB RAM.
 */
SECTIONS
{
  /* ── Code (RAM base) ────────────────────────────────────────────────────── */
  . = 0x80000000;
  .text.init : { *(.text.init) }
  .text      : { *(.text) *(.text.*) }

  /* ── Read-only data ──────────────────────────────────────────────────────── */
  . = ALIGN(0x1000);
  .rodata : { *(.rodata) *(.rodata.*) }

  /* ── Initialised data ────────────────────────────────────────────────────── */
  . = ALIGN(0x1000);
  .data : { *(.data) *(.data.*) }

  /* ── Zero-initialised data (signature lives here) ────────────────────────── */
  . = ALIGN(0x1000);
  .bss (NOLOAD) : {
    *(.bss) *(.bss.*)
    *(COMMON)
  }

  . = ALIGN(0x1000);
  _end = .;

  /* ── HTIF region — tohost/fromhost at top of 64 MB ──────────────────────── */
  . = 0x80000000 + 0x4000000 - 0x1000;   /* 0x83FFF000 */
  .htif (NOLOAD) : {
    PROVIDE(tohost   = .);
    . += 8;
    PROVIDE(fromhost = .);
    . += 8;
  }
}
