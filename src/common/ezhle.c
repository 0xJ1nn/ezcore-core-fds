#include "common/ezhle_internal.h"
#include "ezhle/ezhle_fds.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char ezhle_global_error[192] = "";

void ezhle_set_error(ezhle_core_t *core, const char *fmt, ...)
{
    va_list ap;
    char *buf;
    size_t sz;

    if (core != NULL) {
        buf = core->error;
        sz = sizeof(core->error);
    } else {
        buf = ezhle_global_error;
        sz = sizeof(ezhle_global_error);
    }
    va_start(ap, fmt);
    (void)vsnprintf(buf, sz, fmt, ap);
    va_end(ap);
}

ezhle_core_t *ezhle_create(const ezhle_desc_t *desc)
{
    ezhle_core_t *core;
    size_t n;

    if (desc == NULL) {
        ezhle_set_error(NULL, "ezhle_create: desc is NULL");
        return NULL;
    }
    if (desc->magic != EZHLE_ABI_MAGIC) {
        ezhle_set_error(NULL, "ezhle_create: bad abi magic 0x%08x", desc->magic);
        return NULL;
    }
    if (desc->abi_version != EZHLE_ABI_VERSION) {
        ezhle_set_error(NULL, "ezhle_create: abi version mismatch (got %u, want %u)",
                        desc->abi_version, EZHLE_ABI_VERSION);
        return NULL;
    }
    if (desc->struct_size < EZHLE_DESC_MIN_SIZE) {
        ezhle_set_error(NULL, "ezhle_create: struct_size %u below minimum %u",
                        desc->struct_size, (unsigned)EZHLE_DESC_MIN_SIZE);
        return NULL;
    }
    if (desc->system == NULL) {
        ezhle_set_error(NULL, "ezhle_create: system is required");
        return NULL;
    }

    core = calloc(1, sizeof(*core));
    if (core == NULL) {
        ezhle_set_error(NULL, "ezhle_create: out of memory");
        return NULL;
    }

    n = desc->struct_size;
    if (n > sizeof(core->desc)) {
        n = sizeof(core->desc);
    }
    memcpy(&core->desc, desc, n);

    if (strcmp(desc->system, "ps2") == 0) {
        if (ezhle_ps2_attach(core) != EZHLE_OK) {
            ezhle_set_error(NULL, "%s", core->error[0] != '\0' ? core->error
                                                              : "ps2 attach failed");
            free(core);
            return NULL;
        }
    } else if (strcmp(desc->system, "fds") == 0) {
        if (ezhle_fds_attach(core) != EZHLE_OK) {
            ezhle_set_error(NULL, "%s", core->error[0] != '\0' ? core->error
                                                              : "fds attach failed");
            free(core);
            return NULL;
        }
    } else {
        ezhle_set_error(core, "unknown system: %s", desc->system);
        ezhle_set_error(NULL, "%s", core->error);
        free(core);
        return NULL;
    }

    return core;
}

void ezhle_destroy(ezhle_core_t *core)
{
    if (core == NULL) {
        return;
    }
    if (core->destroy_fn != NULL) {
        core->destroy_fn(core);
    }
    free(core);
}

void ezhle_reset(ezhle_core_t *core)
{
    if (core == NULL) {
        return;
    }
    core->error[0] = '\0';
    ezhle_global_error[0] = '\0';
    core->exited = 0;
    core->exit_code = 0;
    if (core->reset_fn != NULL) {
        core->reset_fn(core);
    }
}

int ezhle_dispatch(ezhle_core_t *core, ezhle_regs_t *regs)
{
    if (core == NULL || regs == NULL) {
        return EZHLE_ERR_ARG;
    }
    if (core->exited != 0) {
        ezhle_set_error(core, "dispatch after guest exit");
        return EZHLE_ERR_STATE;
    }
    if (core->dispatch_fn == NULL) {
        ezhle_set_error(core, "no dispatch backend");
        return EZHLE_ERR_UNIMPL;
    }
    return core->dispatch_fn(core, regs);
}

int ezhle_exited(const ezhle_core_t *core)
{
    return core != NULL ? core->exited : 0;
}

int32_t ezhle_exit_code(const ezhle_core_t *core)
{
    return core != NULL ? core->exit_code : -1;
}

const char *ezhle_last_error(const ezhle_core_t *core)
{
    if (core != NULL) {
        return core->error;
    }
    return ezhle_global_error;
}

const char *ezhle_system(const ezhle_core_t *core)
{
    return core != NULL ? core->desc.system : NULL;
}
