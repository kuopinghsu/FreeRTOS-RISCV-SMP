#ifndef __PLATFORM_H
#define __PLATFORM_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

#define XV_CLINT_BASE 0x02000000ull
#define XV_MSTATUS_MIE (1ull << 3)
#define XV_MIE_MSIE (1ull << 3)
#define XV_MIE_MTIE (1ull << 7)
#define XV_MCAUSE_INTERRUPT (1ull << 63)
#define XV_MCAUSE_MSIP 3ull
#define XV_MCAUSE_MTIP 7ull

#define XV_CLINT_MSIP(hart) \
    (*(volatile uint32_t *)(uintptr_t)(XV_CLINT_BASE + 4ull * (uint64_t)(hart)))
#define XV_CLINT_MTIMECMP(hart) \
    (*(volatile uint64_t *)(uintptr_t)(XV_CLINT_BASE + 0x4000ull + 8ull * (uint64_t)(hart)))
#define XV_CLINT_MTIME \
    (*(volatile uint64_t *)(uintptr_t)(XV_CLINT_BASE + 0xBFF8ull))

typedef struct {
    volatile uint32_t value;
} xv_spinlock_t;

int *__errno(void);

int _open(const char *path, int flags, int mode);
int _close(int fd);
ssize_t _read(int fd, void *buf, size_t len);
ssize_t _write(int fd, const void *buf, size_t len);
off_t _lseek(int fd, off_t offset, int whence);
int _unlink(const char *path);

static inline uint64_t xv_read_mhartid(void) {
    uint64_t value;
    __asm__ volatile("csrr %0, mhartid" : "=r"(value));
    return value;
}

static inline uint64_t xv_read_mcause(void) {
    uint64_t value;
    __asm__ volatile("csrr %0, mcause" : "=r"(value));
    return value;
}

static inline void xv_write_mtvec(void (*handler)(void)) {
    uintptr_t value = (uintptr_t)handler;
    __asm__ volatile("csrw mtvec, %0" : : "r"(value) : "memory");
}

static inline void xv_enable_interrupts(uint64_t mask) {
    __asm__ volatile("csrs mie, %0" : : "r"(mask) : "memory");
    __asm__ volatile("csrs mstatus, %0" : : "r"(XV_MSTATUS_MIE) : "memory");
}

static inline void xv_disable_interrupts(void) {
    __asm__ volatile("csrc mstatus, %0" : : "r"(XV_MSTATUS_MIE) : "memory");
}

static inline uint64_t xv_read_mtime(void) {
    return XV_CLINT_MTIME;
}

static inline void xv_wfi(void) {
    __asm__ volatile("wfi" : : : "memory");
}

static inline void xv_fence(void) {
    __asm__ volatile("fence rw, rw" : : : "memory");
}

static inline uint32_t xv_atomic_fetch_add_u32(volatile uint32_t *addr, uint32_t value) {
    uint32_t previous;
    __asm__ volatile("amoadd.w.aq %0, %2, (%1)"
                     : "=r"(previous)
                     : "r"(addr), "r"(value)
                     : "memory");
    return previous;
}

static inline uint32_t xv_atomic_swap_u32(volatile uint32_t *addr, uint32_t value) {
    uint32_t previous;
    __asm__ volatile("amoswap.w.aq %0, %2, (%1)"
                     : "=r"(previous)
                     : "r"(addr), "r"(value)
                     : "memory");
    return previous;
}

static inline void xv_spin_lock(xv_spinlock_t *lock) {
    while (xv_atomic_swap_u32(&lock->value, 1u) != 0u) {
        while (lock->value != 0u) {
        }
    }
}

static inline void xv_spin_unlock(xv_spinlock_t *lock) {
    xv_fence();
    lock->value = 0;
}

static inline void xv_park_forever(void) {
    for (;;) {
        xv_wfi();
    }
}

static inline bool xv_wait_for_value(volatile uint32_t *addr,
                                     uint32_t expected,
                                     uint64_t timeout_ticks) {
    uint64_t deadline = xv_read_mtime() + timeout_ticks;
    while (*addr != expected) {
        if (xv_read_mtime() >= deadline) {
            return false;
        }
    }
    return true;
}

static inline size_t xv_strlen(const char *text) {
    size_t length = 0;
    while (text[length] != '\0') {
        ++length;
    }
    return length;
}

static inline int xv_memcmp(const void *lhs, const void *rhs, size_t len) {
    const unsigned char *left = (const unsigned char *)lhs;
    const unsigned char *right = (const unsigned char *)rhs;
    for (size_t i = 0; i < len; ++i) {
        if (left[i] != right[i]) {
            return (int)left[i] - (int)right[i];
        }
    }
    return 0;
}

static inline int xv_errno(void) {
    return *__errno();
}

static inline void xv_write_buf(const char *buf, size_t len) {
    while (len != 0u) {
        ssize_t written = _write(1, buf, len);
        if (written <= 0) {
            return;
        }
        buf += written;
        len -= (size_t)written;
    }
}

static inline void xv_write_text(const char *text) {
    xv_write_buf(text, xv_strlen(text));
}

static inline void xv_write_dec_u64(uint64_t value) {
    char buffer[21];
    size_t pos = sizeof(buffer);

    do {
        buffer[--pos] = (char)('0' + (value % 10u));
        value /= 10u;
    } while (value != 0u);

    xv_write_buf(&buffer[pos], sizeof(buffer) - pos);
}

static inline void xv_write_hex_u64(uint64_t value) {
    static const char digits[] = "0123456789abcdef";
    char buffer[18];
    buffer[0] = '0';
    buffer[1] = 'x';
    for (size_t i = 0; i < 16; ++i) {
        unsigned shift = (unsigned)((15u - i) * 4u);
        buffer[2 + i] = digits[(value >> shift) & 0xFu];
    }
    xv_write_buf(buffer, sizeof(buffer));
}

#endif // __PLATFORM_H