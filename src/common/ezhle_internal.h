#ifndef EZHLE_INTERNAL_H
#define EZHLE_INTERNAL_H

#include "ezhle/ezhle.h"

struct ezhle_core {
    ezhle_desc_t desc;
    char error[192];
    int exited;
    int32_t exit_code;
    void *state;
    void (*reset_fn)(ezhle_core_t *core);
    int (*dispatch_fn)(ezhle_core_t *core, ezhle_regs_t *regs);
    void (*destroy_fn)(ezhle_core_t *core);
};

void ezhle_set_error(ezhle_core_t *core, const char *fmt, ...);

int ezhle_ps2_attach(ezhle_core_t *core);
int ezhle_fds_attach(ezhle_core_t *core);

/* Internal test probe (not part of the guest ABI and not exported to include/):
 * return the root function captured by PS2 SetupThread's fifth argument, which
 * PS2SDK crt0.c passes in $8. `*found` is set to 1 only when a thread was
 * created by SetupThread. Used by tests/test_main.c to prove the handler reads
 * r[8] instead of ignoring it. */
uint32_t ezhle_ps2_test_thread_func(ezhle_core_t *core, int *found);

#endif
