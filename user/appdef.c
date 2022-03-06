#include "ets_sys.h"
#include "osapi.h"
#include "os_type.h"
#include "gpio.h"

#include "user_interface.h"
#include "c_types.h"

#include "appdef.h"

const int32_t pin_name[GPIOMAX] = {
    PERIPHS_IO_MUX_GPIO0_U,     /* 0 -  GPIO0   */
    PERIPHS_IO_MUX_U0TXD_U,     /* 1 -  UART    */
    PERIPHS_IO_MUX_GPIO2_U,     /* 2 -  GPIO2   */
    PERIPHS_IO_MUX_U0RXD_U,     /* 3 -  UART    */
    PERIPHS_IO_MUX_GPIO4_U,     /* 4 -  GPIO4   */
    PERIPHS_IO_MUX_GPIO5_U,     /* 5 -  GPIO5   */
    -1,                         /* 6            */
    -1,                         /* 7            */
    -1,                         /* 8            */
    PERIPHS_IO_MUX_SD_DATA2_U,  /* 9 -  GPIO9   */
    PERIPHS_IO_MUX_SD_DATA3_U,  /* 10 - GPIO10  */
    -1,                         /* 11           */
    PERIPHS_IO_MUX_MTDI_U,      /* 12 - GPIO12  */
    PERIPHS_IO_MUX_MTCK_U,      /* 13 - GPIO13  */
    PERIPHS_IO_MUX_MTMS_U,      /* 14 - GPIO14  */
    PERIPHS_IO_MUX_MTDO_U       /* 15 - GPIO15  */
};

const int32_t func[GPIOMAX] = {
    FUNC_GPIO0,
    FUNC_U0TXD,
    FUNC_GPIO2,
    FUNC_GPIO3,
    FUNC_GPIO4,
    FUNC_GPIO5,
    -1,
    -1,
    -1,
    FUNC_GPIO9,
    FUNC_GPIO10,
    -1,
    FUNC_GPIO12,
    FUNC_GPIO13,
    FUNC_GPIO14,
    FUNC_GPIO15
};

