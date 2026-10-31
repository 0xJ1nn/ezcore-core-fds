#include "common/ezhle_internal.h"
#include "ezhle/ezhle.h"
#include "ezhle/ezhle_fds.h"
#include "ezhle/ezhle_fds_image.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    /* Pseudo-register mirror of the last reset dispatch address. */
    uint16_t reset_dispatch;
    /* Last entry point dispatched (for diagnostics). */
    uint16_t last_entry;
    /* Attached FDS disk image (owned). */
    ezhle_fds_image *disk;
} fds_state_t;

static fds_state_t *fds_state(ezhle_core_t *core)
{
    if (core == NULL) {
        return NULL;
    }
    return (fds_state_t *)core->state;
}

/* Forward declarations. */
int ezhle_fds_attach_disk(ezhle_core_t *core, const uint8_t *data, size_t len);
void ezhle_fds_detach_disk(ezhle_core_t *core);
int ezhle_fds_image_set_file_count(ezhle_fds_image *disk, size_t count);

/* ---- bus helpers (8-bit, bounds-checked by the core) ---- */

static int fds_read8(ezhle_core_t *core, uint16_t addr, uint8_t *out)
{
    const ezhle_bus_t *bus = core->desc.bus;
    uint8_t byte = 0u;

    if (bus == NULL || out == NULL) {
        return EZHLE_ERR_ARG;
    }
    if (bus->read_bytes != NULL) {
        if (bus->read_bytes(bus->ctx, addr, &byte, 1u) != 0) {
            return EZHLE_ERR_FAULT;
        }
    } else if (bus->read32 != NULL) {
        uint32_t word = 0u;
        uint16_t base = (uint16_t)(addr & ~3u);
        if (bus->read32(bus->ctx, base, &word) != 0) {
            return EZHLE_ERR_FAULT;
        }
        byte = (uint8_t)((word >> (8u * (addr & 3u))) & 0xffu);
    } else {
        return EZHLE_ERR_UNIMPL;
    }
    *out = byte;
    return EZHLE_OK;
}

static int fds_write8(ezhle_core_t *core, uint16_t addr, uint8_t value)
{
    const ezhle_bus_t *bus = core->desc.bus;
    uint16_t base;
    uint32_t word = 0u;
    unsigned shift;

    if (bus == NULL || bus->read32 == NULL || bus->write32 == NULL) {
        return EZHLE_ERR_UNIMPL;
    }
    base = (uint16_t)(addr & ~3u);
    shift = 8u * (addr & 3u);
    if (bus->read32(bus->ctx, base, &word) != 0) {
        return EZHLE_ERR_FAULT;
    }
    word &= ~(0xffu << shift);
    word |= ((uint32_t)value << shift);
    if (bus->write32(bus->ctx, base, word) != 0) {
        return EZHLE_ERR_FAULT;
    }
    return EZHLE_OK;
}

static uint16_t fds_read16(ezhle_core_t *core, uint16_t addr)
{
    uint8_t lo = 0u;
    uint8_t hi = 0u;

    if (fds_read8(core, addr, &lo) != EZHLE_OK) {
        return 0u;
    }
    if (fds_read8(core, (uint16_t)(addr + 1u), &hi) != EZHLE_OK) {
        return 0u;
    }
    return (uint16_t)lo | ((uint16_t)hi << 8);
}

static void fds_log(ezhle_core_t *core, const char *message)
{
    const ezhle_bus_t *bus = core->desc.bus;

    if (bus != NULL && bus->log != NULL) {
        bus->log(bus->ctx, 2, message);
    }
}

/* ---- implemented entry points ---- */

/* $E149 Delay131: a 131-cycle delay. No inputs/outputs. No-op in HLE. Source: S10. */
static int32_t fds_delay131(ezhle_core_t *core, ezhle_regs_t *regs)
{
    (void)core;
    (void)regs;
    return 0;
}

/* $E97D Pixel2NamConv: convert pixel coords ($02=Y, $03=X) to nametable
 * address at $2000, written to $00 (high) / $01 (low). Source: NESdev FDS. */
