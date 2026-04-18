/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2024 GD32H759 Bootloader Project
 *
 * Platform-specific implementation of mbedtls_platform_zeroize().
 *
 * This file is required because mcuboot-mbedtls-cfg.h defines
 * MBEDTLS_PLATFORM_ZEROIZE_ALT, which means mbedTLS expects the
 * application to provide its own secure zeroize implementation.
 *
 * On ARM Cortex-M7 we use a volatile function pointer to prevent
 * the compiler from optimising out the memset() call.
 */

#include <string.h>
#include <mbedtls/platform_util.h>

/*
 * Use a volatile pointer to memset to prevent the compiler from
 * optimising away the zeroing operation. This is the standard
 * technique used in embedded systems without explicit_bzero().
 */
static void *(*const volatile memset_func)(void *, int, size_t) = memset;

void mbedtls_platform_zeroize(void *buf, size_t len)
{
    if (buf != NULL && len > 0) {
        memset_func(buf, 0, len);
    }
}