/* Deterministic instruction-stepping test harness (FDS-CPU-004).
 * Synthetic inputs only. Steps the HLE through a fixed instruction sequence
 * twice and checks that results and error state are reproducible. */
#include "common/ezhle_internal.h"
#include "ezhle/ezhle.h"
#include "ezhle/ezhle_fds.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint8_t mem[65536];
    char log[4096];
    size_t log_len;
} fds_fake_bus_t;

static int fds_fake_read32(void *ctx, uint32_t addr, uint32_t *out)
{
    fds_fake_bus_t *bus = ctx;
    if (out == NULL || (size_t)addr + 4u > sizeof(bus->mem)) {
        return -1;
    }
    *out = (uint32_t)bus->mem[addr] | ((uint32_t)bus->mem[addr + 1u] << 8) |
           ((uint32_t)bus->mem[addr + 2u] << 16) |
           ((uint32_t)bus->mem[addr + 3u] << 24);
    return 0;
}

static int fds_fake_write32(void *ctx, uint32_t addr, uint32_t value)
{
    fds_fake_bus_t *bus = ctx;
    if ((size_t)addr + 4u > sizeof(bus->mem)) {
        return -1;
    }
    bus->mem[addr] = (uint8_t)(value & 0xffu);
    bus->mem[addr + 1u] = (uint8_t)((value >> 8) & 0xffu);
    bus->mem[addr + 2u] = (uint8_t)((value >> 16) & 0xffu);
    bus->mem[addr + 3u] = (uint8_t)((value >> 24) & 0xffu);
    return 0;
}

static int fds_fake_read_bytes(void *ctx, uint32_t addr, void *dst, size_t len)
{
    fds_fake_bus_t *bus = ctx;
    if (dst == NULL || (size_t)addr + len > sizeof(bus->mem)) {
        return -1;
    }
    memcpy(dst, bus->mem + addr, len);
    return 0;
}

static void fds_fake_log(void *ctx, int level, const char *message)
{
    fds_fake_bus_t *bus = ctx;
    size_t len;
    (void)level;
    if (message == NULL) {
        return;
    }
    len = strlen(message);
    if (bus->log_len + len + 2u >= sizeof(bus->log)) {
        return;
    }
    memcpy(bus->log + bus->log_len, message, len);
    bus->log_len += len;
    bus->log[bus->log_len++] = '\n';
    bus->log[bus->log_len] = '\0';
}

static ezhle_core_t *make_core(fds_fake_bus_t *fake, ezhle_bus_t *bus, ezhle_desc_t *desc)
{
    memset(fake, 0, sizeof(*fake));
    memset(bus, 0, sizeof(*bus));
    bus->read32 = fds_fake_read32;
    bus->write32 = fds_fake_write32;
    bus->read_bytes = fds_fake_read_bytes;
    bus->log = fds_fake_log;
    bus->ctx = fake;
    EZHLE_DESC_INIT(desc, "fds", "test", bus);
    return ezhle_create(desc);
}

int main(void)
{
    fds_fake_bus_t fake;
    ezhle_bus_t bus;
    ezhle_desc_t desc;
    ezhle_core_t *core;
    ezhle_regs_t regs;
    int failed = 0;
    unsigned round;

    core = make_core(&fake, &bus, &desc);
    if (core == NULL) {
        printf("core create failed\n");
        return 1;
    }

    for (round = 0; round < 2u; round++) {
        /* Step 1: unimplemented entry must report UNIMPL, A=$FF, error set. */
        memset(&regs, 0, sizeof(regs));
        regs.pc = 0xE153u; /* Delayms, implemented=0 */
        if (ezhle_dispatch(core, &regs) != EZHLE_ERR_UNIMPL) {
            printf("round %u: step1 rc wrong\n", round);
            failed++;
        }
        if (regs.r[0] != 0xFFu) {
            printf("round %u: step1 A wrong (%u)\n", round, regs.r[0]);
            failed++;
        }
        if (ezhle_last_error(core)[0] == '\0') {
            printf("round %u: step1 missing error\n", round);
            failed++;
        }

        /* Step 2: implemented entry must succeed and clear stale error state. */
        fake.mem[2] = 16;
        fake.mem[3] = 8;
        memset(&regs, 0, sizeof(regs));
        regs.pc = 0xE97Du; /* Pixel2NamConv */
        if (ezhle_dispatch(core, &regs) != EZHLE_OK) {
            printf("round %u: step2 rc wrong (%d)\n", round, ezhle_last_error(core)[0]);
            failed++;
        }
        if (regs.r[0] != 0u) {
            printf("round %u: step2 A wrong (%u)\n", round, regs.r[0]);
            failed++;
        }
        if (fake.mem[0] != 0x20 || fake.mem[1] != 0x41) {
            printf("round %u: step2 nam addr wrong\n", round);
            failed++;
        }
        if (ezhle_last_error(core)[0] != '\0') {
            printf("round %u: stale error leaked: %s\n", round, ezhle_last_error(core));
            failed++;
        }
    }

    ezhle_destroy(core);
    if (failed != 0) {
        printf("%d failure(s)\n", failed);
        return 1;
    }
    printf("fds_cpu_004: PASS\n");
    return 0;
}
