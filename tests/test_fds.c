#include "common/ezhle_internal.h"
#include "ezhle/ezhle.h"
#include "ezhle/ezhle_fds.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint8_t mem[65536];
    char log[4096];
    size_t log_len;
    unsigned trace_count;
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

static void fds_fake_trace(void *ctx, uint32_t nr, const uint32_t *args, size_t argc, int32_t ret)
{
    fds_fake_bus_t *bus = ctx;
    (void)nr; (void)args; (void)argc; (void)ret;
    bus->trace_count++;
}

static ezhle_core_t *fds_make_core(fds_fake_bus_t *fake, ezhle_bus_t *bus, ezhle_desc_t *desc)
{
    memset(fake, 0, sizeof(*fake));
    memset(bus, 0, sizeof(*bus));
    bus->read32 = fds_fake_read32;
    bus->write32 = fds_fake_write32;
    bus->read_bytes = fds_fake_read_bytes;
    bus->log = fds_fake_log;
    bus->trace = fds_fake_trace;
    bus->ctx = fake;

    EZHLE_DESC_INIT(desc, "fds", "test", bus);
    return ezhle_create(desc);
}

static int test_create_destroy(void)
{
    fds_fake_bus_t fake;
    ezhle_bus_t bus;
    ezhle_desc_t desc;
    ezhle_core_t *core = fds_make_core(&fake, &bus, &desc);
    if (core == NULL) {
        printf("  create failed\n");
        return 1;
    }
    if (strcmp(ezhle_system(core), "fds") != 0) {
        printf("  wrong system\n");
        ezhle_destroy(core);
        return 1;
    }
    ezhle_reset(core);
    ezhle_destroy(core);
    return 0;
}

static int test_reset_vector(void)
{
    fds_fake_bus_t fake;
    ezhle_bus_t bus;
    ezhle_desc_t desc;
    ezhle_core_t *core;
    uint16_t rv;

    memset(&fake, 0, sizeof(fake));
    /* reset vector at $FFFC/$FFFD = 0x8234 (little-endian) */
    fake.mem[0xFFFC] = 0x34;
    fake.mem[0xFFFD] = 0x82;
    memset(&bus, 0, sizeof(bus));
    bus.read32 = fds_fake_read32;
    bus.write32 = fds_fake_write32;
    bus.read_bytes = fds_fake_read_bytes;
    bus.log = fds_fake_log;
    bus.trace = fds_fake_trace;
    bus.ctx = &fake;
    EZHLE_DESC_INIT(&desc, "fds", "test", &bus);
    core = ezhle_create(&desc);
    if (core == NULL) {
        return 1;
    }
    rv = ezhle_fds_reset_vector(core);
    if (rv != 0x8234u) {
        printf("  reset vector got 0x%04x want 0x8234\n", rv);
        ezhle_destroy(core);
        return 1;
    }
    ezhle_reset(core);
    if (ezhle_fds_reset_vector(core) != 0x8234u) {
        printf("  reset vector changed after reset\n");
        ezhle_destroy(core);
        return 1;
    }
    ezhle_destroy(core);
    return 0;
}

static int test_implemented_entry(void)
{
    fds_fake_bus_t fake;
    ezhle_bus_t bus;
    ezhle_desc_t desc;
    ezhle_core_t *core = fds_make_core(&fake, &bus, &desc);
    ezhle_regs_t regs;
    int rc;

    if (core == NULL) {
        return 1;
    }
    /* Pixel2NamConv: pixel (Y=16, X=8) -> tile (2,1) -> 0x2041 */
    fake.mem[2] = 16;
    fake.mem[3] = 8;
    memset(&regs, 0, sizeof(regs));
    regs.pc = 0xE97D;
    rc = ezhle_dispatch(core, &regs);
    if (rc != EZHLE_OK) {
        printf("  dispatch rc %d\n", rc);
        ezhle_destroy(core);
        return 1;
    }
    if (regs.r[0] != 0u) {
        printf("  A=%u\n", regs.r[0]);
        ezhle_destroy(core);
        return 1;
    }
    if (fake.mem[0] != 0x20 || fake.mem[1] != 0x41) {
        printf("  nam addr %02x%02x\n", fake.mem[0], fake.mem[1]);
        ezhle_destroy(core);
        return 1;
    }
    /* Delay131 no-op returns 0 */
    memset(&regs, 0, sizeof(regs));
    regs.pc = 0xE149;
    rc = ezhle_dispatch(core, &regs);
    if (rc != EZHLE_OK || regs.r[0] != 0u) {
        printf("  delay131 rc %d A %u\n", rc, regs.r[0]);
        ezhle_destroy(core);
        return 1;
    }
    ezhle_destroy(core);
    return 0;
}

