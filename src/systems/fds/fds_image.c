/* SPDX-License-Identifier: BSD-2-Clause */
/* ezCORE HLE — Famicom Disk System image parser
 * Provenance: clean-room implementation from public format documentation.
 * Sources: https://www.nesdev.org/wiki/FDS_file_format
 *          https://www.nesdev.org/wiki/FDS_disk_format
 * Authors: Hermes Agent, 2026-10-30
 * Reviewed-by: pending independent review (G4)
 */
#include "ezhle/ezhle_fds_image.h"

#include <stdlib.h>
#include <string.h>

#define FDS_SIDE_BYTES 65500u
#define FDS_HEADER_BYTES 16u
#define FDS_DISK_INFO_BYTES 0x38u
#define FDS_FILE_HEADER_BYTES 0x10u
#define FDS_MAX_FILES 255u
#define FDS_MAGIC "*NINTENDO-HVC*"

struct ezhle_fds_image {
    uint8_t *data;
    size_t len;
    ezhle_fds_image_file *files;
    size_t count;
    size_t cap;
    uint8_t boot_code;
    uint8_t boot_set;
    uint8_t sides;
    size_t file_count_offset;
    size_t first_side_count;
    int has_file_count;
};

static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static int push_file(ezhle_fds_image *d, const ezhle_fds_image_file *f)
{
    if (d->count == d->cap) {
        size_t next = d->cap == 0 ? 16u : d->cap * 2u;
        ezhle_fds_image_file *grown;
        if (next > FDS_MAX_FILES) {
            next = FDS_MAX_FILES;
        }
        if (next <= d->cap) {
            return EZHLE_FDS_IMAGE_E_RANGE;
        }
        grown = (ezhle_fds_image_file *)realloc(d->files, next * sizeof(*grown));
        if (grown == NULL) {
            return EZHLE_FDS_IMAGE_E_NOMEM;
        }
        d->files = grown;
        d->cap = next;
    }
    d->files[d->count++] = *f;
    return EZHLE_FDS_IMAGE_OK;
}

static int parse_side(ezhle_fds_image *d, size_t base, size_t side_len)
{
    size_t pos = base;
    size_t end = base + side_len;
    uint8_t file_count = 0;
    unsigned i;

    if (side_len < FDS_DISK_INFO_BYTES + 2u) {
        return EZHLE_FDS_IMAGE_E_FORMAT;
    }
    if (d->data[pos] != 0x01) {
        return EZHLE_FDS_IMAGE_E_FORMAT;
    }
    if (memcmp(d->data + pos + 1, FDS_MAGIC, 14) != 0) {
        return EZHLE_FDS_IMAGE_E_FORMAT;
    }
    if (!d->boot_set) {
        d->boot_code = d->data[pos + 0x19];
        d->boot_set = 1;
    }
    pos += FDS_DISK_INFO_BYTES;

    if (pos + 2u > end || d->data[pos] != 0x02) {
        return EZHLE_FDS_IMAGE_E_FORMAT;
    }
    file_count = d->data[pos + 1];
    if (!d->has_file_count) {
        d->file_count_offset = pos + 1u;
        d->first_side_count = file_count;
        d->has_file_count = 1;
    }
    pos += 2u;

    for (i = 0; i < file_count; i++) {
        ezhle_fds_image_file f;
        uint16_t size;

        if (pos + FDS_FILE_HEADER_BYTES + 1u > end || d->data[pos] != 0x03) {
            return EZHLE_FDS_IMAGE_E_FORMAT;
        }
        memset(&f, 0, sizeof(f));
        f.number = d->data[pos + 1];
        f.id = d->data[pos + 2];
        memcpy(f.name, d->data + pos + 3, 8);
        f.name[8] = '\0';
        f.address = rd16(d->data + pos + 0x0B);
        f.size = rd16(d->data + pos + 0x0D);
        f.type = d->data[pos + 0x0F];
        size = f.size;
        pos += FDS_FILE_HEADER_BYTES;

        if (pos + 1u + (size_t)size > end || d->data[pos] != 0x04) {
            return EZHLE_FDS_IMAGE_E_FORMAT;
        }
        f.data_offset = pos + 1u;
        pos += 1u + (size_t)size;

        if ((uint32_t)f.address + (uint32_t)f.size > 0x10000u) {
            return EZHLE_FDS_IMAGE_E_RANGE;
        }
        if (push_file(d, &f) != EZHLE_FDS_IMAGE_OK) {
            return EZHLE_FDS_IMAGE_E_NOMEM;
        }
    }

    /* Optional test-data block 5 fills the rest of the side. Padding zeros
     * after the last file are also accepted. */
    if (pos < end && d->data[pos] != 0x05 && d->data[pos] != 0x00) {
        return EZHLE_FDS_IMAGE_E_FORMAT;
    }
    return EZHLE_FDS_IMAGE_OK;
}

