// Genesis/SGDK 用 <stdio.h> 最小シム。
// newlib stdio.h を避け、Doom が使う printf のみ宣言する。
// sprintf / vsprintf は SGDK string.h（compiler.h 経由）が提供する。
#ifndef _STDIO_SHIM_H_
#define _STDIO_SHIM_H_

#include <stddef.h>

#if defined PEBBLE_EMERY
// Engine progress messages are dropped on the watch to save image space;
// fatal errors still reach the log through I_Error.
#define printf(...) ((void)0)
#else
int printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
#endif
int sprintf(char *str, const char *format, ...);
int snprintf(char *str, size_t size, const char *format, ...);

#endif