static int32_t fds_pixel2nam(ezhle_core_t *core, ezhle_regs_t *regs)
{
    uint8_t ypix = 0u;
    uint8_t xpix = 0u;
    uint16_t tile;

    if (fds_read8(core, 0x0002u, &ypix) != EZHLE_OK) {
        return EZHLE_ERR_FAULT;
    }
    /* ypix already read; re-read X from $03 */
    if (fds_read8(core, 0x0003u, &xpix) != EZHLE_OK) {
        return EZHLE_ERR_FAULT;
    }
    tile = (uint16_t)((ypix >> 3) * 32u + (xpix >> 3));
    {
        uint16_t addr = (uint16_t)(0x2000u + tile);
        if (fds_write8(core, 0x0000u, (uint8_t)((addr >> 8) & 0xffu)) != EZHLE_OK) {
            return EZHLE_ERR_FAULT;
        }
        if (fds_write8(core, 0x0001u, (uint8_t)(addr & 0xffu)) != EZHLE_OK) {
            return EZHLE_ERR_FAULT;
        }
    }
    regs->r[EZHLE_FDS_REG_A] = 0u;
    return 0;
}

/* $E997 Nam2PixelConv: inverse of Pixel2NamConv. */
static int32_t fds_nam2pixel(ezhle_core_t *core, ezhle_regs_t *regs)
{
    uint8_t hi = 0u;
    uint8_t lo = 0u;
    uint16_t addr;
    uint16_t tile;
    uint8_t ypix;
    uint8_t xpix;

    if (fds_read8(core, 0x0000u, &hi) != EZHLE_OK) {
        return EZHLE_ERR_FAULT;
    }
    if (fds_read8(core, 0x0001u, &lo) != EZHLE_OK) {
        return EZHLE_ERR_FAULT;
    }
    addr = (uint16_t)hi << 8 | lo;
    if (addr < 0x2000u || addr > 0x23FFu) {
        return EZHLE_ERR_FAULT;
    }
    tile = (uint16_t)(addr - 0x2000u);
    ypix = (uint8_t)((tile / 32u) * 8u);
    xpix = (uint8_t)((tile % 32u) * 8u);
    if (fds_write8(core, 0x0002u, ypix) != EZHLE_OK) {
        return EZHLE_ERR_FAULT;
    }
    if (fds_write8(core, 0x0003u, xpix) != EZHLE_OK) {
        return EZHLE_ERR_FAULT;
    }
    regs->r[EZHLE_FDS_REG_A] = 0u;
    return 0;
}

/* ---- file-count services (FDS-106) ---- */

static ezhle_fds_image *fds_attached_disk(ezhle_core_t *core)
{
    fds_state_t *st = fds_state(core);
    return st == NULL ? NULL : st->disk;
}

/* $E484 GetNumFiles: copy the attached image's file count to zero-page $06.
 * Source: https://www.nesdev.org/wiki/FDS_BIOS */
static int32_t fds_get_num_files(ezhle_core_t *core, ezhle_regs_t *regs)
{
    ezhle_fds_image *disk = fds_attached_disk(core);
    size_t count;

    if (disk == NULL) {
        ezhle_set_error(core, "GetNumFiles: no disk");
        fds_log(core, "GetNumFiles: no disk");
        regs->r[EZHLE_FDS_REG_A] = 0xFFu;
        return EZHLE_ERR_UNIMPL;
    }
    count = ezhle_fds_image_first_side_file_count(disk);
    if (count > 0xFFu) {
        return EZHLE_ERR_FAULT;
    }
    if (fds_write8(core, 0x0006u, (uint8_t)count) != EZHLE_OK) {
        return EZHLE_ERR_FAULT;
    }
    regs->r[EZHLE_FDS_REG_A] = 0u;
    return 0;
}

/* $E492 SetNumFiles: A = new file count, written to the attached image.
 * Source: https://www.nesdev.org/wiki/FDS_BIOS */
static int32_t fds_set_num_files(ezhle_core_t *core, ezhle_regs_t *regs)
{
    ezhle_fds_image *disk = fds_attached_disk(core);

    if (disk == NULL) {
        ezhle_set_error(core, "SetNumFiles: no disk");
        fds_log(core, "SetNumFiles: no disk");
        regs->r[EZHLE_FDS_REG_A] = 0xFFu;
        return EZHLE_ERR_UNIMPL;
    }
    if (ezhle_fds_image_set_file_count(disk, (size_t)regs->r[EZHLE_FDS_REG_A]) != 0) {
        return EZHLE_ERR_STATE;
    }
    regs->r[EZHLE_FDS_REG_A] = 0u;
    return 0;
}

/* ---- LoadFiles $E1F8 (FDS-108) ---- */

static int fds_load_files_is_dummy(uint16_t addr, uint16_t size)
{
    uint32_t end = (uint32_t)addr + (uint32_t)size;
    if (end > 0xFFFFu) {
        return 1;
    }
    if (addr <= 0x01FFu) {
        return 1;
    }
    if (addr >= 0x0200u && addr <= 0x07FFu) {
        return 0;
    }
    if (addr >= 0x1200u && addr <= 0x17FFu) {
        return 0;
    }
    if (addr < 0x2000u) {
        return 1;
    }
    return 0;
}

