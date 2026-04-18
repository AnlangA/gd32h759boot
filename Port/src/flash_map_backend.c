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
 *   - Bank 0: 0x0800_0000, up to 1 MB, sector size 128 KB
 *   - Bank 1: 0x0810_0000, up to 1 MB, sector size 128 KB
 *   - Minimum write granularity: 8 bytes (64-bit word)
 *   - Erase granularity: 128 KB per sector
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
 * Wait until the FMC is ready (no ongoing program / erase operation).
 *
 * This polls the FMC bank status registers. GD32H7xx has two banks;
 * we determine which bank based on the address and poll the correct one.
 *
 * @return 0 on success, -1 on timeout.
 */
static int fmc_wait_ready(uint32_t abs_addr)
{
    volatile uint32_t timeout = 0x00FFFFFFU;

    if (abs_addr < 0x08100000U) {
        /* Bank 0 */
        while ((FMC_STAT0 & FMC_STAT0_BUSY) && timeout) {
            timeout--;
        }
        if (0 == timeout) {
            FLASH_LOG_ERR("FMC bank0 wait timeout");
            return -1;
        }
        /* Check for errors */
        if (FMC_STAT0 & (FMC_STAT0_PGERR | FMC_STAT0_WPERR)) {
            FMC_STAT0 = FMC_STAT0_PGERR | FMC_STAT0_WPERR;
            FLASH_LOG_ERR("FMC bank0 error, STAT0=0x%08lX", FMC_STAT0);
            return -1;
        }
    } else {
        /* Bank 1 */
        while ((FMC_STAT1 & FMC_STAT1_BUSY) && timeout) {
            timeout--;
        }
        if (0 == timeout) {
            FLASH_LOG_ERR("FMC bank1 wait timeout");
            return -1;
        }
        if (FMC_STAT1 & (FMC_STAT1_PGERR | FMC_STAT1_WPERR)) {
            FMC_STAT1 = FMC_STAT1_PGERR | FMC_STAT1_WPERR;
            FLASH_LOG_ERR("FMC bank1 error, STAT1=0x%08lX", FMC_STAT1);
            return -1;
        }
    }
    return 0;
}

/**
 * Unlock the FMC for program/erase operations.
 *
 * GD32H7xx requires a specific unlock sequence (KEY0 / KEY1 writes).
 * Both banks may need to be unlocked depending on the target address.
 *
 * @param abs_addr  Absolute flash address being accessed.
 * @return 0 on success, -1 on failure.
 */
static int fmc_unlock(uint32_t abs_addr)
{
    if (abs_addr < 0x08100000U) {
        /* Unlock Bank 0 */
        if (FMC_STAT0 & FMC_STAT0_BUSY) {
            FLASH_LOG_ERR("Cannot unlock bank0: FMC busy");
            return -1;
        }
        FMC_UNLK0 = FMC_UNLOCK_KEY0;
        FMC_UNLK0 = FMC_UNLOCK_KEY1;
    } else {
        /* Unlock Bank 1 */
        if (FMC_STAT1 & FMC_STAT1_BUSY) {
            FLASH_LOG_ERR("Cannot unlock bank1: FMC busy");
            return -1;
        }
        FMC_UNLK1 = FMC_UNLOCK_KEY0;
        FMC_UNLK1 = FMC_UNLOCK_KEY1;
    }
    return 0;
}

/**
 * Lock the FMC after program/erase operations.
 *
 * @param abs_addr  Absolute flash address being accessed.
 */
static void fmc_lock(uint32_t abs_addr)
{
    if (abs_addr < 0x08100000U) {
        FMC_UNLK0 = FMC_LOCK_KEY;
    } else {
        FMC_UNLK1 = FMC_LOCK_KEY;
    }
}

/**
 * Erase a single 128 KB flash sector.
 *
 * GD32H7xx FMC supports sector erase. The sector number is calculated
 * from the absolute address.
 *
 * @param abs_addr  Absolute address within the sector to erase.
 *                  Must be sector-aligned.
 * @return 0 on success, non-zero on failure.
 */
