/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2024 GD32H759 Bootloader Project
 *
 * MCUBoot logging adapter for GD32H759 bare-metal platform.
 * Maps MCUBoot's logging macros to the project's printf-based console output.
 */

#ifndef __MCUBOOT_LOGGING_H__
#define __MCUBOOT_LOGGING_H__

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Map MCUBoot log levels to printf-based output.
 * The project uses USART0 via console_usart for printf redirection.
 *
 * Log levels:
 *   MCUBOOT_LOG_ERR  - Error conditions
 *   MCUBOOT_LOG_WRN  - Warnings
 *   MCUBOOT_LOG_INF  - Informational messages
 *   MCUBOOT_LOG_DBG  - Debug messages (verbose)
 *   MCUBOOT_LOG_SIM  - Simulation / trace messages
 */

#define MCUBOOT_LOG_MODULE_DECLARE(module)
#define MCUBOOT_LOG_MODULE_REGISTER(module)

#define MCUBOOT_LOG_ERR(...)   do { printf("[MCUBOOT][ERR] " __VA_ARGS__); printf("\r\n"); } while(0)
#define MCUBOOT_LOG_WRN(...)   do { printf("[MCUBOOT][WRN] " __VA_ARGS__); printf("\r\n"); } while(0)
#define MCUBOOT_LOG_INF(...)   do { printf("[MCUBOOT][INF] " __VA_ARGS__); printf("\r\n"); } while(0)
#define MCUBOOT_LOG_DBG(...)   do { printf("[MCUBOOT][DBG] " __VA_ARGS__); printf("\r\n"); } while(0)
#define MCUBOOT_LOG_SIM(...)   do { printf("[MCUBOOT][SIM] " __VA_ARGS__); printf("\r\n"); } while(0)

#ifdef __cplusplus
}
#endif

#endif /* __MCUBOOT_LOGGING_H__ */