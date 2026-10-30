#ifndef EZHLE_EZHLE_PS2_H
#define EZHLE_EZHLE_PS2_H

#include "ezhle/ezhle.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PS/2 EE syscall ABI: number in $v1, return in $v0. */
#define EZHLE_PS2_SYSCALL_REG 3u
#define EZHLE_PS2_RETURN_REG 2u

/* PS2 ROMDIR entry: name[10], u16 extinfo_size, u32 size (little-endian). */
typedef struct ezhle_romdir_entry {
    char name[11];
    uint16_t extinfo_size;
    uint32_t size;
} ezhle_romdir_entry_t;

int ezhle_ps2_romdir_parse(const uint8_t *data, size_t size,
                           ezhle_romdir_entry_t *entries, size_t max_entries,
                           size_t *out_count);
const ezhle_romdir_entry_t *ezhle_ps2_romdir_find(
    const ezhle_romdir_entry_t *entries, size_t count, const char *name);

#ifdef __cplusplus
}
#endif

#endif
