#ifndef USER_INCLUDE_APPDEF_H_
#define USER_INCLUDE_APPDEF_H_

#ifndef SPI_FLASH_SIZE_MAP
#define SPI_FLASH_SIZE_MAP 6
#endif

#ifndef OTA_FLASH_SIZE_K
#define OTA_FLASH_SIZE_K 4096
#endif

#if SPI_FLASH_SIZE_MAP == 3
#define FIRMWARE_POS      ((OTA_FLASH_SIZE_K/2)*1024/2)+0x1000
#define FIRMWARE_SIZE     ((OTA_FLASH_SIZE_K/2)*1024/2)-0x1000
#elif SPI_FLASH_SIZE_MAP == 4
#define FIRMWARE_POS      ((OTA_FLASH_SIZE_K/4)*1024/2)+0x1000
#define FIRMWARE_SIZE     ((OTA_FLASH_SIZE_K/4)*1024/2)-0x1000
#else
#define FIRMWARE_POS      ((OTA_FLASH_SIZE_K*1024)/2)+0x1000
#define FIRMWARE_SIZE     ((OTA_FLASH_SIZE_K*1024)/2)-0x1000
#endif


typedef enum {
    GPIO0 = 0,
    GPIO1,
    GPIO2,
    GPIO3,
    GPIO4,
    GPIO5,
    GPIO6,
    GPIO7,
    GPIO8,
    GPIO9,
    GPIO10,
    GPIO11,
    GPIO12,
    GPIO13,
    GPIO14,
    GPIO15,
    GPIOMAX
} gpio_num_t;

extern const int32_t pin_name[GPIOMAX];
extern const int32_t func[GPIOMAX];

#endif /* USER_INCLUDE_APPDEF_H_ */