static int test_unknown_entry(void)
{
    fds_fake_bus_t fake;
    ezhle_bus_t bus;
    ezhle_desc_t desc;
    ezhle_core_t *core = fds_make_core(&fake, &bus, &desc);
    ezhle_regs_t regs;
    int rc;

    if (core == NULL) {
        return 1;
    }
    memset(&regs, 0, sizeof(regs));
    regs.pc = 0xE000; /* not in table */
    rc = ezhle_dispatch(core, &regs);
    if (rc != EZHLE_ERR_UNIMPL) {
        printf("  rc %d\n", rc);
        ezhle_destroy(core);
        return 1;
    }
    if (regs.r[0] != 0xFFu) {
        printf("  A %u\n", regs.r[0]);
        ezhle_destroy(core);
        return 1;
    }
    if (ezhle_last_error(core)[0] == '\0') {
        printf("  no error msg\n");
        ezhle_destroy(core);
        return 1;
    }
    if (fake.log_len == 0) {
        printf("  log not set\n");
        ezhle_destroy(core);
        return 1;
    }
    ezhle_destroy(core);
    return 0;
}

static int test_fds_call_helper(void)
{
    fds_fake_bus_t fake;
    ezhle_bus_t bus;
    ezhle_desc_t desc;
    ezhle_core_t *core = fds_make_core(&fake, &bus, &desc);
    ezhle_regs_t regs;
    if (core == NULL) return 1;
    memset(&regs, 0, sizeof(regs));
    if (ezhle_fds_call(core, &regs, 0xE149) != EZHLE_OK) {
        ezhle_destroy(core);
        return 1;
    }
    ezhle_destroy(core);
    return 0;
}

static int test_entry_table(void)
{
    size_t n = ezhle_fds_entry_count();
    const ezhle_fds_entry_t *t = ezhle_fds_entry_table();
    if (n == 0 || t == NULL) {
        printf("  empty table\n");
        return 1;
    }
    if (ezhle_fds_entry_find(0xE97D) == NULL) {
        printf("  find failed\n");
        return 1;
    }
    if (ezhle_fds_entry_find(0x1234) != NULL) {
        printf("  false find\n");
        return 1;
    }
    return 0;
}

static int fail_read32(void *ctx, uint32_t addr, uint32_t *out)
{
    (void)ctx; (void)addr; (void)out;
    return -1;
}

static int fail_read_bytes(void *ctx, uint32_t addr, void *dst, size_t len)
{
    (void)ctx; (void)addr; (void)dst; (void)len;
    return -1;
}

static int fail_write32(void *ctx, uint32_t addr, uint32_t value)
{
    (void)ctx; (void)addr; (void)value;
    return -1;
}

static int test_null_guards(void)
{
    /* reset_vector(NULL) must not UB and returns 0 */
    if (ezhle_fds_reset_vector(NULL) != 0u) {
        printf("  reset_vector null\\n");
        return 1;
    }
    /* entry_find is pure; call/attach handled below */
    if (ezhle_fds_entry_find(0x1234) != NULL) {
        printf("  entry_find false positive\\n");
        return 1;
    }
    return 0;
}

static int test_double_attach(void)
{
    fds_fake_bus_t fake;
    ezhle_bus_t bus;
    ezhle_desc_t desc;
    ezhle_core_t *core;

    core = fds_make_core(&fake, &bus, &desc);
    if (core == NULL) return 1;
    /* second attach must not leak/UB */
    if (ezhle_fds_attach(core) != EZHLE_OK) {
        printf("  second attach failed\\n");
        ezhle_destroy(core);
        return 1;
    }
    ezhle_reset(core);
    ezhle_destroy(core);
    return 0;
}

