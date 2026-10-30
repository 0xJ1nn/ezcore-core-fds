/* SPDX-License-Identifier: BSD-2-Clause */
/* ezCORE HLE — FDS image parser fuzz harness
 * Provenance: original code; uses only synthetic data. No firmware/ROM/BIOS.
 */
#include "ezhle/ezhle_fds_image.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

static int fuzz_one(const uint8_t *data, size_t len) {
    int err = 0;
    ezhle_fds_image *disk = ezhle_fds_image_open(data, len, &err);
    if (disk == NULL) {
        return 0;
    }
    (void)ezhle_fds_image_file_count(disk);
    (void)ezhle_fds_image_first_side_file_count(disk);
    (void)ezhle_fds_image_boot_read_code(disk);
    (void)ezhle_fds_image_side_count(disk);
    {
        size_t count = ezhle_fds_image_file_count(disk);
        size_t i;
        for (i = 0; i < count; i++) {
            const ezhle_fds_image_file *f = ezhle_fds_image_file_at(disk, i);
            if (f != NULL) {
                (void)ezhle_fds_image_file_data(disk, f);
                (void)f->id;
                (void)f->number;
                (void)f->type;
                (void)f->name[0];
                (void)f->address;
                (void)f->size;
            }
        }
    }
    /* Try to find a couple ids */
    (void)ezhle_fds_image_find(disk, 0);
    (void)ezhle_fds_image_find(disk, 1);
    (void)ezhle_fds_image_find(disk, 255);
    ezhle_fds_image_close(disk);
    return 0;
}

#if defined(__AFL_COMPILER)
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t len) {
    return fuzz_one(data, len);
}
#else
# ifdef USE_LIBFUZZER
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t len) {
    return fuzz_one(data, len);
}
# else
int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "--random") == 0) {
        unsigned i;
        srand(0xE2C0u);
        for (i = 0; i < 20000u; i++) {
            size_t len = (size_t)(rand() % 8192);
            uint8_t *buf = (uint8_t *)malloc(len ? len : 1);
            size_t j;
            if (buf == NULL) {
                return 2;
            }
            for (j = 0; j < len; j++) {
                buf[j] = (uint8_t)rand();
            }
            (void)fuzz_one(buf, len);
            free(buf);
        }
        printf("random fuzz: 20000 inputs OK\n");
        return 0;
    }
    if (argc < 2) {
        fprintf(stderr, "usage: %s <file>\n", argv[0]);
        return 2;
    }
    FILE *f = fopen(argv[1], "rb");
    if (f == NULL) {
        return 2;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return 2;
    }
    long sz = ftell(f);
    if (sz < 0) {
        fclose(f);
        return 2;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return 2;
    }
    size_t len = (size_t)sz;
    uint8_t *buf = NULL;
    if (len > 0) {
        buf = (uint8_t *)malloc(len);
        if (buf == NULL) {
            fclose(f);
            return 2;
        }
        if (fread(buf, 1, len, f) != len) {
            free(buf);
            fclose(f);
            return 2;
        }
    }
    fclose(f);
    (void)fuzz_one(buf, len);
    free(buf);
    return 0;
}
# endif
#endif

#ifdef __cplusplus
}
#endif