static int erase_sector(uint32_t abs_addr)
{
    int rc;

    rc = fmc_unlock(abs_addr);
    if (rc != 0) {
        return rc;
    }

    if (abs_addr < 0x08100000U) {
        /* Bank 0: sector number = (addr - 0x08000000) / 128K */
        uint32_t sn = (abs_addr - 0x08000000U) / FLASH_SECTOR_SIZE;

        /* Set the sector number to erase */
        FMC_CTL0 &= ~FMC_CTL0_SN_MASK;
        FMC_CTL0 |= (sn << FMC_CTL0_SN_POS) & FMC_CTL0_SN_MASK;

        /* Start sector erase */
        FMC_CTL0 |= FMC_CTL0_SER;
        FMC_CTL0 |= FMC_CTL0_START;

        /* Wait for completion */
        rc = fmc_wait_ready(abs_addr);

        /* Clear start bit and sector erase mode */
        FMC_CTL0 &= ~FMC_CTL0_START;
        FMC_CTL0 &= ~FMC_CTL0_SER;
    } else {
        /* Bank 1: sector number = (addr - 0x08100000) / 128K */
        uint32_t sn = (abs_addr - 0x08100000U) / FLASH_SECTOR_SIZE;

        FMC_CTL1 &= ~FMC_CTL1_SN_MASK;
        FMC_CTL1 |= (sn << FMC_CTL1_SN_POS) & FMC_CTL1_SN_MASK;

        FMC_CTL1 |= FMC_CTL1_SER;
        FMC_CTL1 |= FMC_CTL1_START;

        rc = fmc_wait_ready(abs_addr);

        FMC_CTL1 &= ~FMC_CTL1_START;
        FMC_CTL1 &= ~FMC_CTL1_SER;
    }

    fmc_lock(abs_addr);
    return rc;
}

/**
 * Program 8 bytes (64-bit) to flash at the given absolute address.
 *
 * GD32H7xx FMC requires 64-bit aligned writes for internal flash.
 *
 * @param abs_addr  Must be 8-byte aligned.
 * @param data      Pointer to at least 8 bytes of data.
 * @return 0 on success, non-zero on failure.
 */
static int program_64bit(uint32_t abs_addr, const uint64_t *data)
{
    int rc;

    rc = fmc_unlock(abs_addr);
    if (rc != 0) {
        return rc;
    }

    if (abs_addr < 0x08100000U) {
        FMC_CTL0 |= FMC_CTL0_PG;
    } else {
        FMC_CTL1 |= FMC_CTL1_PG;
    }

    /* Write the 64-bit word using volatile pointer to prevent
     * compiler reordering or optimisation. */
    *(__IO uint64_t *)abs_addr = *data;

    /* Wait for completion */
    rc = fmc_wait_ready(abs_addr);

    if (abs_addr < 0x08100000U) {
        FMC_CTL0 &= ~FMC_CTL0_PG;
    } else {
        FMC_CTL1 &= ~FMC_CTL1_PG;
    }

    fmc_lock(abs_addr);

    /* Verify the written data */
    if (*(__IO uint64_t *)abs_addr != *data) {
        FLASH_LOG_ERR("Flash verify failed at 0x%08lX", abs_addr);
        return -1;
    }

    return rc;
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

    /* Bounds check */
    if ((off + len) > fa->fa_size) {
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

    /* Bounds check */
    if ((off + len) > fa->fa_size) {
        FLASH_LOG_ERR("flash_area_write: out of bounds");
        return -1;
    }

    abs_addr = fa_to_abs_addr(fa, off);
    remaining = len;

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

        /* Read the existing 64-bit word */
        buf = *(__IO uint64_t *)aligned_start;

        /* Overlay the new bytes */
        memcpy((uint8_t *)&buf + (abs_addr & 0x7U), src_bytes, head_bytes);

        /* Write it back */
        rc = program_64bit(aligned_start, &buf);
        if (rc != 0) {
            FLASH_LOG_ERR("flash_area_write: head write failed");
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

        rc = program_64bit(abs_addr, &word);
        if (rc != 0) {
            FLASH_LOG_ERR("flash_area_write: write failed at 0x%08lX",
                          (unsigned long)abs_addr);
            return rc;
        }

        src_bytes  += 8;
        abs_addr   += 8;
        remaining  -= 8;
    }

    /* --- Handle trailing bytes --- */
    if (remaining > 0) {
        uint64_t buf;

        /* Read-modify-write the last partial word */
        buf = *(__IO uint64_t *)abs_addr;
        memcpy(&buf, src_bytes, remaining);

        rc = program_64bit(abs_addr, &buf);
        if (rc != 0) {
            FLASH_LOG_ERR("flash_area_write: tail write failed");
            return rc;
        }
    }

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
    int rc;

    if (fa == NULL) {
        return -1;
    }

    /* Bounds check */
    if ((off + len) > fa->fa_size) {
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

    /* Erase one sector at a time */
    while (remaining > 0) {
        rc = erase_sector(abs_addr);
        if (rc != 0) {
            FLASH_LOG_ERR("flash_area_erase: failed at 0x%08lX",
                          (unsigned long)abs_addr);
            return rc;
        }

        /* Verify the sector was erased (check first word) */
        if (*(__IO uint32_t *)abs_addr != 0xFFFFFFFFU) {
            FLASH_LOG_ERR("flash_area_erase: verify failed at 0x%08lX",
                          (unsigned long)abs_addr);
            return -1;
        }

        abs_addr  += FLASH_SECTOR_SIZE;
        remaining -= FLASH_SECTOR_SIZE;
    }

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