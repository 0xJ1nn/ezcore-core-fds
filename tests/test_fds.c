#include "common/ezhle_internal.h"
#include "ezhle/ezhle.h"
#include "ezhle/ezhle_fds.h"
#include "ezhle/ezhle_fds_image.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
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

static void put16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)(v >> 8);
}

static uint8_t *make_attach_image(size_t *out_len)
{
    const size_t side_bytes = 65500u;
    const size_t header_bytes = 16u;
    size_t total = header_bytes + side_bytes;
    uint8_t *img = (uint8_t *)calloc(1, total);
    size_t pos;
    uint8_t *side;

    if (img == NULL) {
        return NULL;
    }
    img[0] = 'F'; img[1] = 'D'; img[2] = 'S'; img[3] = 0x1A; img[4] = 1;
    side = img + header_bytes;
    pos = 0;
    side[pos] = 0x01;
    memcpy(side + pos + 1, "*NINTENDO-HVC*", 14);
    side[pos + 0x19] = 0x02;
    pos += 0x38;
    side[pos] = 0x02;
    side[pos + 1] = 1;
    pos += 2;
    side[pos] = 0x03;
    side[pos + 1] = 0;
    side[pos + 2] = 1;
    memcpy(side + pos + 3, "TEST    ", 8);
    put16(side + pos + 0x0B, 0x6000u);
    put16(side + pos + 0x0D, 4u);
    side[pos + 0x0F] = 0u;
    pos += 0x10;
    side[pos] = 0x04;
    memcpy(side + pos + 1, "ABCD", 4);
    *out_len = total;
    return img;
}

/* ---- FDS-106: file-count services $E484 GetNumFiles / $E492 SetNumFiles ----
 *
 * Documented contract (https://www.nesdev.org/wiki/FDS_BIOS, "Low-Level Disk
 * access routines"):
 *   $E484 GetNumFiles  no input;            $06 = # of files (read from the
 *                                           file amount block)
 *   $E492 SetNumFiles  A = # of files;      no output (writes A into the file
 *                                           amount block)
 */

#define FDS_TEST_SIDE_BYTES 65500u
#define FDS_TEST_HEADER_BYTES 16u
#define FDS_TEST_ZP_NUM_FILES 6u
#define FDS_TEST_GET_NUM_FILES 0xE484u
#define FDS_TEST_SET_NUM_FILES 0xE492u

/* Build one side: disk info block (0x01, 0x38 bytes), file amount block
 * (0x02, count), then `files` file header blocks (0x03) each followed by its
 * data block (0x04). Remaining side bytes stay zero (accepted padding). */
static void build_side_files(uint8_t *side, unsigned files, uint8_t first_id)
{
    size_t pos = 0;
    unsigned i;

    memset(side, 0, FDS_TEST_SIDE_BYTES);
    side[pos] = 0x01;
    memcpy(side + pos + 1, "*NINTENDO-HVC*", 14);
    side[pos + 0x0F] = 0x01;       /* licensee */
    memcpy(side + pos + 0x10, "TST", 3);
    side[pos + 0x13] = 0x20;       /* normal disk */
    side[pos + 0x14] = 0x00;       /* version */
    side[pos + 0x15] = 0x00;       /* side */
    side[pos + 0x16] = 0x00;       /* disk number */
    side[pos + 0x17] = 0x00;       /* disk type */
    side[pos + 0x18] = 0x00;
    side[pos + 0x19] = 0x02;       /* boot read file code */
    pos += 0x38;

    side[pos] = 0x02;
    side[pos + 1] = (uint8_t)files;
    pos += 2;

    for (i = 0; i < files; i++) {
        side[pos] = 0x03;
        side[pos + 1] = (uint8_t)i;                  /* file number */
        side[pos + 2] = (uint8_t)(first_id + i);     /* file ID */
        memcpy(side + pos + 3, "TEST    ", 8);
        put16(side + pos + 0x0B, (uint16_t)(0x6000u + i * 0x10u));
        put16(side + pos + 0x0D, 4u);
        side[pos + 0x0F] = 0x00;                     /* program */
        pos += 0x10;

        side[pos] = 0x04;
        memcpy(side + pos + 1, "ABCD", 4);
        pos += 5;
    }
}