static int32_t fds_load_files(ezhle_core_t *core, ezhle_regs_t *regs)
{
    ezhle_fds_image *disk = fds_attached_disk(core);
    fds_state_t *st = fds_state(core);
    uint8_t sp;
    uint16_t ret_addr;
    uint8_t stack_bytes[2];
    uint16_t ptr_disk_id;
    uint16_t ptr_file_list;
    uint8_t disk_id[10];
    uint8_t info_id[10];
    uint8_t list_ids[20];
    size_t list_count = 0u;
    int empty_list = 0;
    size_t first_count;
    size_t loaded;
    unsigned attempt;
    char msg[128];

    (void)st;

    if (disk == NULL) {
        ezhle_set_error(core, "LoadFiles: no disk");
        fds_log(core, "LoadFiles: no disk");
        regs->r[EZHLE_FDS_REG_A] = 0x01u;
        regs->r[EZHLE_FDS_REG_Y] = 0u;
        return EZHLE_ERR_FAULT;
    }

    /* Direct pointers: read return address from stack (little-endian). */
    sp = (uint8_t)(regs->r[EZHLE_FDS_REG_SP] & 0xFFu);
    if (fds_read8(core, (uint16_t)(0x0100u + sp), &stack_bytes[0]) != EZHLE_OK) {
        ezhle_set_error(core, "LoadFiles: bad stack read");
        fds_log(core, "LoadFiles: bad stack read");
        regs->r[EZHLE_FDS_REG_A] = 0xFFu;
        regs->r[EZHLE_FDS_REG_Y] = 0u;
        return EZHLE_ERR_UNIMPL;
    }
    if (fds_read8(core, (uint16_t)(0x0100u + (uint8_t)(sp + 1u)), &stack_bytes[1]) != EZHLE_OK) {
        ezhle_set_error(core, "LoadFiles: bad stack read");
        fds_log(core, "LoadFiles: bad stack read");
        regs->r[EZHLE_FDS_REG_A] = 0xFFu;
        regs->r[EZHLE_FDS_REG_Y] = 0u;
        return EZHLE_ERR_UNIMPL;
    }
    ret_addr = (uint16_t)stack_bytes[0] | ((uint16_t)stack_bytes[1] << 8);

    /* Read two direct pointer words at return address and +2 (per contract). */
    {
        uint8_t b[4];
        unsigned i;
        for (i = 0u; i < 4u; i++) {
            uint32_t a = (uint32_t)ret_addr + i;
            if (a > 0xFFFFu) {
                ezhle_set_error(core, "LoadFiles: bad direct pointer");
                fds_log(core, "LoadFiles: bad direct pointer");
                regs->r[EZHLE_FDS_REG_A] = 0xFFu;
                regs->r[EZHLE_FDS_REG_Y] = 0u;
                return EZHLE_ERR_UNIMPL;
            }
            if (fds_read8(core, (uint16_t)a, &b[i]) != EZHLE_OK) {
                ezhle_set_error(core, "LoadFiles: bad direct pointer");
                fds_log(core, "LoadFiles: bad direct pointer");
                regs->r[EZHLE_FDS_REG_A] = 0xFFu;
                regs->r[EZHLE_FDS_REG_Y] = 0u;
                return EZHLE_ERR_UNIMPL;
            }
        }
        ptr_disk_id = (uint16_t)b[0] | ((uint16_t)b[1] << 8);
        ptr_file_list = (uint16_t)b[2] | ((uint16_t)b[3] << 8);
    }

    /* Fix return address to land past both pointer words. */
    {
        uint16_t fixed = (uint16_t)(ret_addr + 4u);
        if (fds_write8(core, (uint16_t)(0x0100u + sp), (uint8_t)(fixed & 0xFFu)) != EZHLE_OK) {
            ezhle_set_error(core, "LoadFiles: bad stack write");
            fds_log(core, "LoadFiles: bad stack write");
            regs->r[EZHLE_FDS_REG_A] = 0xFFu;
            regs->r[EZHLE_FDS_REG_Y] = 0u;
            return EZHLE_ERR_UNIMPL;
        }
        if (fds_write8(core, (uint16_t)(0x0100u + (uint8_t)(sp + 1u)), (uint8_t)(fixed >> 8)) != EZHLE_OK) {
            ezhle_set_error(core, "LoadFiles: bad stack write");
            fds_log(core, "LoadFiles: bad stack write");
            regs->r[EZHLE_FDS_REG_A] = 0xFFu;
            regs->r[EZHLE_FDS_REG_Y] = 0u;
            return EZHLE_ERR_UNIMPL;
        }
    }

    /* Read Disk ID structure (10 bytes). */
    for (unsigned i = 0u; i < 10u; i++) {
        uint32_t a = (uint32_t)ptr_disk_id + i;
        if (a > 0xFFFFu) {
            ezhle_set_error(core, "LoadFiles: bad Disk-ID pointer");
            fds_log(core, "LoadFiles: bad Disk-ID pointer");
            regs->r[EZHLE_FDS_REG_A] = 0xFFu;
            regs->r[EZHLE_FDS_REG_Y] = 0u;
            return EZHLE_ERR_UNIMPL;
        }
        if (fds_read8(core, (uint16_t)a, &disk_id[i]) != EZHLE_OK) {
            ezhle_set_error(core, "LoadFiles: bad Disk-ID pointer");
            fds_log(core, "LoadFiles: bad Disk-ID pointer");
            regs->r[EZHLE_FDS_REG_A] = 0xFFu;
            regs->r[EZHLE_FDS_REG_Y] = 0u;
            return EZHLE_ERR_UNIMPL;
        }
    }

    /* Read File List (up to 20 IDs, $FF terminator). */
    {
        uint8_t first_byte;
        uint32_t a0 = ptr_file_list;
        if (a0 > 0xFFFFu) {
            ezhle_set_error(core, "LoadFiles: bad File-List pointer");
            fds_log(core, "LoadFiles: bad File-List pointer");
            regs->r[EZHLE_FDS_REG_A] = 0xFFu;
            regs->r[EZHLE_FDS_REG_Y] = 0u;
            return EZHLE_ERR_UNIMPL;
        }
        if (fds_read8(core, (uint16_t)a0, &first_byte) != EZHLE_OK) {
            ezhle_set_error(core, "LoadFiles: bad File-List pointer");
            fds_log(core, "LoadFiles: bad File-List pointer");
            regs->r[EZHLE_FDS_REG_A] = 0xFFu;
            regs->r[EZHLE_FDS_REG_Y] = 0u;
            return EZHLE_ERR_UNIMPL;
        }
        if (first_byte == 0xFFu) {
            empty_list = 1;
        } else {
            for (unsigned i = 0u; i < 20u; i++) {
                uint32_t a = (uint32_t)ptr_file_list + i;
                uint8_t b;
                if (a > 0xFFFFu) {
                    ezhle_set_error(core, "LoadFiles: bad File-List pointer");
                    fds_log(core, "LoadFiles: bad File-List pointer");
                    regs->r[EZHLE_FDS_REG_A] = 0xFFu;
                    regs->r[EZHLE_FDS_REG_Y] = 0u;
                    return EZHLE_ERR_UNIMPL;
                }
                if (fds_read8(core, (uint16_t)a, &b) != EZHLE_OK) {
                    ezhle_set_error(core, "LoadFiles: bad File-List pointer");
                    fds_log(core, "LoadFiles: bad File-List pointer");
                    regs->r[EZHLE_FDS_REG_A] = 0xFFu;
                    regs->r[EZHLE_FDS_REG_Y] = 0u;
                    return EZHLE_ERR_UNIMPL;
                }
                if (b == 0xFFu) {
                    break;
                }
                list_ids[list_count++] = b;
                if (list_count >= 20u) {
                    break;
                }
            }
        }
    }

    if (ezhle_fds_image_first_side_disk_id(disk, info_id) != 0) {
        ezhle_set_error(core, "LoadFiles: disk info unavailable");
        fds_log(core, "LoadFiles: disk info unavailable");
        regs->r[EZHLE_FDS_REG_A] = 0xFFu;
        regs->r[EZHLE_FDS_REG_Y] = 0u;
        return EZHLE_ERR_UNIMPL;
    }

    /* CheckDiskHeader comparison (wildcard $FF). */
    if (disk_id[0] != 0xFFu && disk_id[0] != info_id[0]) {
        ezhle_set_error(core, "LoadFiles: wrong licensee code");
        fds_log(core, "LoadFiles: wrong licensee code");
        regs->r[EZHLE_FDS_REG_A] = 0x04u;
        regs->r[EZHLE_FDS_REG_Y] = 0u;
        return EZHLE_ERR_FAULT;
    }
    for (unsigned i = 1u; i <= 4u; i++) {
        if (disk_id[i] != 0xFFu && disk_id[i] != info_id[i]) {
            ezhle_set_error(core, "LoadFiles: wrong game name/type");
            fds_log(core, "LoadFiles: wrong game name/type");
            regs->r[EZHLE_FDS_REG_A] = 0x05u;
            regs->r[EZHLE_FDS_REG_Y] = 0u;
            return EZHLE_ERR_FAULT;
        }
    }
    if (disk_id[5] != 0xFFu && disk_id[5] != info_id[5]) {
        ezhle_set_error(core, "LoadFiles: wrong game version");
        fds_log(core, "LoadFiles: wrong game version");
        regs->r[EZHLE_FDS_REG_A] = 0x06u;
        regs->r[EZHLE_FDS_REG_Y] = 0u;
        return EZHLE_ERR_FAULT;
    }
    if (disk_id[6] != 0xFFu && disk_id[6] != info_id[6]) {
        ezhle_set_error(core, "LoadFiles: wrong side number");
        fds_log(core, "LoadFiles: wrong side number");
        regs->r[EZHLE_FDS_REG_A] = 0x07u;
        regs->r[EZHLE_FDS_REG_Y] = 0u;
        return EZHLE_ERR_FAULT;
    }
    if (disk_id[7] != 0xFFu && disk_id[7] != info_id[7]) {
        ezhle_set_error(core, "LoadFiles: wrong disk number");
        fds_log(core, "LoadFiles: wrong disk number");
        regs->r[EZHLE_FDS_REG_A] = 0x08u;
        regs->r[EZHLE_FDS_REG_Y] = 0u;
        return EZHLE_ERR_FAULT;
    }
    if (disk_id[8] != 0xFFu && disk_id[8] != info_id[8]) {
        ezhle_set_error(core, "LoadFiles: wrong disk type");
        fds_log(core, "LoadFiles: wrong disk type");
        regs->r[EZHLE_FDS_REG_A] = 0x09u;
        regs->r[EZHLE_FDS_REG_Y] = 0u;
        return EZHLE_ERR_FAULT;
    }
    if (disk_id[9] != 0xFFu && disk_id[9] != info_id[9]) {
        ezhle_set_error(core, "LoadFiles: wrong unknown field");
        fds_log(core, "LoadFiles: wrong unknown field");
        regs->r[EZHLE_FDS_REG_A] = 0x10u;
        regs->r[EZHLE_FDS_REG_Y] = 0u;
        return EZHLE_ERR_FAULT;
    }

    first_count = ezhle_fds_image_first_side_file_count(disk);
    if (first_count > ezhle_fds_image_file_count(disk)) {
        first_count = ezhle_fds_image_file_count(disk);
    }

    /* Missing file detection for explicit lists. */
    if (!empty_list) {
        for (size_t k = 0u; k < list_count; k++) {
            int found = 0;
            for (size_t i = 0u; i < first_count; i++) {
                const ezhle_fds_image_file *f = ezhle_fds_image_file_at(disk, i);
                if (f != NULL && f->id == list_ids[k]) {
                    found = 1;
                    break;
                }
            }
            if (!found) {
                ezhle_set_error(core, "LoadFiles: could not load all files");
                fds_log(core, "LoadFiles: could not load all files");
                regs->r[EZHLE_FDS_REG_A] = 0x40u;
                regs->r[EZHLE_FDS_REG_Y] = 0u;
                return EZHLE_ERR_FAULT;
            }
        }
    }

    /* Two-attempt retry wrapper. */
    for (attempt = 0u; attempt < 2u; attempt++) {
        loaded = 0u;
        int failed = 0;
        for (size_t i = 0u; i < first_count; i++) {
            const ezhle_fds_image_file *f = ezhle_fds_image_file_at(disk, i);
            int match = 0;
            if (f == NULL) {
                continue;
            }
            if (empty_list) {
                if (f->id <= ezhle_fds_image_boot_read_code(disk)) {
                    match = 1;
                }
            } else {
                for (size_t k = 0u; k < list_count; k++) {
                    if (list_ids[k] == f->id) {
                        match = 1;
                        break;
                    }
                }
            }
            if (!match) {
                continue;
            }

            loaded++;
            if (!fds_load_files_is_dummy(f->address, f->size)) {
                const uint8_t *data = ezhle_fds_image_file_data(disk, f);
                if (data == NULL) {
                    failed = 1;
                    break;
                }
                for (uint16_t s = 0u; s < f->size; s++) {
                    uint16_t addr = (uint16_t)((uint32_t)f->address + s);
                    if (fds_write8(core, addr, data[s]) != EZHLE_OK) {
                        failed = 1;
                        break;
                    }
                }
                if (failed) {
                    break;
                }
            }
        }

        if (!failed) {
            regs->r[EZHLE_FDS_REG_Y] = (uint32_t)loaded;
            regs->r[EZHLE_FDS_REG_A] = 0u;
            (void)snprintf(msg, sizeof(msg), "LoadFiles: loaded %u files", (unsigned)loaded);
            fds_log(core, msg);
            return 0;
        }
    }

    ezhle_set_error(core, "LoadFiles: write fault");
    fds_log(core, "LoadFiles: write fault");
    regs->r[EZHLE_FDS_REG_A] = 0xFFu;
    regs->r[EZHLE_FDS_REG_Y] = 0u;
    return EZHLE_ERR_FAULT;
}

