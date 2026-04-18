/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2024 GD32H759 Bootloader Project
 *
 * Bare-metal memory allocation for MCUBoot on GD32H7xx.
 *
 * This file exists solely because mcuboot/boot/bootutil/src/loader.c
 * contains the following conditional include:
 *
 *   #if !defined(MCUBOOT_DIRECT_XIP) && !defined(MCUBOOT_RAM_LOAD)
 *   #include <os/os_malloc.h>
 *   #endif
 *
 * In overwrite-only and swap modes, this header MUST be resolvable.
 * For a bare-metal bootloader we simply delegate to the C standard
 * library heap — no OS, no custom allocator, no extra indirection.
 */

#ifndef H_OS_MALLOC_
#define H_OS_MALLOC_

#include <stdlib.h>

/*
 * Redirect os_ prefixed names to stdlib.
 * These are only needed if any mcuboot code calls os_malloc() directly;
 * the #undef / #define below handles cases where mcuboot code uses
 * bare malloc/free/realloc after including this header.
 */
#define os_malloc   malloc
#define os_free     free
#define os_realloc  realloc

#endif /* H_OS_MALLOC_ */