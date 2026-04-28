// Logger.h - 最小化日志存根
#pragma once

// 禁用日志输出的宏，全部展开为空
#define LOG_INFO(msg)  ((void)0)
#define LOG_WARN(msg)  ((void)0)
#define LOG_ERROR(msg) ((void)0)