static const ezhle_fds_entry_t FDS_ENTRIES[] = {
    { 0xE149u, "Delay131", 1 },
    { 0xE153u, "Delayms", 0 },
    { 0xE161u, "DisPFObj", 0 },
    { 0xE16Bu, "EnPFObj", 0 },
    { 0xE171u, "DisObj", 0 },
    { 0xE178u, "EnObj", 0 },
    { 0xE17Eu, "DisPF", 0 },
    { 0xE185u, "EnPF", 0 },
    { 0xE19Du, "VINTWaitHandler", 0 }, /* vector-control NMI-handler target (S10), not a JSR routine */
    { 0xE1B2u, "VINTWait", 0 },
    { 0xE1CEu, "DiskTransfer", 0 },
    { 0xE1D9u, "DiskSkipBytes", 0 },
    { 0xE1EFu, "AcknowledgeDelay", 0 },
    { 0xE1F8u, "LoadFiles", 1 },
    { 0xE237u, "AppendFile", 0 },
    { 0xE239u, "WriteFile", 0 },
    { 0xE2B7u, "CheckFileCount", 0 },
    { 0xE2BBu, "AdjustFileCount", 0 },
    { 0xE301u, "SetFileCount1", 0 },
    { 0xE305u, "SetFileCount", 0 },
    { 0xE32Au, "GetDiskInfo", 0 },
    { 0xE445u, "CheckDiskHeader", 0 },
    { 0xE484u, "GetNumFiles", 1 },
    { 0xE492u, "SetNumFiles", 1 },
    { 0xE4A0u, "FileMatchTest", 0 },
    { 0xE4DAu, "SkipFiles", 0 },
    { 0xE7BBu, "VRAMStructWrite", 0 },
    { 0xE844u, "FetchDirectPtr", 0 },
    { 0xE86Au, "WriteVRAMBuffer", 0 },
    { 0xE8B3u, "ReadVRAMBuffer", 0 },
    { 0xE8D2u, "PrepareVRAMString", 0 },
    { 0xE8E1u, "PrepareVRAMStrings", 0 },
    { 0xE94Fu, "GetVRAMBufferByte", 0 },
    { 0xE97Du, "Pixel2NamConv", 1 },
    { 0xE997u, "Nam2PixelConv", 1 },
    { 0xE9B1u, "Random", 0 },
};