/* fwNES-headed image: side 0 holds `first_side_files` files, then
 * `extra_sides` further sides holding one file each. */
static uint8_t *make_numfiles_image(unsigned first_side_files, unsigned extra_sides, size_t *out_len)
{
    size_t sides = 1u + (size_t)extra_sides;
    size_t total = FDS_TEST_HEADER_BYTES + sides * FDS_TEST_SIDE_BYTES;
    uint8_t *img = (uint8_t *)calloc(1, total);
    unsigned s;

    if (img == NULL) {
        return NULL;
    }
    img[0] = 'F';
    img[1] = 'D';
    img[2] = 'S';
    img[3] = 0x1A;
    img[4] = (uint8_t)sides;
    build_side_files(img + FDS_TEST_HEADER_BYTES, first_side_files, 1u);
    for (s = 1u; s <= extra_sides; s++) {
        build_side_files(img + FDS_TEST_HEADER_BYTES + (size_t)s * FDS_TEST_SIDE_BYTES,
                         1u, (uint8_t)(1u + s));
    }
    *out_len = total;
    return img;
}

/* Positive: two-file image, $06 = 2 and no other zero-page byte touched. */
static int test_get_num_files(void)
{
    fds_fake_bus_t fake;
    ezhle_bus_t bus;
    ezhle_desc_t desc;
    ezhle_core_t *core = fds_make_core(&fake, &bus, &desc);
    ezhle_regs_t regs;
    uint8_t *img;
    size_t len = 0u;
    int rc;

    if (core == NULL) {
        return 1;
    }
    img = make_numfiles_image(2u, 0u, &len);
    if (img == NULL) {
        ezhle_destroy(core);
        return 1;
    }
    if (ezhle_fds_attach_disk(core, img, len) != EZHLE_OK) {
        printf("  attach failed\n");
        free(img);
        ezhle_destroy(core);
        return 1;
    }
    /* the parser copies: the caller's buffer may go away now */
    free(img);

    fake.mem[FDS_TEST_ZP_NUM_FILES] = 0xEEu;   /* sentinel, must be overwritten */
    fake.mem[FDS_TEST_ZP_NUM_FILES + 1u] = 0x5Au; /* neighbour, must not move */
    memset(&regs, 0, sizeof(regs));
    rc = ezhle_fds_call(core, &regs, FDS_TEST_GET_NUM_FILES);
    if (rc != EZHLE_OK) {
        printf("  get rc %d\n", rc);
        ezhle_destroy(core);
        return 1;
    }
    if (fake.mem[FDS_TEST_ZP_NUM_FILES] != 2u) {
        printf("  $06 = %u, want 2\n", fake.mem[FDS_TEST_ZP_NUM_FILES]);
        ezhle_destroy(core);
        return 1;
    }
    if (fake.mem[FDS_TEST_ZP_NUM_FILES + 1u] != 0x5Au) {
        printf("  $07 = %u, want 0x5A\n", fake.mem[FDS_TEST_ZP_NUM_FILES + 1u]);
        ezhle_destroy(core);
        return 1;
    }
    if (regs.r[0] != 0u) {
        printf("  A = %u after GetNumFiles, want 0 (success convention)\n", regs.r[0]);
        ezhle_destroy(core);
        return 1;
    }
    if (ezhle_last_error(core)[0] != '\0') {
        printf("  last_error set on success: %s\n", ezhle_last_error(core));
        ezhle_destroy(core);
        return 1;
    }
    ezhle_destroy(core);
    return 0;
}

/* Positive: SetNumFiles(A) is observed by GetNumFiles. A is written verbatim
 * (the wiki documents no validation) and SetNumFiles must not touch $06. */
