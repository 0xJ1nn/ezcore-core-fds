/* FDS repository: PS2 is a different HLE repo. Keep the shared dispatcher
 * linkable without dragging PS2 code into this package. */
#include "common/ezhle_internal.h"

int ezhle_ps2_attach(ezhle_core_t *core)
{
    (void)core;
    return EZHLE_ERR_UNIMPL;
}
