/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2024 GD32H759 Bootloader Project
 *
 * MCUBoot integration glue header for GD32H759.
 *
 * This is the single header that application code (e.g. App/main.c)
 * should include to gain access to MCUBoot functionality.  It pulls
 * in every header the porting layer provides and sets up the correct
 * include search order so that MCUBoot's bootutil library sees the
 * GD32H7xx-specific implementations instead of the Zephyr defaults.
 *
 * Include path ordering (set in your build system / CMakeLists.txt):
 *
 *   -I Port/include                         ← our headers below
 *   -I mcuboot/boot/bootutil/include        ← bootutil public API
 *   -I mcuboot/boot/zephyr/include          ← Zephyr-compatible stubs
 *                                            (only if needed)
 *   -I Firmware/CMSIS/GD/GD32H7xx/Include  ← gd32h7xx.h, core_cm7.h
 *   -I Firmware/GD32H7xx_standard_peripheral/Include  ← gd32h7xx_fmc.h etc.
 *
 * The headers below are searched in the order listed; because
 * Port/include appears before mcuboot/boot/zephyr/include, the
 * GD32H7xx-specific mcuboot_config.h, flash_map_backend.h, etc.
 * take precedence over the Zephyr reference implementations.
 */

#ifndef __MCUBOOT_GLUE_H__
#define __MCUBOOT_GLUE_H__

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================== */
/*  1. MCUBoot configuration                                                */
/* ======================================================================== */
/*
 * mcuboot_config.h defines the feature macros that the bootutil library
 * inspects at compile time:
 *
 *   MCUBOOT_SIGN_EC256        – signature algorithm
 *   MCUBOOT_SHA256            – hash algorithm
 *   MCUBOOT_USE_MBED_TLS      – crypto backend
 *   MCUBOOT_IMAGE_NUMBER      – number of firmware images (1 = single)
 *   MCUBOOT_OVERWRITE_ONLY    – upgrade mode
 *   MCUBOOT_VALIDATE_PRIMARY_SLOT
 *   MCUBOOT_HAVE_LOGGING
 *   MCUBOOT_MAX_IMG_SECTORS
 *   MCUBOOT_BOOT_MAX_ALIGN
 *   ... etc.
 */
#include <mcuboot_config/mcuboot_config.h>

/* ======================================================================== */
/*  2. MCUBoot logging                                                      */
/* ======================================================================== */
/*
 * mcuboot_logging.h maps MCUBOOT_LOG_ERR / _WRN / _INF / _DBG / _SIM
 * to the platform's printf-based console output (USART0 via
 * console_usart.c).
 *
 * The main.c function check_reset_source() uses MCUBOOT_LOG_SIM()
 * which is defined here.
 */
#include <mcuboot_config/mcuboot_logging.h>

/* ======================================================================== */
/*  3. Flash map backend                                                    */
/* ======================================================================== */
/*
 * flash_map_backend.h provides the struct flash_area / flash_sector
 * definitions and the full flash I/O API that bootutil calls:
 *
 *   flash_area_open / close / read / write / erase
 *   flash_area_align / erased_val / get_sectors
 *   flash_device_base
 *
 * The implementation lives in Port/src/flash_map_backend.c and
 * uses GD32H7xx FMC peripheral calls.
 */
#include <flash_map_backend/flash_map_backend.h>

/* ======================================================================== */
/*  4. System flash layout                                                  */
/* ======================================================================== */
/*
 * sysflash.h defines the physical partition table:
 *
 *   FLASH_AREA_BOOTLOADER_OFFSET / SIZE
 *   FLASH_AREA_IMAGE_PRIMARY_OFFSET / SIZE    (matches APP_ADDRESS)
 *   FLASH_AREA_IMAGE_SECONDARY_OFFSET / SIZE
 *   FLASH_AREA_IMAGE_SCRATCH_OFFSET / SIZE
 *
 * It also provides the FLASH_AREA_IMAGE_PRIMARY(x) /
 * FLASH_AREA_IMAGE_SECONDARY(x) macros used by bootutil internals.
 */
#include <sysflash/sysflash.h>

/* ======================================================================== */
/*  5. Memory allocation (bare-metal)                                       */
/* ======================================================================== */
/*
 * os/os_malloc.h exists solely because mcuboot/boot/bootutil/src/loader.c
 * conditionally includes it:
 *
 *   #if !defined(MCUBOOT_DIRECT_XIP) && !defined(MCUBOOT_RAM_LOAD)
 *   #include <os/os_malloc.h>
 *   #endif
 *
 * In overwrite-only and swap modes this file MUST be resolvable.
 * It simply redirects os_malloc/os_free/os_realloc to the standard C
 * library — no RTOS, no custom heap, no extra indirection.
 *
 * There is nothing else to include here; the bare-metal bootloader
 * uses the C runtime heap directly.
 */