const ezhle_fds_entry_t *ezhle_fds_entry_table(void)
{
    return FDS_ENTRIES;
}

size_t ezhle_fds_entry_count(void)
{
    return sizeof(FDS_ENTRIES) / sizeof(FDS_ENTRIES[0]);
}

const ezhle_fds_entry_t *ezhle_fds_entry_find(uint16_t address)
{
    size_t i;

    for (i = 0; i < ezhle_fds_entry_count(); i++) {
        if (FDS_ENTRIES[i].address == address) {
            return &FDS_ENTRIES[i];
        }
    }
    return NULL;
}

uint16_t ezhle_fds_reset_vector(ezhle_core_t *core)
{
    if (core == NULL) {
        return 0u;
    }
    /* Read the vector live on each call (two byte reads) so a guest image
     * mapped after create() is reflected without a separate reset. */
    return fds_read16(core, EZHLE_FDS_RESET_VECTOR_ADDR);
}

uint8_t ezhle_fds_nmi_control(ezhle_core_t *core)
{
    uint8_t v = 0u;
    if (core == NULL) {
        return 0u;
    }
    if (fds_read8(core, EZHLE_FDS_NMI_CONTROL_ADDR, &v) != EZHLE_OK) {
        return 0u;
    }
    return v;
}

uint8_t ezhle_fds_irq_control(ezhle_core_t *core)
{
    uint8_t v = 0u;
    if (core == NULL) {
        return 0u;
    }
    if (fds_read8(core, EZHLE_FDS_IRQ_CONTROL_ADDR, &v) != EZHLE_OK) {
        return 0u;
    }
    return v;
}

