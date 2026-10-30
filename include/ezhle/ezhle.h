#ifndef EZHLE_EZHLE_H
#define EZHLE_EZHLE_H

#include <stddef.h>
#include <stdint.h>

/*
 * ABI version of ezhle_desc_t. Bump on ANY layout change (size or field order).
 *   1 = original 24-byte descriptor (system, build_id, bus pointers).
 *   2 = prologue (magic, abi_version, struct_size) in front of the ABI-1 body
 *       fields; sizeof(ezhle_desc_t) = 192 (16-byte prologue + 176-byte body).
 * The library rejects descriptors whose magic or abi_version do not match, or
 * whose struct_size is below EZHLE_DESC_MIN_SIZE, before it reads any body byte.
 */
#define EZHLE_ABI_MAGIC 0x455A484Cu
#define EZHLE_ABI_VERSION 2u

enum ezhle_status {
    EZHLE_OK = 0,
    EZHLE_ERR_ARG = -1,
    EZHLE_ERR_NOMEM = -2,
    EZHLE_ERR_UNIMPL = -3,
    EZHLE_ERR_FAULT = -4,
    EZHLE_ERR_STATE = -5
};

typedef struct ezhle_core ezhle_core_t;

typedef struct ezhle_regs {
    uint32_t r[32];
    uint32_t pc;
    uint32_t hi;
    uint32_t lo;
} ezhle_regs_t;

/*
 * Guest memory and logging hooks. The core owns the memory map; the HLE
 * library never dereferences guest pointers directly.
 *
 * All callbacks return 0 on success and non-zero on fault.
 */
typedef struct ezhle_bus {
    int (*read32)(void *ctx, uint32_t addr, uint32_t *out);
    int (*write32)(void *ctx, uint32_t addr, uint32_t value);
    int (*read_bytes)(void *ctx, uint32_t addr, void *dst, size_t len);
    void (*log)(void *ctx, int level, const char *message);
    void (*trace)(void *ctx, uint32_t nr, const uint32_t *args, size_t argc,
                  int32_t ret);
    void *ctx;
} ezhle_bus_t;


/* Scheduler intent callbacks. The HLE library never blocks a host thread; it
 * reports that a guest thread would block, and the core decides when to
 * re-dispatch. All fields are optional (NULL means "no scheduler hooked"). */
typedef struct ezhle_sched {
    union {
        void (*yield)(void *ctx, int reason, uint32_t thread_id);
        void (*yield_fn)(void *ctx, int reason, uint32_t thread_id);
        void (*on_yield)(void *ctx, int reason, uint32_t thread_id);
        void (*sched_yield)(void *ctx, int reason, uint32_t thread_id);
    };
    union {
        void (*make_ready)(void *ctx, uint32_t thread_id);
        void (*make_ready_fn)(void *ctx, uint32_t thread_id);
        void (*on_make_ready)(void *ctx, uint32_t thread_id);
        void (*sched_make_ready)(void *ctx, uint32_t thread_id);
    };
    union {
        void *ctx;
        void *context;
        void *user;
        void *user_ctx;
        void *user_data;
    };
} ezhle_sched_t;

/* Scheduler yield reasons (informational; the core may ignore them). */
enum ezhle_sched_reason {
    EZHLE_SCHED_BLOCKED = 0,
    EZHLE_SCHED_SLEEP = 1,
    EZHLE_SCHED_WAIT_SEMA = 2,
    EZHLE_SCHED_WAIT_EVENTFLAG = 3
};

/* LAYOUT CONTRACT: any change to the size or field order of ezhle_desc_t
 * requires an EZHLE_ABI_VERSION bump. See docs/ARCHITECTURE.md section 3. */
/* Blocking semantics (PS2 scheduler contract):
 *   - A call that cannot proceed reports would-block -3 (PS2_EBUSY), except
 *     SleepThread, which returns 0: WaitSema on an empty semaphore returns -3
 *     and yields. (SleepThread sets THS_WAIT, yields once and returns 0;
 *     event flags are not implemented in this HLE.)
 *   - ReleaseWaitThread on a thread that is not waiting returns -3 with no
 *     state change (error, not silent no-op).
 *   - SignalSema publishes the token: the count is incremented by one up to
 *     max_count (returning -3 when already at max_count) and one waiter, if
 *     any, is made READY, rather than handing the token directly to a waiter. */