static int test_reset_vector_live(void)
{
    fds_fake_bus_t fake;
    ezhle_bus_t bus;
    ezhle_desc_t desc;
    ezhle_core_t *core;
    uint16_t rv;

    memset(&fake, 0, sizeof(fake));
    memset(&bus, 0, sizeof(bus));
    bus.read32 = fds_fake_read32;
    bus.write32 = fds_fake_write32;
    bus.read_bytes = fds_fake_read_bytes;
    bus.log = fds_fake_log;
    bus.trace = fds_fake_trace;
    bus.ctx = &fake;
    EZHLE_DESC_INIT(&desc, "fds", "test", &bus);
    core = ezhle_create(&desc);
    if (core == NULL) return 1;
    /* map vector after create, without reset */
    fake.mem[0xFFFC] = 0x56;
    fake.mem[0xFFFD] = 0x78;
    rv = ezhle_fds_reset_vector(core);
    if (rv != 0x7856u) {
        printf("  live vector got 0x%04x want 0x7856\\n", rv);
        ezhle_destroy(core);
        return 1;
    }
    ezhle_destroy(core);
    return 0;
}

static int test_errprop_read(void)
{
    fds_fake_bus_t fake;
    ezhle_bus_t bus;
    ezhle_desc_t desc;
    ezhle_core_t *core;
    ezhle_regs_t regs;
    int rc;

    memset(&fake, 0, sizeof(fake));
    memset(&bus, 0, sizeof(bus));
    bus.read32 = fail_read32;
    bus.write32 = fds_fake_write32;
    bus.read_bytes = fail_read_bytes;
    bus.log = fds_fake_log;
    bus.trace = fds_fake_trace;
    bus.ctx = &fake;
    EZHLE_DESC_INIT(&desc, "fds", "test", &bus);
    core = ezhle_create(&desc);
    if (core == NULL) return 1;
    /* Pixel2NamConv path */
    memset(&regs, 0, sizeof(regs));
    regs.pc = 0xE97D;
    rc = ezhle_dispatch(core, &regs);
    if (rc >= 0) {
        printf("  rc not error: %d\\n", rc);
        ezhle_destroy(core);
        return 1;
    }
    if (regs.r[0] > 0xFFu) {
        printf("  A out of 8-bit: %u\\n", regs.r[0]);
        ezhle_destroy(core);
        return 1;
    }
    if (ezhle_last_error(core)[0] == '\0') {
        printf("  no last_error\\n");
        ezhle_destroy(core);
        return 1;
    }
    if (fake.log_len == 0) {
        printf("  no log\\n");
        ezhle_destroy(core);
        return 1;
    }
    ezhle_destroy(core);
    return 0;
}

static int test_errprop_write(void)
{
    fds_fake_bus_t fake;
    ezhle_bus_t bus;
    ezhle_desc_t desc;
    ezhle_core_t *core;
    ezhle_regs_t regs;
    int rc;

    /* read ok, write fails */
    memset(&fake, 0, sizeof(fake));
    fake.mem[2] = 16; fake.mem[3] = 8;
    memset(&bus, 0, sizeof(bus));
    bus.read32 = fds_fake_read32;
    bus.write32 = fail_write32;
    bus.read_bytes = fds_fake_read_bytes;
    bus.log = fds_fake_log;
    bus.trace = fds_fake_trace;
    bus.ctx = &fake;
    EZHLE_DESC_INIT(&desc, "fds", "test", &bus);
    core = ezhle_create(&desc);
    if (core == NULL) return 1;
    memset(&regs, 0, sizeof(regs));
    regs.pc = 0xE97D;
    rc = ezhle_dispatch(core, &regs);
    if (rc >= 0) {
        printf("  rc not error: %d\\n", rc);
        ezhle_destroy(core);
        return 1;
    }
    if (regs.r[0] > 0xFFu) {
        printf("  A out of 8-bit: %u\\n", regs.r[0]);
        ezhle_destroy(core);
        return 1;
    }
    if (ezhle_last_error(core)[0] == '\0') {
        printf("  no last_error\\n");
        ezhle_destroy(core);
        return 1;
    }
    ezhle_destroy(core);
    return 0;
}

