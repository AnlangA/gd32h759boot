#pragma once

#include "log.h"

/* Map MCUboot logging macros to our logging system */
#define MCUBOOT_LOG_ERR(...) Log_error(__VA_ARGS__)
#define MCUBOOT_LOG_WRN(...) Log_warn(__VA_ARGS__)
#define MCUBOOT_LOG_INF(...) Log_info(__VA_ARGS__)
#define MCUBOOT_LOG_DBG(...) Log_debug(__VA_ARGS__)
#define MCUBOOT_LOG_SIM(...) Log_info(__VA_ARGS__)

/* Module support - simplified implementation */
#define MCUBOOT_LOG_MODULE_DECLARE(module)
#define MCUBOOT_LOG_MODULE_REGISTER(module)
