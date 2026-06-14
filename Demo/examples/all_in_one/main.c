#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../common/platform.h"

enum {
    kExpectedHarts = 2,
    kSpinIterations = 1000,
    kStageBoot = 0,
    kStageSpin = 1,
    kStageIpi = 2,
    kStageDone = 3
};

static xv_spinlock_t g_spinlock;
static volatile uint32_t g_stage;
static volatile uint32_t g_hart_online_mask;
static volatile uint32_t g_spin_done_mask;
static volatile uint32_t g_shared_counter;
static volatile uint32_t g_ipi_seen_mask;
static volatile uint32_t g_timer_fired;

void machine_trap_handler(void) __attribute__((interrupt("machine"), aligned(4)));

static void set_stage(uint32_t stage) {
    xv_fence();
    g_stage = stage;
    xv_fence();
}

static bool wait_for_mask(volatile uint32_t *addr, uint32_t mask, uint64_t timeout_ticks) {
    uint64_t deadline = xv_read_mtime() + timeout_ticks;
    while ((*addr & mask) != mask) {
        if (xv_read_mtime() >= deadline) {
            return false;
        }
    }
    return true;
}

static void spinlock_worker(uint32_t hart_bit) {
    for (int i = 0; i < kSpinIterations; ++i) {
        xv_spin_lock(&g_spinlock);
        ++g_shared_counter;
        xv_spin_unlock(&g_spinlock);
    }
    xv_atomic_fetch_add_u32(&g_spin_done_mask, hart_bit);
}

__attribute__((aligned(4))) void machine_trap_handler(void) {
    uint64_t hart = xv_read_mhartid();
    uint64_t cause = xv_read_mcause();

    if (cause == (XV_MCAUSE_INTERRUPT | XV_MCAUSE_MSIP)) {
        XV_CLINT_MSIP(hart) = 0;
        xv_atomic_fetch_add_u32(&g_ipi_seen_mask, (uint32_t)(1u << hart));
        return;
    }

    if (cause == (XV_MCAUSE_INTERRUPT | XV_MCAUSE_MTIP)) {
        XV_CLINT_MTIMECMP(hart) = UINT64_MAX;
        g_timer_fired = 1;
        return;
    }

    for (;;) {
    }
}

static int secondary_main(void) {
    const uint32_t hart_bit = 1u << 1;

    xv_write_mtvec(machine_trap_handler);
    xv_enable_interrupts(XV_MIE_MSIE);
    xv_atomic_fetch_add_u32(&g_hart_online_mask, hart_bit);

    while (g_stage == kStageBoot) {
    }

    spinlock_worker(hart_bit);

    while (g_stage == kStageSpin) {
    }

    while (g_stage == kStageIpi && (g_ipi_seen_mask & hart_bit) == 0u) {
        xv_wfi();
    }

    while (g_stage != kStageDone) {
        xv_wfi();
    }

    xv_park_forever();
    return 0;
}

static int primary_main(void) {
    static const char file_path[] = "all_in_one_demo.txt";
    static const char file_message[] = "all_in_one: stdio file I/O verified\n";
    char buffer[sizeof(file_message)] = {0};
    FILE *fp;
    size_t bytes;

    xv_write_mtvec(machine_trap_handler);
    xv_atomic_fetch_add_u32(&g_hart_online_mask, 1u);

    printf("all_in_one: hart0 online, waiting for hart1\n");
    if (!wait_for_mask(&g_hart_online_mask, 0x3u, 500000u)) {
        printf("all_in_one: timeout waiting for secondary hart, mask=0x%x\n", g_hart_online_mask);
        return 1;
    }
    printf("SMP: both harts are running, mask=0x%x\n", g_hart_online_mask);

    set_stage(kStageSpin);
    spinlock_worker(1u);
    if (!wait_for_mask(&g_spin_done_mask, 0x3u, 500000u)) {
        printf("spinlock: timeout waiting for completion, done=0x%x counter=%u\n",
               g_spin_done_mask,
               g_shared_counter);
        return 1;
    }
    if (g_shared_counter != (uint32_t)(kExpectedHarts * kSpinIterations)) {
        printf("spinlock: counter mismatch expected=%u actual=%u\n",
               kExpectedHarts * kSpinIterations,
               g_shared_counter);
        return 1;
    }
    printf("spinlock: shared counter=%u\n", g_shared_counter);

    set_stage(kStageIpi);
    xv_enable_interrupts(XV_MIE_MSIE | XV_MIE_MTIE);
    XV_CLINT_MSIP(1) = 1;
    if (!wait_for_mask(&g_ipi_seen_mask, 0x2u, 500000u)) {
        printf("IPI: secondary hart did not acknowledge software interrupt, mask=0x%x\n", g_ipi_seen_mask);
        return 1;
    }
    printf("IPI: hart1 received software interrupt\n");

    fp = fopen(file_path, "w+");
    if (fp == NULL) {
        printf("FILE I/O: fopen failed\n");
        return 1;
    }
    bytes = fwrite(file_message, 1u, sizeof(file_message) - 1u, fp);
    if (bytes != sizeof(file_message) - 1u) {
        printf("FILE I/O: fwrite wrote %zu bytes\n", bytes);
        fclose(fp);
        return 1;
    }
    fflush(fp);
    if (fseek(fp, 0, SEEK_SET) != 0) {
        printf("FILE I/O: fseek failed\n");
        fclose(fp);
        return 1;
    }
    bytes = fread(buffer, 1u, sizeof(buffer) - 1u, fp);
    if (bytes != sizeof(file_message) - 1u || memcmp(buffer, file_message, sizeof(file_message) - 1u) != 0) {
        printf("FILE I/O: fread/compare failed, bytes=%zu\n", bytes);
        fclose(fp);
        return 1;
    }
    fclose(fp);
    remove(file_path);
    printf("FILE I/O: fopen/fwrite/fread succeeded\n");

    g_timer_fired = 0u;
    XV_CLINT_MTIMECMP(0) = xv_read_mtime() + 512u;
    printf("WFI: waiting for timer interrupt\n");
    while (g_timer_fired == 0u) {
        xv_wfi();
    }
    printf("WFI: woke on timer interrupt\n");

    set_stage(kStageDone);
    printf("all_in_one: all requested tests passed\n");
    return 0;
}

int main(void) {
    uint64_t hart = xv_read_mhartid();

    if (hart == 0) {
        return primary_main();
    }
    if (hart == 1) {
        return secondary_main();
    }

    xv_park_forever();
}