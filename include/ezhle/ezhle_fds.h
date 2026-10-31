#ifndef EZHLE_EZHLE_FDS_H
#define EZHLE_EZHLE_FDS_H

#include "ezhle/ezhle.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Famicom Disk System BIOS HLE.
 *
 * The FDS BIOS is an 8 KiB ROM at $E000-$FFFF. This header describes the
 * 6502 register/status surface that a core passes to ezhle_dispatch() and the
 * documented BIOS entry-point addresses (public NESdev FDS documentation).
 *
 * Sources: docs/SOURCES.md (S10, S11).
 */

#define EZHLE_FDS_BIOS_BASE 0xE000u
#define EZHLE_FDS_BIOS_SIZE 0x2000u

/* Reset vector location (little-endian 16-bit address). */
#define EZHLE_FDS_RESET_VECTOR_ADDR 0xFFFCu

/* 6502 registers are passed in ezhle_regs_t.r[] by these indices. */
#define EZHLE_FDS_REG_A 0u /* accumulator */
#define EZHLE_FDS_REG_X 1u /* X index */
#define EZHLE_FDS_REG_Y 2u /* Y index */
#define EZHLE_FDS_REG_P 3u /* processor status */
#define EZHLE_FDS_REG_SP 4u /* stack pointer */

/* Processor status flags. */
#define EZHLE_FDS_FLAG_C 0x01u
#define EZHLE_FDS_FLAG_Z 0x02u
#define EZHLE_FDS_FLAG_I 0x04u
#define EZHLE_FDS_FLAG_D 0x08u
#define EZHLE_FDS_FLAG_B 0x10u
#define EZHLE_FDS_FLAG_V 0x40u
#define EZHLE_FDS_FLAG_N 0x80u

/*
 * A documented BIOS entry point. `address` is the 6502 address a guest JSRs to;
 * `name` is the public routine name; `implemented` is non-zero for the routines
 * that have a real handler in this pilot, zero for stubs.
 */
typedef struct ezhle_fds_entry {
    uint16_t address;
    const char *name;
    int implemented;
} ezhle_fds_entry_t;

/* Read-only table of documented entry points (see fds_hle.c). */
const ezhle_fds_entry_t *ezhle_fds_entry_table(void);
size_t ezhle_fds_entry_count(void);

/* Look up an entry by address; returns NULL when unknown. */
const ezhle_fds_entry_t *ezhle_fds_entry_find(uint16_t address);

/* Read the reset vector ($FFFC/$FFFD, little-endian) through the bus. */
uint16_t ezhle_fds_reset_vector(ezhle_core_t *core);

/* Vector-control bytes at the bottom of the stack page. These select how the
 * BIOS dispatches NMI/IRQ (including during disk access). Source: NESdev FDS
 * BIOS, "Interrupt/Reset vector controls". */
#define EZHLE_FDS_NMI_CONTROL_ADDR 0x0100u
#define EZHLE_FDS_IRQ_CONTROL_ADDR 0x0101u
#define EZHLE_FDS_RESET_FLAG_ADDR 0x0102u
#define EZHLE_FDS_RESET_TYPE_ADDR 0x0103u

/* Game interrupt vectors selected by the control bytes above. */
#define EZHLE_FDS_GAME_NMI_VECTOR_1_ADDR 0xDFF6u
#define EZHLE_FDS_GAME_NMI_VECTOR_2_ADDR 0xDFF8u
#define EZHLE_FDS_GAME_NMI_VECTOR_3_ADDR 0xDFFAu
#define EZHLE_FDS_GAME_IRQ_VECTOR_ADDR 0xDFFEu
#define EZHLE_FDS_GAME_RESET_VECTOR_ADDR 0xDFFCu

/* CPU interrupt vector locations. */
#define EZHLE_FDS_NMI_CPU_VECTOR_ADDR 0xFFFAu
#define EZHLE_FDS_IRQ_CPU_VECTOR_ADDR 0xFFFEu

/* BIOS handlers selected by the vector-control bytes. */
#define EZHLE_FDS_VINTWAIT_HANDLER_ADDR 0xE19Du
#define EZHLE_FDS_DISK_SKIP_BYTES_ADDR 0xE1D9u
#define EZHLE_FDS_DISK_TRANSFER_ADDR 0xE1CEu
#define EZHLE_FDS_ACKNOWLEDGE_DELAY_ADDR 0xE1EFu

/* Read the NMI/IRQ vector-control bytes through the bus. */
uint8_t ezhle_fds_nmi_control(ezhle_core_t *core);
uint8_t ezhle_fds_irq_control(ezhle_core_t *core);

/* Resolve the effective NMI/IRQ handler address from the vector-control byte. */
uint16_t ezhle_fds_nmi_vector(ezhle_core_t *core);
uint16_t ezhle_fds_irq_vector(ezhle_core_t *core);

/* Aliases for the vector-resolution helpers (same behaviour). */
uint16_t ezhle_fds_nmi_target(ezhle_core_t *core);
uint16_t ezhle_fds_irq_target(ezhle_core_t *core);
uint16_t ezhle_fds_nmi_handler(ezhle_core_t *core);
uint16_t ezhle_fds_irq_handler(ezhle_core_t *core);
uint16_t ezhle_fds_nmi_dispatch(ezhle_core_t *core);
uint16_t ezhle_fds_irq_dispatch(ezhle_core_t *core);
uint16_t ezhle_fds_resolve_nmi(ezhle_core_t *core);
uint16_t ezhle_fds_resolve_irq(ezhle_core_t *core);

/* Read the CPU NMI/IRQ vector words ($FFFA/$FFFE) through the bus. */
uint16_t ezhle_fds_nmi_cpu_vector(ezhle_core_t *core);
uint16_t ezhle_fds_irq_cpu_vector(ezhle_core_t *core);

/*
 * Convenience: set regs->pc to `entry` and dispatch. Returns the same status as
 * ezhle_dispatch().
 */
int ezhle_fds_call(ezhle_core_t *core, ezhle_regs_t *regs, uint16_t entry);

/* Attach/detach a parsed FDS disk image to a core's FDS state.
 * Returns EZHLE_OK on success, EZHLE_ERR_ARG for bad arguments,
 * EZHLE_ERR_STATE when the image cannot be parsed (state untouched). */
int ezhle_fds_attach_disk(ezhle_core_t *core, const uint8_t *data, size_t len);
void ezhle_fds_detach_disk(ezhle_core_t *core);

#ifdef __cplusplus
}
#endif

#endif /* EZHLE_EZHLE_FDS_H */