static int test_set_num_files_roundtrip(void)
{
    fds_fake_bus_t fake;
    ezhle_bus_t bus;
    ezhle_desc_t desc;
    ezhle_core_t *core = fds_make_core(&fake, &bus, &desc);
    ezhle_regs_t regs;
    uint8_t *img;
    size_t len = 0u;
    const uint32_t want[] = { 1u, 0u, 0xABu, 2u };

    if (core == NULL) {
        return 1;
    }
    img = make_numfiles_image(2u, 0u, &len);
    if (img == NULL) {
        ezhle_destroy(core);
        return 1;
    }
    if (ezhle_fds_attach_disk(core, img, len) != EZHLE_OK) {
        free(img);
        ezhle_destroy(core);
        return 1;
    }
    free(img);

    for (size_t k = 0; k < sizeof(want) / sizeof(want[0]); k++) {
        int rc;
        memset(&regs, 0, sizeof(regs));
        fake.mem[FDS_TEST_ZP_NUM_FILES] = 0x11u;   /* must survive SetNumFiles */
        regs.r[0] = want[k];
        rc = ezhle_fds_call(core, &regs, FDS_TEST_SET_NUM_FILES);
        if (rc != EZHLE_OK) {
            printf("  set %u rc %d\n", want[k], rc);
            ezhle_destroy(core);
            return 1;
        }
        if (fake.mem[FDS_TEST_ZP_NUM_FILES] != 0x11u) {
            printf("  SetNumFiles wrote $06 = %u\n", fake.mem[FDS_TEST_ZP_NUM_FILES]);
            ezhle_destroy(core);
            return 1;
        }
        memset(&regs, 0, sizeof(regs));
        rc = ezhle_fds_call(core, &regs, FDS_TEST_GET_NUM_FILES);
        if (rc != EZHLE_OK) {
            printf("  get after set %u rc %d\n", want[k], rc);
            ezhle_destroy(core);
            return 1;
        }
        if ((uint32_t)fake.mem[FDS_TEST_ZP_NUM_FILES] != want[k]) {
            printf("  readback $06 = %u, want %u\n",
                   fake.mem[FDS_TEST_ZP_NUM_FILES], want[k]);
            ezhle_destroy(core);
            return 1;
        }
    }
    ezhle_destroy(core);
    return 0;
}

/* Negative: no disk attached. The wiki pins no guest-visible error channel for
 * either routine, so the documented fallback is UNIMPL with the reason logged;
 * $06 must not be fabricated and the core must stay usable afterwards. */
static int test_num_files_no_disk(void)
{
    fds_fake_bus_t fake;
    ezhle_bus_t bus;
    ezhle_desc_t desc;
    ezhle_core_t *core = fds_make_core(&fake, &bus, &desc);
    ezhle_regs_t regs;
    uint8_t *img;
    size_t len = 0u;
    int rc;

    if (core == NULL) {
        return 1;
    }
    fake.mem[FDS_TEST_ZP_NUM_FILES] = 0x77u;
    memset(&regs, 0, sizeof(regs));
    rc = ezhle_fds_call(core, &regs, FDS_TEST_GET_NUM_FILES);
    if (rc != EZHLE_ERR_UNIMPL) {
        printf("  get without disk rc %d, want EZHLE_ERR_UNIMPL\n", rc);
        ezhle_destroy(core);
        return 1;
    }
    if (regs.r[0] != 0xFFu) {
        printf("  A = %u, want 0xFF\n", regs.r[0]);
        ezhle_destroy(core);
        return 1;
    }
    if (fake.mem[FDS_TEST_ZP_NUM_FILES] != 0x77u) {
        printf("  $06 = %u, want untouched 0x77\n", fake.mem[FDS_TEST_ZP_NUM_FILES]);
        ezhle_destroy(core);
        return 1;
    }
    if (strstr(ezhle_last_error(core), "no disk") == NULL) {
        printf("  last_error = %s\n", ezhle_last_error(core));
        ezhle_destroy(core);
        return 1;
    }
    if (strstr(fake.log, "no disk") == NULL) {
        printf("  log = %s\n", fake.log);
        ezhle_destroy(core);
        return 1;
    }

    memset(&regs, 0, sizeof(regs));
    regs.r[0] = 3u;
    rc = ezhle_fds_call(core, &regs, FDS_TEST_SET_NUM_FILES);
    if (rc != EZHLE_ERR_UNIMPL) {
        printf("  set without disk rc %d, want EZHLE_ERR_UNIMPL\n", rc);
        ezhle_destroy(core);
        return 1;
    }
    if (regs.r[0] != 0xFFu) {
        printf("  A = %u, want 0xFF\n", regs.r[0]);
        ezhle_destroy(core);
        return 1;
    }

    /* the failed calls must not have damaged the core: attach and read */
    img = make_numfiles_image(2u, 0u, &len);
    if (img == NULL) {
        ezhle_destroy(core);
        return 1;
    }
    if (ezhle_fds_attach_disk(core, img, len) != EZHLE_OK) {
        free(img);
        ezhle_destroy(core);
        return 1;
    }
    free(img);
    memset(&regs, 0, sizeof(regs));
    if (ezhle_fds_call(core, &regs, FDS_TEST_GET_NUM_FILES) != EZHLE_OK ||
        fake.mem[FDS_TEST_ZP_NUM_FILES] != 2u) {
        printf("  after attach $06 = %u, want 2\n", fake.mem[FDS_TEST_ZP_NUM_FILES]);
        ezhle_destroy(core);
        return 1;
    }
    ezhle_destroy(core);
    return 0;
}

