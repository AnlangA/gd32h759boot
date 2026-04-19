/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2024 GD32H759 Bootloader Project
 *
 * Flash map backend implementation for GD32H7xx internal flash.
 *
 * This file implements the flash abstraction layer that MCUBoot's bootutil
 * library calls. Every operation ultimately maps to GD32H7xx FMC (Flash
 * Memory Controller) peripheral calls via the GD32 standard peripheral library.
 *
 * Reference: GD32H7xx User Manual
 *   - FMC chapter for program / erase sequences
 *   - gd32h7xx_fmc.h / gd32h7xx_fmc.c for HAL API
 *
 * Flash geometry (GD32H759):
 *   - Flash: 0x0800_0000, up to 3840 KB
 *   - Sector size (erase granularity): 4 KB
 *   - Minimum write granularity: 8 bytes (64-bit double word)
 *   - Erased byte value: 0xFF
 */

#include <flash_map_backend/flash_map_backend.h>
#include <sysflash/sysflash.h>
#include <mcuboot_config/mcuboot_config.h>
#include <mcuboot_config/mcuboot_logging.h>

#include "gd32h7xx.h"
#include "gd32h7xx_fmc.h"

#include <string.h>

/* ======================================================================== */
/*  Logging helpers                                                         */
/* ======================================================================== */
#define FLASH_LOG_ERR(...)  MCUBOOT_LOG_ERR(__VA_ARGS__)
#define FLASH_LOG_INF(...)  MCUBOOT_LOG_INF(__VA_ARGS__)

/* ======================================================================== */
/*  Internal flash base address                                             */
/* ======================================================================== */
#define INTERNAL_FLASH_BASE  0x08000000U

/* ======================================================================== */
/*  Static flash area descriptors                                           */
/* ======================================================================== */
/*
 * The table below mirrors the partition layout defined in sysflash.h.
 * Each entry maps a logical flash_area ID to a physical offset and size.
 *
 * IMPORTANT: Keep this table in sync with sysflash.h macros!
 */
static struct flash_area s_flash_areas[] = {
    /* id                                   device_id  pad   offset                         size */
    [FLASH_AREA_ID_BOOTLOADER] = {
        .fa_id        = FLASH_AREA_ID_BOOTLOADER,
        .fa_device_id = FLASH_DEVICE_INTERNAL_FLASH,
        .pad16        = 0,
        .fa_off       = FLASH_AREA_BOOTLOADER_OFFSET,
        .fa_size      = FLASH_AREA_BOOTLOADER_SIZE,
    },
    [FLASH_AREA_ID_IMAGE_PRIMARY] = {
        .fa_id        = FLASH_AREA_ID_IMAGE_PRIMARY,
        .fa_device_id = FLASH_DEVICE_INTERNAL_FLASH,
        .pad16        = 0,
        .fa_off       = FLASH_AREA_IMAGE_PRIMARY_OFFSET,
        .fa_size      = FLASH_AREA_IMAGE_PRIMARY_SIZE,
    },
    [FLASH_AREA_ID_IMAGE_SECONDARY] = {
        .fa_id        = FLASH_AREA_ID_IMAGE_SECONDARY,
        .fa_device_id = FLASH_DEVICE_INTERNAL_FLASH,
        .pad16        = 0,
        .fa_off       = FLASH_AREA_IMAGE_SECONDARY_OFFSET,
        .fa_size      = FLASH_AREA_IMAGE_SECONDARY_SIZE,
    },
#if defined(MCUBOOT_SWAP_USING_SCRATCH) || \
    defined(MCUBOOT_SWAP_USING_MOVE)    || \
    defined(MCUBOOT_SWAP_USING_OFFSET)
    [FLASH_AREA_ID_IMAGE_SCRATCH] = {
        .fa_id        = FLASH_AREA_ID_IMAGE_SCRATCH,
        .fa_device_id = FLASH_DEVICE_INTERNAL_FLASH,
        .pad16        = 0,
        .fa_off       = FLASH_AREA_IMAGE_SCRATCH_OFFSET,
        .fa_size      = FLASH_AREA_IMAGE_SCRATCH_SIZE,
    },
#endif
};

/* Number of entries in the flash area table */
#define FLASH_AREA_COUNT  (sizeof(s_flash_areas) / sizeof(s_flash_areas[0]))

/* ======================================================================== */
/*  Private helpers                                                         */
/* ======================================================================== */

/**
 * Convert a flash_area-relative offset to an absolute memory-mapped address.
 *
 * The entire internal flash is memory-mapped, so we can compute the
 * absolute address simply as:
 *   INTERNAL_FLASH_BASE + fa->fa_off + area_relative_offset
 */
