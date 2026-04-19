/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2024 GD32H759 Bootloader Project
 *
 * MCUBoot configuration for GD32H759 platform.
 * This file maps platform-specific settings to MCUBoot's internal macros.
 */

#ifndef __MCUBOOT_CONFIG_H__
#define __MCUBOOT_CONFIG_H__

/* ======================================================================== */
/*  Signature verification                                                  */
/* ======================================================================== */
/*
 * Choose one of: MCUBOOT_SIGN_RSA, MCUBOOT_SIGN_EC256, MCUBOOT_SIGN_ED25519
 * ECDSA-P256 is a good balance for Cortex-M7 with HW crypto (HAU).
 */
#define MCUBOOT_SIGN_EC256
#undef  MCUBOOT_SIGN_RSA
#undef  MCUBOOT_SIGN_ED25519

/* ======================================================================== */
/*  Hash algorithm                                                          */
/* ======================================================================== */
#define MCUBOOT_SHA256

/* ======================================================================== */
/*  Crypto backend                                                          */
/* ======================================================================== */
/*
 * Use mbed TLS as the crypto library.
 * GD32H7xx has HAU (Hash Acceleration Unit) which can be used via
 * mbed TLS_ALT or custom HAL; start with standard mbed TLS.
 */
#define MCUBOOT_USE_MBED_TLS

/* ======================================================================== */
/*  Image number (single image: primary + secondary)                        */
/* ======================================================================== */
#define MCUBOOT_IMAGE_NUMBER    1

/* ======================================================================== */
/*  Upgrade mode                                                            */
/* ======================================================================== */
/*
 * MCUBOOT_OVERWRITE_ONLY         - Overwrite secondary onto primary (no revert)
 * MCUBOOT_SWAP_USING_SCRATCH     - Swap using scratch area (allows revert)
 * MCUBOOT_SWAP_USING_MOVE        - Swap using move (allows revert, no scratch)
 * MCUBOOT_DIRECT_XIP             - Direct XIP, no copy
 *
 * Overwrite-only is simplest and most suitable for initial bring-up.
 * Switch to SWAP mode when revert support is needed.
 */
#define MCUBOOT_OVERWRITE_ONLY
/* #define MCUBOOT_OVERWRITE_ONLY_FAST */

/* ======================================================================== */
/*  Validate primary slot on every boot                                     */
/* ======================================================================== */
#define MCUBOOT_VALIDATE_PRIMARY_SLOT

/*
 * Validate only once after boot to save time, then cache result.
 * Requires non-volatile storage for the flag.
 */
/* #define MCUBOOT_VALIDATE_PRIMARY_SLOT_ONCE */

/* ======================================================================== */
/*  Downgrade prevention                                                    */
/* ======================================================================== */
/* #define MCUBOOT_DOWNGRADE_PREVENTION */

/* ======================================================================== */
/*  Logging                                                                 */
/* ======================================================================== */
#define MCUBOOT_HAVE_LOGGING    1

/* ======================================================================== */
/*  Flash alignment                                                         */
/* ======================================================================== */
/*
 * GD32H7xx FMC minimum write granularity is 8 bytes for bank0/bank1.
 * This must be a power of 2 between 8 and 32 for swap-based modes.
 */
#define MCUBOOT_BOOT_MAX_ALIGN  8

/* ======================================================================== */
/*  Max image sectors                                                       */
/* ======================================================================== */
/*
 * Maximum number of flash sectors per image slot.
 * GD32H759: 640 KB slot / 4 KB sector = 160 sectors; set to 192 for headroom.
 */
#define MCUBOOT_MAX_IMG_SECTORS 192

/* ======================================================================== */
/*  Hardware key (optional)                                                 */
/* ======================================================================== */
/* #define MCUBOOT_HW_KEY */

/* ======================================================================== */
/*  Encrypted images (optional)                                             */
/* ======================================================================== */
/* #define MCUBOOT_ENC_IMAGES */

/* ======================================================================== */
/*  RAM load mode (optional)                                                */
/* ======================================================================== */
/* #define MCUBOOT_RAM_LOAD */

/* ======================================================================== */
/*  Measured boot (optional)                                                */
/* ======================================================================== */
/* #define MCUBOOT_MEASURED_BOOT */

/* ======================================================================== */
/*  Data sharing with application (optional)                                */
/* ======================================================================== */
/* #define MCUBOOT_DATA_SHARING */

/* ======================================================================== */
/*  FIH (Fault Injection Hardening) profile                                 */
/* ======================================================================== */
#define MCUBOOT_FIH_PROFILE_OFF
/* #define MCUBOOT_FIH_PROFILE_LOW    */
/* #define MCUBOOT_FIH_PROFILE_MEDIUM */
/* #define MCUBOOT_FIH_PROFILE_HIGH   */

/* ======================================================================== */
/*  Bootstrap (allow booting a newer image in secondary slot)               */
/* ======================================================================== */
/* #define MCUBOOT_BOOTSTRAP 1 */

/* ======================================================================== */
/*  Use snprintf (provided by standard C library)                           */
/* ======================================================================== */
#define MCUBOOT_USE_SNPRINTF   1

/* ======================================================================== */
/*  CPU idle hook                                                           */
/* ======================================================================== */
#define MCUBOOT_CPU_IDLE()     ((void)0)

/* ======================================================================== */
/*  Watchdog feed                                                           */
/* ======================================================================== */
/*
 * MCUBoot calls MCUBOOT_WATCHDOG_FEED() during long operations (erase,
 * swap) to prevent watchdog resets.  For bare-metal bring-up without a
 * watchdog driver, define it as a no-op.
 */
#define MCUBOOT_WATCHDOG_FEED()  ((void)0)

/* ======================================================================== */
/*  Flash sector API selection                                              */
/* ======================================================================== */
/*
 * Use flash_area_get_sectors() instead of the deprecated flash_area_to_sectors().
 * Our flash_map_backend.c implements flash_area_get_sectors(), so enable this.
 */
#define MCUBOOT_USE_FLASH_AREA_GET_SECTORS

/* ======================================================================== */
/*  Assert                                                                  */
/* ======================================================================== */
/*
 * Use standard assert() by default. The platform can provide its own
 * mcuboot_assert.h via MCUBOOT_HAVE_ASSERT_H if desired.
 */
/* #define MCUBOOT_HAVE_ASSERT_H */

#endif /* __MCUBOOT_CONFIG_H__ */