/* Negative: detach (twice) then call is safe for both services. */
static int test_num_files_after_detach(void)
{
    fds_fake_bus_t fake;
    ezhle_bus_t bus;
    ezhle_desc_t desc;
    ezhle_core_t *core = fds_make_core(&fake, &bus, &desc);
    ezhle_regs_t regs;
    uint8_t *img;
    size_t len = 0u;

    if (core == NULL) {
        return 1;
    }
    img = make_numfiles_image(2u, 0u, &len);
    if (img == NULL) {
        ezhle_destroy(core);
        return 1;
    }
    if (ezhle_fds_attach_disk(core, img, len) != EZHLE_OK) {
        free(img);
        ezhle_destroy(core);
        return 1;
    }
    free(img);
    memset(&regs, 0, sizeof(regs));
    if (ezhle_fds_call(core, &regs, FDS_TEST_GET_NUM_FILES) != EZHLE_OK ||
        fake.mem[FDS_TEST_ZP_NUM_FILES] != 2u) {
        printf("  pre-detach read failed ($06 = %u)\n", fake.mem[FDS_TEST_ZP_NUM_FILES]);
        ezhle_destroy(core);
        return 1;
    }

    ezhle_fds_detach_disk(core);
    ezhle_fds_detach_disk(core);   /* idempotent */

    fake.mem[FDS_TEST_ZP_NUM_FILES] = 0x33u;
    memset(&regs, 0, sizeof(regs));
    if (ezhle_fds_call(core, &regs, FDS_TEST_GET_NUM_FILES) != EZHLE_ERR_UNIMPL) {
        printf("  post-detach get did not return UNIMPL\n");
        ezhle_destroy(core);
        return 1;
    }
    if (fake.mem[FDS_TEST_ZP_NUM_FILES] != 0x33u) {
        printf("  post-detach $06 = %u, want untouched 0x33\n",
               fake.mem[FDS_TEST_ZP_NUM_FILES]);
        ezhle_destroy(core);
        return 1;
    }
    memset(&regs, 0, sizeof(regs));
    regs.r[0] = 1u;
    if (ezhle_fds_call(core, &regs, FDS_TEST_SET_NUM_FILES) != EZHLE_ERR_UNIMPL) {
        printf("  post-detach set did not return UNIMPL\n");
        ezhle_destroy(core);
        return 1;
    }
    ezhle_destroy(core);   /* destroy with no image attached */
    return 0;
}

/* Determinism: the count lives in the attached image copy, so SetNumFiles
 * survives neither detach nor a fresh attach of the same bytes, and the same
 * call sequence produces the same results every time (no clock, no globals). */