static inline uint32_t fa_to_abs_addr(const struct flash_area *fa, uint32_t off)
{
    return INTERNAL_FLASH_BASE + fa->fa_off + off;
}

/**
 * Clear all FMC status flags before/after an operation.
 *
 * The GD32H7xx HAL does not provide fmc_all_flags_clear(), so we
 * clear each flag individually. This matches the official FMC example
 * which clears all flags between operations.
 */
static void fmc_clear_all_flags(void)
{
    fmc_flag_clear(FMC_FLAG_END);
    fmc_flag_clear(FMC_FLAG_WPERR);
    fmc_flag_clear(FMC_FLAG_PGSERR);
    fmc_flag_clear(FMC_FLAG_RPERR);
    fmc_flag_clear(FMC_FLAG_RSERR);
    fmc_flag_clear(FMC_FLAG_ECCCOR);
    fmc_flag_clear(FMC_FLAG_ECCDET);
    fmc_flag_clear(FMC_FLAG_OBMERR);
}

/**
 * Program 8 bytes (64-bit) to flash at the given absolute address.
 *
 * Caller must ensure FMC is unlocked before calling this function.
 *
 * @param abs_addr  Must be 8-byte aligned.
 * @param data      64-bit data word to program.
 * @return 0 on success, non-zero on failure.
 */
static int program_doubleword(uint32_t abs_addr, uint64_t data)
{
    fmc_state_enum fmc_state;

    fmc_state = fmc_doubleword_program(abs_addr, data);

    if (fmc_state != FMC_READY) {
        FLASH_LOG_ERR("fmc_doubleword_program failed at 0x%08lX, state=%d",
                      (unsigned long)abs_addr, (int)fmc_state);
        return -1;
    }

    /* Invalidate D-Cache for the written 8 bytes so read-back is correct */
    SCB_InvalidateDCache_by_Addr((uint32_t *)abs_addr, 8);

    /* Verify the written data */
    if (*(__IO uint64_t *)abs_addr != data) {
        FLASH_LOG_ERR("Flash verify failed at 0x%08lX", (unsigned long)abs_addr);
        return -1;
    }

    return 0;
}

/* ======================================================================== */
/*  Public API: Flash area lifecycle                                        */
/* ======================================================================== */

int flash_area_open(uint8_t id, const struct flash_area **area_outp)
{
    if (area_outp == NULL) {
        return -1;
    }

    if (id >= FLASH_AREA_COUNT || s_flash_areas[id].fa_size == 0) {
        FLASH_LOG_ERR("flash_area_open: invalid ID %u", id);
        return -1;
    }

    *area_outp = &s_flash_areas[id];
    return 0;
}

void flash_area_close(const struct flash_area *fa)
{
    (void)fa;
    /* No-op for internal memory-mapped flash */
}

/* ======================================================================== */
/*  Public API: Flash read                                                  */
/* ======================================================================== */

int flash_area_read(const struct flash_area *fa, uint32_t off,
                    void *dst, uint32_t len)
{
    uint32_t abs_addr;

    if (fa == NULL || dst == NULL) {
        return -1;
    }

    /* Bounds check (safe against uint32_t overflow) */
    if (len > fa->fa_size || off > fa->fa_size - len) {
        FLASH_LOG_ERR("flash_area_read: out of bounds (off=0x%lX len=%lu, size=0x%lX)",
                      (unsigned long)off, (unsigned long)len,
                      (unsigned long)fa->fa_size);
        return -1;
    }

    abs_addr = fa_to_abs_addr(fa, off);

    /*
     * Internal flash is directly memory-mapped, so a simple memcpy
     * is sufficient. No FMC unlock or special sequencing needed for reads.
     */
    memcpy(dst, (const void *)abs_addr, len);

    return 0;
}

/* ======================================================================== */
/*  Public API: Flash write                                                 */
/* ======================================================================== */

