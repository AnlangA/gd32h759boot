/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2024 GD32H759 Bootloader Project
 *
 * Flash map backend for GD32H7xx internal flash.
 *
 * This header defines the flash area abstraction layer that MCUBoot's
 * bootutil library requires. It provides a uniform interface for reading,
 * writing, erasing, and querying flash geometry independent of the
 * underlying HAL.
 *
 * Reference: GD32H7xx User Manual - FMC (Flash Memory Controller) chapter
 *   - Bank 0: 0x0800_0000 - 0x080F_FFFF  (up to 1 MB, sector size 128 KB)
 *   - Bank 1: 0x0810_0000 - 0x081F_FFFF  (up to 1 MB, sector size 128 KB)
 *   - Total:  up to 2 MB internal Flash
 *   - Minimum write granularity: 8 bytes (64-bit)
 *   - Erase granularity: 128 KB per sector (8 KB sub-sectors on some parts)
 */

#ifndef __FLASH_MAP_BACKEND_H__
#define __FLASH_MAP_BACKEND_H__

#include <inttypes.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================== */
/*  Flash device identifiers                                                */
/* ======================================================================== */
#define FLASH_DEVICE_INTERNAL_FLASH     0

/* ======================================================================== */
/*  Flash area IDs                                                          */
/* ======================================================================== */
/*
 * These IDs are used by MCUBoot to identify different firmware slots.
 * The actual address mapping is defined in flash_map_backend.c.
 *
 * FLASH_AREA_IMAGE_PRIMARY(x) / FLASH_AREA_IMAGE_SECONDARY(x) function-like
 * macros are provided by sysflash/sysflash.h (included below).  We do NOT
 * redefine the plain names FLASH_AREA_IMAGE_PRIMARY / FLASH_AREA_IMAGE_SECONDARY
 * as object-like macros here, because that would conflict with the function-like
 * macros from sysflash.h and cause "called object type 'int' is not a function"
 * errors.
 *
 * Use FLASH_AREA_IMAGE_PRIMARY(0) to get the integer ID (evaluates to 1).
 */

/* ======================================================================== */
/*  sysflash.h – provides FLASH_AREA_IMAGE_PRIMARY(x) etc.                  */
/* ======================================================================== */
/*
 * Include sysflash.h FIRST so the function-like macros are defined before
 * anything else tries to use them.  sysflash.h also provides:
 *   FLASH_AREA_ID_BOOTLOADER / _IMAGE_PRIMARY / _IMAGE_SECONDARY / _IMAGE_SCRATCH
 * which are plain integer constants used in flash_map_backend.c.
 */
#include <sysflash/sysflash.h>

/* ======================================================================== */
/*  Flash area ID constants (legacy names)                                  */
/* ======================================================================== */
#define FLASH_AREA_BOOTLOADER          FLASH_AREA_ID_BOOTLOADER
#define FLASH_AREA_IMAGE_SCRATCH       FLASH_AREA_ID_IMAGE_SCRATCH

/* ======================================================================== */
/*  Convenience macros for single-image setup                               */
/* ======================================================================== */
#define FLASH_AREA_IMAGE_PRIMARY_SLOT     0
#define FLASH_AREA_IMAGE_SECONDARY_SLOT   1

/* ======================================================================== */
/*  Flash geometry constants (GD32H7xx)                                     */
/* ======================================================================== */
#ifndef FLASH_SECTOR_SIZE
#define FLASH_SECTOR_SIZE              (128U * 1024U)  /* 128 KB           */
#endif
#define FLASH_WRITE_ALIGN              8U               /* 8-byte writes   */
#define FLASH_ERASED_VAL               0xFFU

/* ======================================================================== */
/*  struct flash_area                                                       */
/* ======================================================================== */
/**
 * Represents a contiguous region of flash memory.
 *
 * fa_id        - Logical ID (one of FLASH_AREA_IMAGE_PRIMARY, etc.)
 * fa_device_id - Physical flash device (FLASH_DEVICE_INTERNAL_FLASH)
 * fa_off       - Byte offset from the start of the flash device
 * fa_size      - Size of this area in bytes
 */
struct flash_area {
    uint8_t  fa_id;
    uint8_t  fa_device_id;
    uint16_t pad16;
    uint32_t fa_off;
    uint32_t fa_size;
};

/* ======================================================================== */
/*  struct flash_sector                                                     */
/* ======================================================================== */
/**
 * Describes a single erase sector within a flash_area.
 *
 * fs_off  - Byte offset of this sector from the start of the flash area
 * fs_size - Size of this sector in bytes
 */
struct flash_sector {
    uint32_t fs_off;
    uint32_t fs_size;
};

/* ======================================================================== */
/*  Inline accessors                                                        */
/* ======================================================================== */
static inline uint8_t flash_area_get_id(const struct flash_area *fa)
{
    return fa->fa_id;
}

static inline uint8_t flash_area_get_device_id(const struct flash_area *fa)
{
    return fa->fa_device_id;
}

static inline uint32_t flash_area_get_off(const struct flash_area *fa)
{
    return fa->fa_off;
}

