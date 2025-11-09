#pragma once

#include "logging_stack.h"

#define Log_info(format, ...) LogInfo((format, ##__VA_ARGS__))
#define Log_warn(format, ...) LogWarn((format, ##__VA_ARGS__))
#define Log_error(format, ...) LogError((format, ##__VA_ARGS__))
#define Log_debug(format, ...) LogDebug((format, ##__VA_ARGS__))
#define Log_always(format, ...) LogAlways((format, ##__VA_ARGS__))
