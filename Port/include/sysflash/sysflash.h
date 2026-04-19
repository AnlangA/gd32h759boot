/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2024 GD32H759 Bootloader Project
 *
 * System flash layout for GD32H759.
 *
 * Defines the physical flash partition table used by MCUBoot.
 * All offsets are relative to the start of internal flash (0x0800_0000).
 *
 * GD32H759 Flash total: up to 3840 KB (0x0800_0000 - 0x083C_0000)
 *   - Sector size (erase granularity): 4 KB (0x1000)
 *   - Total sectors: 960 (0x000 ~ 0x3BF)
 *   - Minimum write granularity: 8 bytes (64-bit double word)
 *
 * Partition layout (overwrite-only, single image):
 *
 *   Offset       Size        Usage
 *   ----------   --------    --------------------------------
 *   0x0000_0000  128 KB      Bootloader (MCUBoot itself)
 *   0x0002_0000  640 KB      Primary slot   (Slot 0)
 *   0x000C_0000  640 KB      Secondary slot (Slot 1)
 *   0x0016_0000  128 KB      Scratch area   (swap modes only)
 *   0x0018_0000  ...         Application / reserved
 *
 * NOTE: Adjust these macros to match your actual linker script and
 *       application requirements. The scratch area is only needed for
 *       swap-based upgrade modes (MCUBOOT_SWAP_USING_SCRATCH,
 *       MCUBOOT_SWAP_USING_MOVE). For overwrite-only mode it can be
 *       omitted to free up flash space.
 */

#ifndef __SYSFLASH_H__
#define __SYSFLASH_H__

#include <mcuboot_config/mcuboot_config.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================== */
/*  Flash base and total size                                               */
/* ======================================================================== */
#define FLASH_BASE_ADDR               0x08000000U
#define FLASH_TOTAL_SIZE              0x003C0000U   /* 3840 KB (GD32H759IQ)  */
#define FLASH_SECTOR_SIZE             0x00001000U   /* 4 KB (hardware erase granularity) */

/* ======================================================================== */
/*  Bootloader partition                                                    */
/* ======================================================================== */
#define FLASH_AREA_BOOTLOADER_OFFSET  0x00000000U
#define FLASH_AREA_BOOTLOADER_SIZE    0x00020000U   /* 128 KB (32 x 4 KB sectors) */

/* ======================================================================== */
/*  Primary slot (Slot 0)                                                   */
/* ======================================================================== */
/*
 * The application's vector table must be located at the beginning of
 * this area. After MCUBoot finishes, it jumps to
 *   FLASH_BASE_ADDR + FLASH_AREA_IMAGE_PRIMARY_OFFSET
 * which matches the APP_ADDRESS defined in main.h (0x0802_0000).
 */
#define FLASH_AREA_IMAGE_PRIMARY_OFFSET    0x00020000U   /* 128 KB from start */
#define FLASH_AREA_IMAGE_PRIMARY_SIZE      0x000A0000U   /* 640 KB (160 x 4 KB sectors) */

/* ======================================================================== */
/*  Secondary slot (Slot 1)                                                 */
/* ======================================================================== */
/*
 * Holds the candidate firmware image for upgrade.
 * Must be the same size as the primary slot.
 */
#define FLASH_AREA_IMAGE_SECONDARY_OFFSET  0x000C0000U   /* 768 KB from start */
#define FLASH_AREA_IMAGE_SECONDARY_SIZE    0x000A0000U   /* 640 KB (160 x 4 KB sectors) */

/* ======================================================================== */
/*  Scratch area                                                            */
/* ======================================================================== */
/*
 * Required only for swap-based upgrade modes.
 * Can be set to 0 for overwrite-only mode.
 */
#if defined(MCUBOOT_SWAP_USING_SCRATCH) || \
    defined(MCUBOOT_SWAP_USING_MOVE)    || \
    defined(MCUBOOT_SWAP_USING_OFFSET)
#define FLASH_AREA_IMAGE_SCRATCH_OFFSET   0x00160000U   /* 1408 KB from start */
#define FLASH_AREA_IMAGE_SCRATCH_SIZE     0x00020000U   /* 128 KB (32 x 4 KB sectors) */
#else
#define FLASH_AREA_IMAGE_SCRATCH_OFFSET   0x00000000U
#define FLASH_AREA_IMAGE_SCRATCH_SIZE     0x00000000U
#endif

/* ======================================================================== */
/*  Slot-to-ID mapping (used by MCUBoot internals)                          */
/* ======================================================================== */
/*
 * These macros convert an image index + slot number into a flash area ID.
 * For single-image setup (MCUBOOT_IMAGE_NUMBER == 1), the image index
 * is always 0.
 *
 * FLASH_AREA_IMAGE_PRIMARY(x)   -> area ID for primary slot of image x
 * FLASH_AREA_IMAGE_SECONDARY(x) -> area ID for secondary slot of image x
 */
#define FLASH_AREA_IMAGE_PRIMARY(x)                                     \
    (((x) == 0) ? 1 : 255)    /* ID 1 = primary slot                  */

#define FLASH_AREA_IMAGE_SECONDARY(x)                                   \
    (((x) == 0) ? 2 : 255)    /* ID 2 = secondary slot                */

/* ======================================================================== */
/*  Convenience: absolute addresses                                         */
/* ======================================================================== */
#define BOOTLOADER_ADDR   (FLASH_BASE_ADDR + FLASH_AREA_BOOTLOADER_OFFSET)
#define PRIMARY_SLOT_ADDR (FLASH_BASE_ADDR + FLASH_AREA_IMAGE_PRIMARY_OFFSET)
#define SECONDARY_SLOT_ADDR (FLASH_BASE_ADDR + FLASH_AREA_IMAGE_SECONDARY_OFFSET)
#define SCRATCH_ADDR      (FLASH_BASE_ADDR + FLASH_AREA_IMAGE_SCRATCH_OFFSET)

/* ======================================================================== */
/*  Flash area ID definitions (must match flash_map_backend.c table)        */
/* ======================================================================== */
#define FLASH_AREA_ID_BOOTLOADER       0
#define FLASH_AREA_ID_IMAGE_PRIMARY    1
#define FLASH_AREA_ID_IMAGE_SECONDARY  2
#define FLASH_AREA_ID_IMAGE_SCRATCH    3

#ifdef __cplusplus
}
#endif

#endif /* __SYSFLASH_H__ */