typedef struct ezhle_desc {
    uint32_t magic;        /* EZHLE_ABI_MAGIC */
    uint32_t abi_version;  /* EZHLE_ABI_VERSION */
    uint32_t struct_size;  /* sizeof(ezhle_desc_t) the caller compiled against */
    const char *system;    /* "ps2" */
    const char *build_id;  /* provenance tag, e.g. git describe */
    const ezhle_bus_t *bus;
    /* Scheduler intent surface (optional). Either a packed struct or the
     * individual flat callbacks below may be used.
     *
     * ALIAS PRECEDENCE (packed group wins over flat callbacks):
     *   - The packed ezhle_sched_t * alias group
     *     (sched / scheduler / sched_cb / sched_callbacks / thread_sched /
     *     sched_ops) wins over the flat callback fields (yield, yield_fn,
     *     on_yield, sched_yield, make_ready, ...). A non-NULL packed group
     *     suppresses the flat fallback entirely: even a NULL member (e.g.
     *     sched->yield == NULL) is not filled from a flat callback that is
     *     set (packed sched with sched->yield == NULL plus a flat yield set
     *     yields 0 yields).
     *   - Within the alias group, the first non-NULL alias wins.
     *   - Within the flat group, the first non-NULL callback wins.
     *   - ctx resolution: sched->ctx first, then sched_ctx, context, user_ctx,
     *     user_data, ctx, then bus->ctx. */
    const ezhle_sched_t *sched;
    const ezhle_sched_t *scheduler;
    const ezhle_sched_t *sched_cb;
    const ezhle_sched_t *sched_callbacks;
    const ezhle_sched_t *thread_sched;
    const ezhle_sched_t *sched_ops;
    void (*yield)(void *ctx, int reason, uint32_t thread_id);
    void (*yield_fn)(void *ctx, int reason, uint32_t thread_id);
    void (*on_yield)(void *ctx, int reason, uint32_t thread_id);
    void (*sched_yield)(void *ctx, int reason, uint32_t thread_id);
    void (*make_ready)(void *ctx, uint32_t thread_id);
    void (*make_ready_fn)(void *ctx, uint32_t thread_id);
    void (*on_make_ready)(void *ctx, uint32_t thread_id);
    void (*sched_make_ready)(void *ctx, uint32_t thread_id);
    void *sched_ctx;
    void *context;
    void *user_ctx;
    void *user_data;
    void *ctx;
} ezhle_desc_t;

/*
 * Minimum bytes an ezhle_desc_t must declare. A descriptor whose struct_size is
 * below this is rejected: it was compiled against an older layout that does not
 * even reach the scheduler surface.
 */
#define EZHLE_DESC_MIN_SIZE (offsetof(ezhle_desc_t, sched) + sizeof(const ezhle_sched_t *))

/*
 * Construction macro. Sets the ABI prologue (magic, abi_version, struct_size)
 * plus the core identity fields. Use this at every descriptor construction site
 * so the prologue is never hand-edited or forgotten.
 *
 * Forward compatibility: when struct_size > sizeof(ezhle_desc_t), ezhle_create
 * copies only sizeof(ezhle_desc_t) bytes (min of the two). The extra declared
 * bytes are caller-owned padding/future fields and are never read.
 */
#define EZHLE_DESC_INIT(desc, system_, build_id_, bus_) do {            \
    memset((desc), 0, sizeof(*(desc)));                                 \
    (desc)->magic = EZHLE_ABI_MAGIC;                                    \
    (desc)->abi_version = EZHLE_ABI_VERSION;                            \
    (desc)->struct_size = sizeof(*(desc));                               \
    (desc)->system = (system_);                                         \
    (desc)->build_id = (build_id_);                                     \
    (desc)->bus = (bus_);                                               \
} while (0)


ezhle_core_t *ezhle_create(const ezhle_desc_t *desc);
void ezhle_destroy(ezhle_core_t *core);
void ezhle_reset(ezhle_core_t *core);

/*
 * Dispatch one firmware call. The core sets the syscall number and argument
 * registers, calls this function, then reads the return register.
 *
 * Returns EZHLE_OK when the call was handled, EZHLE_ERR_UNIMPL when no handler
 * exists (the guest return register is set to -1), or another negative status
 * on a fault.
 */
int ezhle_dispatch(ezhle_core_t *core, ezhle_regs_t *regs);

int ezhle_exited(const ezhle_core_t *core);
int32_t ezhle_exit_code(const ezhle_core_t *core);
const char *ezhle_last_error(const ezhle_core_t *core);
const char *ezhle_system(const ezhle_core_t *core);

#endif
