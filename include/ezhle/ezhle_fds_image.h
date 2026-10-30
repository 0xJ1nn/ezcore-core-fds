/* SPDX-License-Identifier: BSD-2-Clause */
/* ezCORE HLE — Famicom Disk System image parser
 * Provenance: clean-room implementation from public format documentation.
 * Sources: https://www.nesdev.org/wiki/FDS_file_format
 *          https://www.nesdev.org/wiki/FDS_disk_format
 * Authors: ezCORE contributors, 2026-10-30
 * Reviewed-by: pending independent review (G4)
 */
#ifndef EZHLE_FDS_IMAGE_H
#define EZHLE_FDS_IMAGE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum ezhle_fds_result {
    EZHLE_FDS_IMAGE_OK = 0,
    EZHLE_FDS_IMAGE_E_FORMAT = 1,
    EZHLE_FDS_IMAGE_E_NOMEM = 2,
    EZHLE_FDS_IMAGE_E_RANGE = 3
} ezhle_fds_result;

typedef struct ezhle_fds_image_file {
    uint8_t id;
    uint8_t number;
    uint8_t type;        /* 0 program, 1 character, 2 nametable */
    char name[9];        /* NUL-terminated copy of the 8-byte name */
    uint16_t address;    /* load address */
    uint16_t size;
    size_t data_offset;  /* offset into the parser-owned image buffer */
} ezhle_fds_image_file;

typedef struct ezhle_fds_image ezhle_fds_image;

/* Parse an .fds image (with or without the 16-byte fwNES header). The parser
 * copies the image, so the caller may free its buffer after this returns.
 * Returns NULL on failure and writes a result code to *err when err != NULL. */
ezhle_fds_image *ezhle_fds_image_open(const uint8_t *data, size_t len, int *err);
void ezhle_fds_image_close(ezhle_fds_image *disk);

size_t ezhle_fds_image_file_count(const ezhle_fds_image *disk);
size_t ezhle_fds_image_first_side_file_count(const ezhle_fds_image *disk);
const ezhle_fds_image_file *ezhle_fds_image_file_at(const ezhle_fds_image *disk, size_t index);
const uint8_t *ezhle_fds_image_file_data(const ezhle_fds_image *disk, const ezhle_fds_image_file *file);
const ezhle_fds_image_file *ezhle_fds_image_find(const ezhle_fds_image *disk, uint8_t id);
uint8_t ezhle_fds_image_boot_read_code(const ezhle_fds_image *disk);
uint8_t ezhle_fds_image_side_count(const ezhle_fds_image *disk);

#ifdef __cplusplus
}
#endif

#endif /* EZHLE_FDS_IMAGE_H */