uint16_t ezhle_fds_nmi_vector(ezhle_core_t *core)
{
    uint8_t ctrl;
    uint16_t game_addr;

    if (core == NULL) {
        return 0u;
    }
    ctrl = ezhle_fds_nmi_control(core);
    switch (ctrl >> 6) {
    case 0:
        return EZHLE_FDS_VINTWAIT_HANDLER_ADDR;
    case 1:
        game_addr = fds_read16(core, EZHLE_FDS_GAME_NMI_VECTOR_1_ADDR);
        return game_addr;
    case 2:
        game_addr = fds_read16(core, EZHLE_FDS_GAME_NMI_VECTOR_2_ADDR);
        return game_addr;
    default:
        game_addr = fds_read16(core, EZHLE_FDS_GAME_NMI_VECTOR_3_ADDR);
        return game_addr;
    }
}

uint16_t ezhle_fds_irq_vector(ezhle_core_t *core)
{
    uint8_t ctrl;

    if (core == NULL) {
        return 0u;
    }
    ctrl = ezhle_fds_irq_control(core);
    switch (ctrl >> 6) {
    case 0:
        return EZHLE_FDS_DISK_SKIP_BYTES_ADDR;
    case 1:
        return EZHLE_FDS_DISK_TRANSFER_ADDR;
    case 2:
        return EZHLE_FDS_ACKNOWLEDGE_DELAY_ADDR;
    default:
        return fds_read16(core, EZHLE_FDS_GAME_IRQ_VECTOR_ADDR);
    }
}