int flash_area_write(const struct flash_area *fa, uint32_t off,
                     const void *src, uint32_t len)
{
    const uint8_t *src_bytes = (const uint8_t *)src;
    uint32_t abs_addr;
    uint32_t remaining;
    int rc;

    if (fa == NULL || src == NULL) {
        return -1;
    }

    /* Bounds check (safe against uint32_t overflow) */
    if (len > fa->fa_size || off > fa->fa_size - len) {
        FLASH_LOG_ERR("flash_area_write: out of bounds");
        return -1;
    }

    abs_addr = fa_to_abs_addr(fa, off);
    remaining = len;

    fmc_unlock();
    fmc_clear_all_flags();

    /*
     * GD32H7xx FMC requires 8-byte (64-bit) aligned writes.
     *
     * Strategy:
     *   1. If the destination is not 8-byte aligned, handle the first
     *      few bytes with a read-modify-write of the containing 64-bit word.
     *   2. Write full 64-bit words for the bulk of the data.
     *   3. Handle trailing bytes (if any) with another read-modify-write.
     */

    /* --- Handle leading unaligned bytes --- */
    if (abs_addr & 0x7U) {
        uint32_t aligned_start = abs_addr & ~0x7U;
        uint32_t head_bytes = 8U - (abs_addr & 0x7U);
        uint64_t buf;

        if (head_bytes > remaining) {
            head_bytes = remaining;
        }

        /* Ensure D-Cache does not hold stale data for this word */
        SCB_InvalidateDCache_by_Addr((uint32_t *)aligned_start, 8);

        /* Read the existing 64-bit word */
        buf = *(__IO uint64_t *)aligned_start;

        /* Overlay the new bytes */
        memcpy((uint8_t *)&buf + (abs_addr & 0x7U), src_bytes, head_bytes);

        /* Write it back */
        rc = program_doubleword(aligned_start, buf);
        if (rc != 0) {
            FLASH_LOG_ERR("flash_area_write: head write failed");
            fmc_clear_all_flags();
            fmc_lock();
            return rc;
        }

        src_bytes  += head_bytes;
        abs_addr   += head_bytes;
        remaining  -= head_bytes;
    }

    /* --- Write aligned 64-bit words --- */
    while (remaining >= 8U) {
        uint64_t word;
        memcpy(&word, src_bytes, 8);

        rc = program_doubleword(abs_addr, word);
        if (rc != 0) {
            FLASH_LOG_ERR("flash_area_write: write failed at 0x%08lX",
                          (unsigned long)abs_addr);
            fmc_clear_all_flags();
            fmc_lock();
            return rc;
        }

        src_bytes  += 8;
        abs_addr   += 8;
        remaining  -= 8;
    }

    /* --- Handle trailing bytes --- */
    if (remaining > 0) {
        uint64_t buf;

        /* Ensure D-Cache does not hold stale data for this word */
        SCB_InvalidateDCache_by_Addr((uint32_t *)abs_addr, 8);

        /* Read-modify-write the last partial word */
        buf = *(__IO uint64_t *)abs_addr;
        memcpy(&buf, src_bytes, remaining);

        rc = program_doubleword(abs_addr, buf);
        if (rc != 0) {
            FLASH_LOG_ERR("flash_area_write: tail write failed");
            fmc_clear_all_flags();
            fmc_lock();
            return rc;
        }
    }

    fmc_clear_all_flags();
    fmc_lock();

    return 0;
}

/* ======================================================================== */
/*  Public API: Flash erase                                                 */
/* ======================================================================== */

int flash_area_erase(const struct flash_area *fa,
                     uint32_t off, uint32_t len)
{
    uint32_t abs_addr;
    uint32_t remaining;
    fmc_state_enum fmc_state;

    if (fa == NULL) {
        return -1;
    }

    /* Bounds check (safe against uint32_t overflow) */
    if (len > fa->fa_size || off > fa->fa_size - len) {
        FLASH_LOG_ERR("flash_area_erase: out of bounds");
        return -1;
    }

    /* Alignment check: offset and length must be sector-aligned */
    if ((off % FLASH_SECTOR_SIZE) != 0) {
        FLASH_LOG_ERR("flash_area_erase: offset 0x%lX not sector-aligned",
                      (unsigned long)off);
        return -1;
    }
    if ((len % FLASH_SECTOR_SIZE) != 0) {
        FLASH_LOG_ERR("flash_area_erase: len %lu not sector-aligned",
                      (unsigned long)len);
        return -1;
    }

    abs_addr  = fa_to_abs_addr(fa, off);
    remaining = len;

    fmc_unlock();
    fmc_clear_all_flags();

    /* Erase all sectors within a single unlock/lock cycle */
    while (remaining > 0) {
        fmc_state = fmc_sector_erase(abs_addr);
        if (fmc_state != FMC_READY) {
            FLASH_LOG_ERR("flash_area_erase: failed at 0x%08lX, state=%d",
                          (unsigned long)abs_addr, (int)fmc_state);
            fmc_clear_all_flags();
            fmc_lock();
            return -1;
        }

        /* Invalidate D-Cache for this sector */
        SCB_InvalidateDCache_by_Addr((uint32_t *)abs_addr, FLASH_SECTOR_SIZE);

        /* Verify the sector was erased (check first word) */
        if (*(__IO uint32_t *)abs_addr != 0xFFFFFFFFU) {
            FLASH_LOG_ERR("flash_area_erase: verify failed at 0x%08lX",
                          (unsigned long)abs_addr);
            fmc_clear_all_flags();
            fmc_lock();
            return -1;
        }

        abs_addr  += FLASH_SECTOR_SIZE;
        remaining -= FLASH_SECTOR_SIZE;
    }

    fmc_clear_all_flags();
    fmc_lock();

    return 0;
}