ezhle_fds_image *ezhle_fds_image_open(const uint8_t *data, size_t len, int *err)
{
    ezhle_fds_image *d;
    size_t offset = 0;
    uint8_t sides;
    unsigned s;

    if (err != NULL) {
        *err = EZHLE_FDS_IMAGE_E_FORMAT;
    }
    if (data == NULL || len < FDS_DISK_INFO_BYTES + 2u) {
        return NULL;
    }

    if (len >= FDS_HEADER_BYTES && data[0] == 'F' && data[1] == 'D' && data[2] == 'S' && data[3] == 0x1A) {
        sides = data[4];
        offset = FDS_HEADER_BYTES;
        if (sides == 0) {
            sides = 1;
        }
    } else {
        sides = (uint8_t)(len / FDS_SIDE_BYTES);
        if (sides == 0) {
            sides = 1;
        }
    }

    if (len < offset + (size_t)sides * FDS_SIDE_BYTES) {
        /* Accept a final partial side only if it is at least one block. */
        if (len < offset + FDS_DISK_INFO_BYTES + 2u) {
            return NULL;
        }
    }

    d = (ezhle_fds_image *)calloc(1, sizeof(*d));
    if (d == NULL) {
        if (err != NULL) {
            *err = EZHLE_FDS_IMAGE_E_NOMEM;
        }
        return NULL;
    }
    d->data = (uint8_t *)malloc(len);
    if (d->data == NULL) {
        free(d);
        if (err != NULL) {
            *err = EZHLE_FDS_IMAGE_E_NOMEM;
        }
        return NULL;
    }
    memcpy(d->data, data, len);
    d->len = len;
    d->sides = sides;

    for (s = 0; s < sides; s++) {
        size_t base = offset + (size_t)s * FDS_SIDE_BYTES;
        size_t avail;
        int rc;
        if (base >= len) {
            break;
        }
        avail = len - base;
        if (avail > FDS_SIDE_BYTES) {
            avail = FDS_SIDE_BYTES;
        }
        rc = parse_side(d, base, avail);
        if (rc != EZHLE_FDS_IMAGE_OK) {
            ezhle_fds_image_close(d);
            if (err != NULL) {
                *err = rc;
            }
            return NULL;
        }
    }

    if (d->count == 0) {
        ezhle_fds_image_close(d);
        return NULL;
    }
    if (err != NULL) {
        *err = EZHLE_FDS_IMAGE_OK;
    }
    return d;
}

void ezhle_fds_image_close(ezhle_fds_image *disk)
{
    if (disk == NULL) {
        return;
    }
    free(disk->files);
    free(disk->data);
    free(disk);
}

size_t ezhle_fds_image_file_count(const ezhle_fds_image *disk)
{
    return disk == NULL ? 0 : disk->count;
}

int ezhle_fds_image_set_file_count(ezhle_fds_image *disk, size_t count)
{
    if (disk == NULL || count > FDS_MAX_FILES) {
        return -1;
    }
    disk->count = count;
    disk->first_side_count = count;
    if (disk->has_file_count && disk->file_count_offset < disk->len) {
        disk->data[disk->file_count_offset] = (uint8_t)count;
    }
    return 0;
}

size_t ezhle_fds_image_first_side_file_count(const ezhle_fds_image *disk)
{
    return disk == NULL ? 0 : disk->first_side_count;
}

const ezhle_fds_image_file *ezhle_fds_image_file_at(const ezhle_fds_image *disk, size_t index)
{
    if (disk == NULL || index >= disk->count) {
        return NULL;
    }
    return &disk->files[index];
}

const uint8_t *ezhle_fds_image_file_data(const ezhle_fds_image *disk, const ezhle_fds_image_file *file)
{
    if (disk == NULL || file == NULL || file->data_offset + file->size > disk->len) {
        return NULL;
    }
    return disk->data + file->data_offset;
}

const ezhle_fds_image_file *ezhle_fds_image_find(const ezhle_fds_image *disk, uint8_t id)
{
    size_t i;
    if (disk == NULL) {
        return NULL;
    }
    for (i = 0; i < disk->count; i++) {
        if (disk->files[i].id == id) {
            return &disk->files[i];
        }
    }
    return NULL;
}

uint8_t ezhle_fds_image_boot_read_code(const ezhle_fds_image *disk)
{
    return disk == NULL ? 0xFF : disk->boot_code;
}

uint8_t ezhle_fds_image_side_count(const ezhle_fds_image *disk)
{
    return disk == NULL ? 0 : disk->sides;
}