static int test_num_files_reattach_fresh(void)
{
    fds_fake_bus_t fake;
    ezhle_bus_t bus;
    ezhle_desc_t desc;
    ezhle_core_t *core = fds_make_core(&fake, &bus, &desc);
    ezhle_regs_t regs;
    uint8_t *img;
    size_t len = 0u;

    if (core == NULL) {
        return 1;
    }
    img = make_numfiles_image(2u, 0u, &len);
    if (img == NULL) {
        ezhle_destroy(core);
        return 1;
    }

    for (unsigned round = 0; round < 2u; round++) {
        if (ezhle_fds_attach_disk(core, img, len) != EZHLE_OK) {
            free(img);
            ezhle_destroy(core);
            return 1;
        }
        memset(&regs, 0, sizeof(regs));
        if (ezhle_fds_call(core, &regs, FDS_TEST_GET_NUM_FILES) != EZHLE_OK ||
            fake.mem[FDS_TEST_ZP_NUM_FILES] != 2u) {
            printf("  round %u fresh attach $06 = %u, want 2\n",
                   round, fake.mem[FDS_TEST_ZP_NUM_FILES]);
            free(img);
            ezhle_destroy(core);
            return 1;
        }
        memset(&regs, 0, sizeof(regs));
        regs.r[0] = 1u;
        if (ezhle_fds_call(core, &regs, FDS_TEST_SET_NUM_FILES) != EZHLE_OK) {
            free(img);
            ezhle_destroy(core);
            return 1;
        }
        memset(&regs, 0, sizeof(regs));
        if (ezhle_fds_call(core, &regs, FDS_TEST_GET_NUM_FILES) != EZHLE_OK ||
            fake.mem[FDS_TEST_ZP_NUM_FILES] != 1u) {
            printf("  round %u after set $06 = %u, want 1\n",
                   round, fake.mem[FDS_TEST_ZP_NUM_FILES]);
            free(img);
            ezhle_destroy(core);
            return 1;
        }
        ezhle_fds_detach_disk(core);
        /* the caller's buffer is unchanged: the parser and the service worked
         * on the library-owned copy */
        if (img[FDS_TEST_HEADER_BYTES + 0x39u] != 2u) {
            printf("  caller image mutated: count byte %u\n",
                   img[FDS_TEST_HEADER_BYTES + 0x39u]);
            free(img);
            ezhle_destroy(core);
            return 1;
        }
    }
    free(img);
    ezhle_destroy(core);
    return 0;
}

/* Documented limitation: the library has no drive/side model, so the current
 * side is the first side of the attached image (docs/FDS-COVERAGE.md). */
static int test_num_files_multi_side(void)
{
    fds_fake_bus_t fake;
    ezhle_bus_t bus;
    ezhle_desc_t desc;
    ezhle_core_t *core = fds_make_core(&fake, &bus, &desc);
    ezhle_regs_t regs;
    uint8_t *img;
    size_t len = 0u;

    if (core == NULL) {
        return 1;
    }
    img = make_numfiles_image(2u, 1u, &len);   /* side 0: 2 files, side 1: 1 */
    if (img == NULL) {
        ezhle_destroy(core);
        return 1;
    }
    if (ezhle_fds_attach_disk(core, img, len) != EZHLE_OK) {
        free(img);
        ezhle_destroy(core);
        return 1;
    }
    free(img);
    memset(&regs, 0, sizeof(regs));
    if (ezhle_fds_call(core, &regs, FDS_TEST_GET_NUM_FILES) != EZHLE_OK ||
        fake.mem[FDS_TEST_ZP_NUM_FILES] != 2u) {
        printf("  two-side $06 = %u, want 2 (first side)\n",
               fake.mem[FDS_TEST_ZP_NUM_FILES]);
        ezhle_destroy(core);
        return 1;
    }
    ezhle_destroy(core);
    return 0;
}

/* Negative: the $06 store goes through the bus; a faulting write must be
 * reported, not swallowed. */