uint16_t ezhle_fds_nmi_cpu_vector(ezhle_core_t *core)
{
    if (core == NULL) {
        return 0u;
    }
    return fds_read16(core, EZHLE_FDS_NMI_CPU_VECTOR_ADDR);
}

uint16_t ezhle_fds_irq_cpu_vector(ezhle_core_t *core)
{
    if (core == NULL) {
        return 0u;
    }
    return fds_read16(core, EZHLE_FDS_IRQ_CPU_VECTOR_ADDR);
}

/* Aliases. */
uint16_t ezhle_fds_nmi_target(ezhle_core_t *core) { return ezhle_fds_nmi_vector(core); }
uint16_t ezhle_fds_irq_target(ezhle_core_t *core) { return ezhle_fds_irq_vector(core); }
uint16_t ezhle_fds_nmi_handler(ezhle_core_t *core) { return ezhle_fds_nmi_vector(core); }
uint16_t ezhle_fds_irq_handler(ezhle_core_t *core) { return ezhle_fds_irq_vector(core); }
uint16_t ezhle_fds_nmi_dispatch(ezhle_core_t *core) { return ezhle_fds_nmi_vector(core); }
uint16_t ezhle_fds_irq_dispatch(ezhle_core_t *core) { return ezhle_fds_irq_vector(core); }
uint16_t ezhle_fds_resolve_nmi(ezhle_core_t *core) { return ezhle_fds_nmi_vector(core); }
uint16_t ezhle_fds_resolve_irq(ezhle_core_t *core) { return ezhle_fds_irq_vector(core); }

int ezhle_fds_call(ezhle_core_t *core, ezhle_regs_t *regs, uint16_t entry)
{
    if (core == NULL) {
        return EZHLE_ERR_ARG;
    }
    if (regs == NULL) {
        return EZHLE_ERR_ARG;
    }
    regs->pc = entry;
    return ezhle_dispatch(core, regs);
}

/* ---- dispatch ---- */