/* ======================================================================== */
/*  6. MCUBoot bootutil public API                                          */
/* ======================================================================== */
/*
 * These are the primary headers that application code will use:
 *
 *   bootutil.h      – boot_go(), boot_rsp, boot_state_init()
 *   image.h         – image_header, image_version, TLV definitions
 *   bootutil_public.h – boot_swap_type(), boot_set_pending(),
 *                       boot_set_confirmed(), swap state queries
 *
 * After calling boot_go(), the returned boot_rsp tells the application
 * where to jump:  rsp->br_flash_dev_id + rsp->br_image_off give the
 * absolute flash address of the selected image header.
 */
#include <bootutil/bootutil.h>
#include <bootutil/image.h>
#include <bootutil/bootutil_public.h>

/* ======================================================================== */
/*  7. Convenience macros and inline helpers                                */
/* ======================================================================== */

/**
 * Compute the absolute address of the image to execute from a boot_rsp.
 *
 * After boot_go() returns successfully, pass the boot_rsp to this
 * macro to get the memory-mapped address of the image header.
 * The application's reset vector is at offset +4 from that header.
 *
 * Example usage in main.c:
 *
 *   struct boot_rsp rsp;
 *   fih_ret rc = boot_go(&rsp);
 *   if (rc == FIH_SUCCESS) {
 *       uint32_t img_addr = MCUBOOT_IMAGE_ADDR(&rsp);
 *       uint32_t sp   = *(volatile uint32_t *)(img_addr);
 *       uint32_t pc   = *(volatile uint32_t *)(img_addr + 4);
 *       // ... validate and jump
 *   }
 */
#define MCUBOOT_IMAGE_ADDR(rsp_p)                                           \
    ((uint32_t)(INTERNAL_FLASH_BASE_FOR_GLUE) +                             \
     (rsp_p)->br_image_off)

/*
 * The internal flash base address, exposed here for the convenience
 * macro above.  Must match the value used in flash_map_backend.c.
 */
#define INTERNAL_FLASH_BASE_FOR_GLUE   0x08000000U

/**
 * Validate that an image address looks plausible ( Thumb bit set,
 * stack pointer in SRAM range ).
 *
 * This is a lightweight check; MCUBoot has already verified the
 * cryptographic signature at this point.
 */
static inline int mcuboot_validate_jump_addr(uint32_t img_addr)
{
    uint32_t sp = *(__IO uint32_t *)(img_addr);
    uint32_t pc = *(__IO uint32_t *)(img_addr + 4);

    /* SRAM_BASE for GD32H7xx is 0x24000000, AXI SRAM 512 KB */
    if (sp < 0x24000000U || sp > 0x240E0000U) {
        return -1;  /* Stack pointer not in SRAM */
    }
    if (pc < 0x08000000U || pc > 0x083C0000U) {
        return -1;  /* PC not in flash */
    }
    if (!(pc & 1U)) {
        return -1;  /* Not a Thumb address */
    }
    return 0;
}

/* ======================================================================== */
/*  8. Flash area quick-open helpers                                        */
/* ======================================================================== */

/**
 * Quick-open the primary image slot.
 *
 * @param area_out  Receives pointer to the flash_area descriptor.
 * @return 0 on success.
 */
static inline int mcuboot_open_primary(const struct flash_area **area_out)
{
    return flash_area_open(FLASH_AREA_IMAGE_PRIMARY(0), area_out);
}

/**
 * Quick-open the secondary image slot.
 *
 * @param area_out  Receives pointer to the flash_area descriptor.
 * @return 0 on success.
 */
static inline int mcuboot_open_secondary(const struct flash_area **area_out)
{
    return flash_area_open(FLASH_AREA_IMAGE_SECONDARY(0), area_out);
}

/* ======================================================================== */
/*  9. Version string / build info                                          */
/* ======================================================================== */

#define MCUBOOT_PORT_VERSION     "0.1.0"
#define MCUBOOT_PORT_TARGET      "GD32H759IMK6"
#define MCUBOOT_PORT_UPGRADE_MODE "overwrite-only"

#ifdef __cplusplus
}
#endif

#endif /* __MCUBOOT_GLUE_H__ */