static int test_get_num_files_write_fault(void)
{
    fds_fake_bus_t fake;
    ezhle_bus_t bus;
    ezhle_desc_t desc;
    ezhle_core_t *core;
    ezhle_regs_t regs;
    uint8_t *img;
    size_t len = 0u;
    int rc;

    memset(&fake, 0, sizeof(fake));
    memset(&bus, 0, sizeof(bus));
    bus.read32 = fds_fake_read32;
    bus.write32 = fail_write32;
    bus.read_bytes = fds_fake_read_bytes;
    bus.log = fds_fake_log;
    bus.trace = fds_fake_trace;
    bus.ctx = &fake;
    EZHLE_DESC_INIT(&desc, "fds", "test", &bus);
    core = ezhle_create(&desc);
    if (core == NULL) {
        return 1;
    }
    img = make_numfiles_image(2u, 0u, &len);
    if (img == NULL) {
        ezhle_destroy(core);
        return 1;
    }
    if (ezhle_fds_attach_disk(core, img, len) != EZHLE_OK) {
        free(img);
        ezhle_destroy(core);
        return 1;
    }
    free(img);
    memset(&regs, 0, sizeof(regs));
    rc = ezhle_fds_call(core, &regs, FDS_TEST_GET_NUM_FILES);
    if (rc != EZHLE_ERR_FAULT) {
        printf("  rc %d, want EZHLE_ERR_FAULT\n", rc);
        ezhle_destroy(core);
        return 1;
    }
    if (regs.r[0] > 0xFFu) {
        printf("  A out of 8-bit: %u\n", regs.r[0]);
        ezhle_destroy(core);
        return 1;
    }
    if (ezhle_last_error(core)[0] == '\0') {
        printf("  no last_error\n");
        ezhle_destroy(core);
        return 1;
    }
    if (fake.log_len == 0) {
        printf("  no log\n");
        ezhle_destroy(core);
        return 1;
    }
    ezhle_destroy(core);
    return 0;
}

static int test_attach_disk(void)
{
    fds_fake_bus_t fake;
    ezhle_bus_t bus;
    ezhle_desc_t desc;
    ezhle_core_t *core = fds_make_core(&fake, &bus, &desc);
    uint8_t *img;
    size_t len = 0;
    int rc;

    if (core == NULL) {
        return 1;
    }
    img = make_attach_image(&len);
    if (img == NULL) {
        ezhle_destroy(core);
        return 1;
    }

    rc = ezhle_fds_attach_disk(core, img, len);
    if (rc != EZHLE_OK) { printf("  attach valid rc=%d\n", rc); free(img); ezhle_destroy(core); return 1; }
    /* A second attach must replace the owned image without leaking. */
    rc = ezhle_fds_attach_disk(core, img, len);
    if (rc != EZHLE_OK) { printf("  attach twice rc=%d\n", rc); free(img); ezhle_destroy(core); return 1; }

    if (ezhle_fds_attach_disk(NULL, img, len) != EZHLE_ERR_ARG) { free(img); ezhle_destroy(core); return 1; }
    if (ezhle_fds_attach_disk(core, NULL, len) != EZHLE_ERR_ARG) { free(img); ezhle_destroy(core); return 1; }
    if (ezhle_fds_attach_disk(core, img, 0) != EZHLE_ERR_ARG) { free(img); ezhle_destroy(core); return 1; }
    {
        const uint8_t bad[4] = {0, 0, 0, 0};
        if (ezhle_fds_attach_disk(core, bad, sizeof(bad)) != EZHLE_ERR_STATE) { free(img); ezhle_destroy(core); return 1; }
    }

    ezhle_fds_detach_disk(core);
    ezhle_fds_detach_disk(core);
    free(img);
    ezhle_destroy(core);
    return 0;
}

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
    failed += run("attach_disk", test_attach_disk);
    failed += run("get_num_files", test_get_num_files);
    failed += run("set_num_files", test_set_num_files_roundtrip);
    failed += run("num_files_no_disk", test_num_files_no_disk);
    failed += run("num_files_detached", test_num_files_after_detach);
    failed += run("num_files_reattach", test_num_files_reattach_fresh);
    failed += run("num_files_two_sides", test_num_files_multi_side);
    failed += run("get_num_files_fault", test_get_num_files_write_fault);

    if (failed != 0) {
        printf("\n%d test(s) failed\n", failed);
        return 1;
    }
    printf("\nall fds tests passed\n");
    return 0;
}