static inline uint32_t flash_area_get_size(const struct flash_area *fa)
{
    return fa->fa_size;
}

static inline uint32_t flash_sector_get_off(const struct flash_sector *fs)
{
    return fs->fs_off;
}

static inline uint32_t flash_sector_get_size(const struct flash_sector *fs)
{
    return fs->fs_size;
}

/* ======================================================================== */
/*  Flash area lifecycle                                                    */
/* ======================================================================== */

/**
 * Open a flash area by ID.
 *
 * @param id        One of FLASH_AREA_IMAGE_PRIMARY, etc.
 * @param area_outp Receives a pointer to a static flash_area descriptor.
 *
 * @return 0 on success, non-zero on failure.
 */
int flash_area_open(uint8_t id, const struct flash_area **area_outp);

/**
 * Close a previously opened flash area (no-op for internal flash).
 */
void flash_area_close(const struct flash_area *fa);

/* ======================================================================== */
/*  Flash I/O                                                               */
/* ======================================================================== */

/**
 * Read from a flash area.
 *
 * @param fa   Flash area to read from.
 * @param off  Byte offset within the area.
 * @param dst  Destination buffer.
 * @param len  Number of bytes to read.
 *
 * @return 0 on success, non-zero on failure.
 */
int flash_area_read(const struct flash_area *fa, uint32_t off,
                    void *dst, uint32_t len);

/**
 * Write to a flash area.
 *
 * The caller must ensure the target region has been erased beforehand.
 * Writes must be aligned to FLASH_WRITE_ALIGN bytes.
 *
 * @param fa   Flash area to write to.
 * @param off  Byte offset within the area.
 * @param src  Source buffer.
 * @param len  Number of bytes to write.
 *
 * @return 0 on success, non-zero on failure.
 */
int flash_area_write(const struct flash_area *fa, uint32_t off,
                     const void *src, uint32_t len);

/**
 * Erase a region of a flash area.
 *
 * The offset and length must be aligned to the flash sector size.
 *
 * @param fa   Flash area to erase.
 * @param off  Byte offset within the area.
 * @param len  Number of bytes to erase.
 *
 * @return 0 on success, non-zero on failure.
 */
int flash_area_erase(const struct flash_area *fa,
                     uint32_t off, uint32_t len);

/* ======================================================================== */
/*  Flash properties                                                        */
/* ======================================================================== */

/**
 * Returns the minimum write alignment (in bytes) for the given flash area.
 */
uint32_t flash_area_align(const struct flash_area *fa);

/**
 * Returns the value of an erased flash byte (0xFF for GD32H7xx).
 */
uint8_t flash_area_erased_val(const struct flash_area *fa);

/**
 * Fill the sectors array with information about each erase sector
 * within the given flash area.
 *
 * @param fa_id   Flash area ID.
 * @param count   [in/out] Input: max entries; Output: actual count.
 * @param sectors Array of flash_sector to fill.
 *
 * @return 0 on success, non-zero on failure.
 */
int flash_area_get_sectors(int fa_id, uint32_t *count,
                           struct flash_sector *sectors);

/* ======================================================================== */
/*  Sector lookup                                                           */
/* ======================================================================== */

/**
 * Find the flash sector that contains the given absolute offset.
 *
 * @param off     Absolute flash offset.
 * @param sector  Output sector information.
 *
 * @return 0 on success, non-zero on failure.
 */
int flash_area_sector_from_off(uint32_t off, struct flash_sector *sector);

/**
 * Find the flash sector within a given area that contains the
 * given area-relative offset.
 *
 * @param fa      Flash area.
 * @param off     Offset within the flash area.
 * @param sector  Output sector information.
 *
 * @return 0 on success, non-zero on failure.
 */
int flash_area_get_sector(const struct flash_area *fa, uint32_t off,
                          struct flash_sector *sector);

/* ======================================================================== */
/*  Slot / ID conversion                                                    */
/* ======================================================================== */

/**
 * Convert a slot index (0=primary, 1=secondary) to a flash area ID
 * for single-image setups.
 */
int flash_area_id_from_image_slot(int slot);

/**
 * Convert a slot index to a flash area ID for multi-image setups.
 *
 * @param image_index  Image pair index (0-based).
 * @param slot         Slot index (0=primary, 1=secondary).
 *
 * @return Flash area ID, or negative on error.
 */
int flash_area_id_from_multi_image_slot(int image_index, int slot);

/**
 * Convert a flash area ID back to a slot index.
 *
 * @param image_index  Image pair index (0-based).
 * @param area_id      Flash area ID.
 *
 * @return Slot index (0 or 1), or -1 if not an image slot.
 */
int flash_area_id_to_multi_image_slot(int image_index, int area_id);

/**
 * Get the base address of a flash device in the memory map.
 *
 * @param fd_id  Flash device ID (e.g. FLASH_DEVICE_INTERNAL_FLASH).
 * @param ret    Output base address.
 *
 * @return 0 on success, non-zero on failure.
 */
int flash_device_base(uint8_t fd_id, uintptr_t *ret);

#ifdef __cplusplus
}
#endif

#endif /* __FLASH_MAP_BACKEND_H__ */