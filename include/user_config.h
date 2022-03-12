#ifndef __USER_CONFIG_H__
#define __USER_CONFIG_H__

#define LOG_DEBUG

#ifndef LOG_DEBUG
#define log_print(fmt, ...) ets_uart_printf(fmt, ##__VA_ARGS__)
#else
#define log_print(fmt, ...) os_printf(fmt, ##__VA_ARGS__)
#endif

extern int ets_uart_printf(const char *fmt, ...);

#endif