static int test_range_nam2pixel(void)
{
    fds_fake_bus_t fake;
    ezhle_bus_t bus;
    ezhle_desc_t desc;
    ezhle_core_t *core;
    ezhle_regs_t regs;
    int rc;
    uint16_t addrs[] = { 0x1FFFu, 0x2000u, 0x23FFu, 0x2400u, 0xFFFFu };
    /* expected: only 0x2000 and 0x23FF succeed */
    for (size_t k = 0; k < 5; k++) {
        memset(&fake, 0, sizeof(fake));
        memset(&bus, 0, sizeof(bus));
        /* rebuild bus each iteration not needed; reuse fds_make_core */
    }
    for (size_t k = 0; k < 5; k++) {
        core = fds_make_core(&fake, &bus, &desc);
        if (core == NULL) return 1;
        fake.mem[0] = (uint8_t)((addrs[k] >> 8) & 0xffu);
        fake.mem[1] = (uint8_t)(addrs[k] & 0xffu);
        memset(&regs, 0, sizeof(regs));
        regs.pc = 0xE997;
        rc = ezhle_dispatch(core, &regs);
        if (addrs[k] == 0x2000u || addrs[k] == 0x23FFu) {
            if (rc != EZHLE_OK) {
                printf("  addr %04x expected ok got %d\\n", addrs[k], rc);
                ezhle_destroy(core);
                return 1;
            }
        } else {
            if (rc >= 0) {
                printf("  addr %04x expected error got %d\\n", addrs[k], rc);
                ezhle_destroy(core);
                return 1;
            }
        }
        ezhle_destroy(core);
    }
    return 0;
}

static int test_nam2pixel_cov(void)
{
    fds_fake_bus_t fake;
    ezhle_bus_t bus;
    ezhle_desc_t desc;
    ezhle_core_t *core;
    ezhle_regs_t regs;

    /* tile 0 -> (0,0) */
    core = fds_make_core(&fake, &bus, &desc);
    if (core == NULL) return 1;
    fake.mem[0] = 0x20; fake.mem[1] = 0x00;
    memset(&regs, 0, sizeof(regs));
    regs.pc = 0xE997;
    if (ezhle_dispatch(core, &regs) != EZHLE_OK) { ezhle_destroy(core); return 1; }
    if (fake.mem[2] != 0 || fake.mem[3] != 0) {
        printf("  tile0 yx %u %u\\n", fake.mem[2], fake.mem[3]);
        ezhle_destroy(core); return 1;
    }
    ezhle_destroy(core);

    /* tile 0x41 / $2041 -> (16,8) */
    core = fds_make_core(&fake, &bus, &desc);
    if (core == NULL) return 1;
    fake.mem[0] = 0x20; fake.mem[1] = 0x41;
    memset(&regs, 0, sizeof(regs));
    regs.pc = 0xE997;
    if (ezhle_dispatch(core, &regs) != EZHLE_OK) { ezhle_destroy(core); return 1; }
    if (fake.mem[2] != 16 || fake.mem[3] != 8) {
        printf("  tile41 yx %u %u\\n", fake.mem[2], fake.mem[3]);
        ezhle_destroy(core); return 1;
    }
    ezhle_destroy(core);

    /* tile 0x3FF / $23FF -> (248,248) */
    core = fds_make_core(&fake, &bus, &desc);
    if (core == NULL) return 1;
    fake.mem[0] = 0x23; fake.mem[1] = 0xFF;
    memset(&regs, 0, sizeof(regs));
    regs.pc = 0xE997;
    if (ezhle_dispatch(core, &regs) != EZHLE_OK) { ezhle_destroy(core); return 1; }
    if (fake.mem[2] != 248 || fake.mem[3] != 248) {
        printf("  tile3ff yx %u %u\\n", fake.mem[2], fake.mem[3]);
        ezhle_destroy(core); return 1;
    }
    ezhle_destroy(core);
    return 0;
}

typedef int (*test_fn_t)(void);

static int run(const char *name, test_fn_t fn)
{
    int rc = fn();
    printf("%-24s %s\n", name, rc == 0 ? "PASS" : "FAIL");
    return rc;
}

int main(void)
{
    int failed = 0;
    printf("ezhle fds tests\n");
    failed += run("create_destroy", test_create_destroy);
    failed += run("reset_vector", test_reset_vector);
    failed += run("implemented_entry", test_implemented_entry);
    failed += run("unknown_entry", test_unknown_entry);
    failed += run("fds_call", test_fds_call_helper);
    failed += run("entry_table", test_entry_table);
    failed += run("null_guards", test_null_guards);
    failed += run("double_attach", test_double_attach);
    failed += run("reset_vector_live", test_reset_vector_live);
    failed += run("errprop_read", test_errprop_read);
    failed += run("errprop_write", test_errprop_write);
    failed += run("range_nam2pixel", test_range_nam2pixel);
    failed += run("nam2pixel_cov", test_nam2pixel_cov);

    if (failed != 0) {
        printf("\n%d test(s) failed\n", failed);
        return 1;
    }
    printf("\nall fds tests passed\n");
    return 0;
}