static int fds_dispatch(ezhle_core_t *core, ezhle_regs_t *regs)
{
    fds_state_t *st;
    uint16_t entry;
    const ezhle_fds_entry_t *info;
    int32_t ret;
    char msg[128];

    if (core == NULL) {
        return EZHLE_ERR_ARG;
    }
    st = fds_state(core);
    if (st == NULL) {
        return EZHLE_ERR_STATE;
    }
    if (regs == NULL) {
        return EZHLE_ERR_ARG;
    }

    entry = (uint16_t)regs->pc;
    st->last_entry = entry;

    info = ezhle_fds_entry_find(entry);
    if (info == NULL) {
        (void)snprintf(msg, sizeof(msg),
                       "unimplemented FDS entry $%04" PRIx16, entry);
        ezhle_set_error(core, "%s", msg);
        fds_log(core, msg);
        regs->r[EZHLE_FDS_REG_A] = 0xFFu;
        return EZHLE_ERR_UNIMPL;
    }

    ret = EZHLE_ERR_UNIMPL;
    if (info->implemented) {
        switch (entry) {
        case 0xE149u:
            ret = fds_delay131(core, regs);
            break;
        case 0xE97Du:
            ret = fds_pixel2nam(core, regs);
            break;
        case 0xE997u:
            ret = fds_nam2pixel(core, regs);
            break;
        case 0xE484u:
            ret = fds_get_num_files(core, regs);
            break;
        case 0xE492u:
            ret = fds_set_num_files(core, regs);
            break;
        case 0xE1F8u:
            ret = fds_load_files(core, regs);
            break;
        default:
            ret = EZHLE_ERR_UNIMPL;
            break;
        }
    }

    if (ret == EZHLE_ERR_UNIMPL) {
        if (info->implemented) {
            (void)snprintf(msg, sizeof(msg),
                           "implemented FDS entry %s ($%04" PRIx16 ") returned UNIMPL",
                           info->name, entry);
            fds_log(core, msg);
        } else {
            (void)snprintf(msg, sizeof(msg),
                           "unimplemented FDS entry %s ($%04" PRIx16 ")",
                           info->name, entry);
            ezhle_set_error(core, "%s", msg);
            fds_log(core, msg);
        }
        regs->r[EZHLE_FDS_REG_A] = 0xFFu;
        return EZHLE_ERR_UNIMPL;
    }

    if (ret < 0) {
        (void)snprintf(msg, sizeof(msg),
                       "FDS entry %s ($%04" PRIx16 ") failed",
                       info->name, entry);
        ezhle_set_error(core, "%s", msg);
        fds_log(core, msg);
        if (entry != 0xE1F8u) {
            regs->r[EZHLE_FDS_REG_A] = (uint32_t)(uint8_t)ret;
        }
        return ret;
    }

    regs->r[EZHLE_FDS_REG_A] = (uint32_t)ret;

    if (core->desc.bus != NULL && core->desc.bus->trace != NULL) {
        uint32_t args[4];
        args[0] = regs->r[EZHLE_FDS_REG_A];
        args[1] = regs->r[EZHLE_FDS_REG_X];
        args[2] = regs->r[EZHLE_FDS_REG_Y];
        args[3] = regs->r[EZHLE_FDS_REG_P];
        core->desc.bus->trace(core->desc.bus->ctx, entry, args, 4u, ret);
    }
    return EZHLE_OK;
}

static void fds_reset(ezhle_core_t *core)
{
    fds_state_t *st;

    if (core == NULL) {
        return;
    }
    st = fds_state(core);
    if (st != NULL) {
        ezhle_fds_image *disk = st->disk;
        memset(st, 0, sizeof(*st));
        st->disk = disk;
        st->reset_dispatch = fds_read16(core, EZHLE_FDS_RESET_VECTOR_ADDR);
    }
}

static void fds_destroy(ezhle_core_t *core)
{
    if (core == NULL) {
        return;
    }
    ezhle_fds_detach_disk(core);
    free(core->state);
    core->state = NULL;
}

int ezhle_fds_attach(ezhle_core_t *core)
{
    fds_state_t *st;

    if (core == NULL) {
        return EZHLE_ERR_ARG;
    }
    if (core->state != NULL) {
        ezhle_fds_detach_disk(core);
        free(core->state);
        core->state = NULL;
    }
    st = calloc(1, sizeof(*st));
    if (st == NULL) {
        return EZHLE_ERR_NOMEM;
    }
    core->state = st;
    core->reset_fn = fds_reset;
    core->dispatch_fn = fds_dispatch;
    core->destroy_fn = fds_destroy;
    st->reset_dispatch = fds_read16(core, EZHLE_FDS_RESET_VECTOR_ADDR);
    /* Populate reset dispatch after wiring (reset_fn will be called by ezhle_reset). */
    return EZHLE_OK;
}

int ezhle_fds_attach_disk(ezhle_core_t *core, const uint8_t *data, size_t len)
{
    fds_state_t *st;
    ezhle_fds_image *new_disk;

    if (core == NULL || data == NULL || len == 0u) {
        return EZHLE_ERR_ARG;
    }
    st = fds_state(core);
    if (st == NULL) {
        return EZHLE_ERR_STATE;
    }
    new_disk = ezhle_fds_image_open(data, len, NULL);
    if (new_disk == NULL) {
        return EZHLE_ERR_STATE;
    }
    if (st->disk != NULL) {
        ezhle_fds_image_close(st->disk);
    }
    st->disk = new_disk;
    return EZHLE_OK;
}

void ezhle_fds_detach_disk(ezhle_core_t *core)
{
    fds_state_t *st;

    st = fds_state(core);
    if (st == NULL) {
        return;
    }
    if (st->disk != NULL) {
        ezhle_fds_image_close(st->disk);
        st->disk = NULL;
    }
}

/* Ensure reset_dispatch is populated even without an explicit reset call. */
/* (ezhle_create does not call reset; callers/tests may call ezhle_reset.) */