/* ======================================================================== */
/*  Public API: Flash properties                                            */
/* ======================================================================== */

uint32_t flash_area_align(const struct flash_area *fa)
{
    (void)fa;
    return FLASH_WRITE_ALIGN;  /* 8 bytes for GD32H7xx */
}

uint8_t flash_area_erased_val(const struct flash_area *fa)
{
    (void)fa;
    return FLASH_ERASED_VAL;   /* 0xFF */
}

/* ======================================================================== */
/*  Public API: Sector enumeration                                          */
/* ======================================================================== */

int flash_area_get_sectors(int fa_id, uint32_t *count,
                           struct flash_sector *sectors)
{
    const struct flash_area *fa;
    uint32_t total_sectors;
    uint32_t i;

    if (fa_id < 0 || (uint32_t)fa_id >= FLASH_AREA_COUNT) {
        return -1;
    }

    fa = &s_flash_areas[fa_id];

    if (fa->fa_size == 0) {
        *count = 0;
        return 0;
    }

    total_sectors = fa->fa_size / FLASH_SECTOR_SIZE;

    /*
     * If the caller only wants the count (sectors == NULL), or
     * the provided buffer is too small, report the required count.
     */
    if (sectors == NULL) {
        *count = total_sectors;
        return 0;
    }

    if (*count < total_sectors) {
        FLASH_LOG_ERR("flash_area_get_sectors: buffer too small (%lu < %lu)",
                      (unsigned long)*count, (unsigned long)total_sectors);
        *count = total_sectors;
        return -1;
    }

    /* Fill the sector information */
    for (i = 0; i < total_sectors; i++) {
        sectors[i].fs_off  = i * FLASH_SECTOR_SIZE;
        sectors[i].fs_size = FLASH_SECTOR_SIZE;
    }

    *count = total_sectors;
    return 0;
}

/* ======================================================================== */
/*  Public API: Sector lookup                                               */
/* ======================================================================== */

int flash_area_sector_from_off(uint32_t off, struct flash_sector *sector)
{
    if (sector == NULL) {
        return -1;
    }

    /* off is an absolute flash offset from FLASH_BASE */
    if (off >= FLASH_TOTAL_SIZE) {
        return -1;
    }

    sector->fs_off  = (off / FLASH_SECTOR_SIZE) * FLASH_SECTOR_SIZE;
    sector->fs_size = FLASH_SECTOR_SIZE;
    return 0;
}

int flash_area_get_sector(const struct flash_area *fa, uint32_t off,
                          struct flash_sector *sector)
{
    if (fa == NULL || sector == NULL) {
        return -1;
    }

    if (off >= fa->fa_size) {
        return -1;
    }

    sector->fs_off  = (off / FLASH_SECTOR_SIZE) * FLASH_SECTOR_SIZE;
    sector->fs_size = FLASH_SECTOR_SIZE;
    return 0;
}

/* ======================================================================== */
/*  Public API: Slot / ID conversion                                        */
/* ======================================================================== */

int flash_area_id_from_image_slot(int slot)
{
    return flash_area_id_from_multi_image_slot(0, slot);
}

int flash_area_id_from_multi_image_slot(int image_index, int slot)
{
    switch (slot) {
    case 0:
        return FLASH_AREA_ID_IMAGE_PRIMARY + (image_index * 2);
    case 1:
        return FLASH_AREA_ID_IMAGE_SECONDARY + (image_index * 2);
    default:
        FLASH_LOG_ERR("Invalid slot: %d", slot);
        return -1;
    }
}

int flash_area_id_to_multi_image_slot(int image_index, int area_id)
{
    (void)image_index;

    if (area_id == FLASH_AREA_ID_IMAGE_PRIMARY) {
        return 0;
    }
    if (area_id == FLASH_AREA_ID_IMAGE_SECONDARY) {
        return 1;
    }

    return -1;
}

/* ======================================================================== */
/*  Public API: Device base address                                         */
/* ======================================================================== */

int flash_device_base(uint8_t fd_id, uintptr_t *ret)
{
    if (ret == NULL) {
        return -1;
    }

    switch (fd_id) {
    case FLASH_DEVICE_INTERNAL_FLASH:
        *ret = INTERNAL_FLASH_BASE;
        return 0;
    default:
        FLASH_LOG_ERR("Unknown flash device ID: %u", fd_id);
        return -1;
    }
}