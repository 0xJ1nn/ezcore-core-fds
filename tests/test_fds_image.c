/* SPDX-License-Identifier: BSD-2-Clause */
/* ezCORE HLE — FDS image parser tests
 * Provenance: original test code from public format documentation.
 * Sources: https://www.nesdev.org/wiki/FDS_disk_format
 * Authors: Hermes Agent, 2026-10-30
 */
#include "ezhle/ezhle_fds_image.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SIDE_BYTES 65500u
#define HEADER_BYTES 16u

static int failures = 0;

#define CHECK(cond) do { \
    if (!(cond)) { \
        printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        failures++; \
    } \
} while (0)

static void put16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)(v >> 8);
}

static void build_side(uint8_t *side, unsigned files, uint8_t boot_code, uint8_t first_id)
{
    size_t pos = 0;
    unsigned i;

    memset(side, 0, SIDE_BYTES);
    side[pos] = 0x01;
    memcpy(side + pos + 1, "*NINTENDO-HVC*", 14);
    side[pos + 0x0F] = 0x01;       /* licensee */
    memcpy(side + pos + 0x10, "TST", 3);
    side[pos + 0x13] = 0x20;       /* normal disk */
    side[pos + 0x14] = 0x00;       /* version */
    side[pos + 0x15] = 0x00;       /* side A */
    side[pos + 0x16] = 0x00;       /* disk 0 */
    side[pos + 0x17] = 0x00;
    side[pos + 0x18] = 0x00;
    side[pos + 0x19] = boot_code;
    pos += 0x38;

    side[pos] = 0x02;
    side[pos + 1] = (uint8_t)files;
    pos += 2;

    for (i = 0; i < files; i++) {
        side[pos] = 0x03;
        side[pos + 1] = (uint8_t)i;
        side[pos + 2] = (uint8_t)(first_id + i);
        memcpy(side + pos + 3, "TEST    ", 8);
        put16(side + pos + 0x0B, (uint16_t)(0x6000u + i * 0x10u));
        put16(side + pos + 0x0D, 4);
        side[pos + 0x0F] = 0x00;
        pos += 0x10;

        side[pos] = 0x04;
        memcpy(side + pos + 1, "ABCD", 4);
        pos += 5;
    }
}

static uint8_t *make_image(unsigned sides, uint8_t header)
{
    size_t total = (header ? HEADER_BYTES : 0) + (size_t)sides * SIDE_BYTES;
    uint8_t *img = (uint8_t *)calloc(1, total);
    size_t off = 0;
    unsigned s;

    if (img == NULL) {
        return NULL;
    }
    if (header) {
        img[0] = 'F';
        img[1] = 'D';
        img[2] = 'S';
        img[3] = 0x1A;
        img[4] = (uint8_t)sides;
        off = HEADER_BYTES;
    }
    for (s = 0; s < sides; s++) {
        build_side(img + off + (size_t)s * SIDE_BYTES, 1, 2, (uint8_t)(1 + s));
    }
    return img;
}

int main(void)
{
    int err = 0;
    uint8_t *img;
    ezhle_fds_image *disk;
    const ezhle_fds_image_file *f;

    /* 1. Header image, one side, one file. */
    img = make_image(1, 1);
    CHECK(img != NULL);
    disk = ezhle_fds_image_open(img, HEADER_BYTES + SIDE_BYTES, &err);
    CHECK(disk != NULL);
    CHECK(err == EZHLE_FDS_IMAGE_OK);
    CHECK(ezhle_fds_image_file_count(disk) == 1);
    CHECK(ezhle_fds_image_boot_read_code(disk) == 2);
    CHECK(ezhle_fds_image_side_count(disk) == 1);
    f = ezhle_fds_image_file_at(disk, 0);
    CHECK(f != NULL);
    CHECK(f->id == 1);
    CHECK(f->address == 0x6000);
    CHECK(f->size == 4);
    CHECK(memcmp(f->name, "TEST", 4) == 0);
    CHECK(memcmp(ezhle_fds_image_file_data(disk, f), "ABCD", 4) == 0);
    CHECK(ezhle_fds_image_find(disk, 1) == f);
    CHECK(ezhle_fds_image_find(disk, 2) == NULL);
    ezhle_fds_image_close(disk);
    free(img);

    /* 2. Headerless image. */
    img = make_image(1, 0);
    CHECK(img != NULL);
    disk = ezhle_fds_image_open(img, SIDE_BYTES, &err);
    CHECK(disk != NULL);
    CHECK(ezhle_fds_image_file_count(disk) == 1);
    ezhle_fds_image_close(disk);
    free(img);

    /* 3. Corrupt magic is rejected. */
    img = make_image(1, 1);
    CHECK(img != NULL);
    img[HEADER_BYTES + 1] = 'X';
    disk = ezhle_fds_image_open(img, HEADER_BYTES + SIDE_BYTES, &err);
    CHECK(disk == NULL);
    CHECK(err == EZHLE_FDS_IMAGE_E_FORMAT);
    free(img);

    /* 4. Truncated file data is rejected. */
    img = make_image(1, 1);
    CHECK(img != NULL);
    {
        uint8_t *side = img + HEADER_BYTES;
        size_t hdr = 0x38 + 2;
        put16(side + hdr + 0x0B, 0);
        put16(side + hdr + 0x0D, 0xFFF0);
    }
    disk = ezhle_fds_image_open(img, HEADER_BYTES + SIDE_BYTES, &err);
    CHECK(disk == NULL);
    CHECK(err == EZHLE_FDS_IMAGE_E_FORMAT);
    free(img);

    /* 5. Two sides, one file each. */
    img = make_image(2, 1);
    CHECK(img != NULL);
    disk = ezhle_fds_image_open(img, HEADER_BYTES + 2 * SIDE_BYTES, &err);
    CHECK(disk != NULL);
    CHECK(ezhle_fds_image_side_count(disk) == 2);
    CHECK(ezhle_fds_image_file_count(disk) == 2);
    CHECK(ezhle_fds_image_find(disk, 1) != NULL);
    CHECK(ezhle_fds_image_find(disk, 2) != NULL);
    ezhle_fds_image_close(disk);
    free(img);

    /* 6. Invalid arguments. */
    CHECK(ezhle_fds_image_open(NULL, 0, &err) == NULL);
    CHECK(ezhle_fds_image_file_count(NULL) == 0);
    CHECK(ezhle_fds_image_file_at(NULL, 0) == NULL);
    CHECK(ezhle_fds_image_file_data(NULL, NULL) == NULL);

    if (failures == 0) {
        printf("fds_image_test: PASS (parse, headerless, magic, truncation, sides)\n");
        return 0;
    }
    printf("fds_image_test: FAILED (%d checks)\n", failures);
    return 1;
}
