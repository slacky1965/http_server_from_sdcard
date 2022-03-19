#include "user_interface.h"
#include "osapi.h"
#include "c_types.h"
#include "user_config.h"

#include "mem.h"

#include "appdef.h"
#include "sdcard.h"
#include "utils.h"

static sdcard_t sdcard;
static char csd[CSD_SIZE] = {0};
static const char *sd_type[] = {
        "Unknown SD type",
        "SD type SDSC V.1",
        "SD type SDSC V.2",
        "SD type SDHC V.2",
        "SD type SDUC V.3"
};

static os_timer_t sd_timer;
static uint8_t count_error = 0;

static void ICACHE_FLASH_ATTR sd_timer_cb(void *arg) {

    os_timer_disarm(&sd_timer);

    if (sdcard.sdcard_init) {
        if (sdcard.sdcard_err) {
            if (count_error++ > 2) {
                sd_init();
                count_error = 0;
            }
        } else count_error = 0;
    }

    os_timer_arm(&sd_timer, 1000, false);
}


static sdcard_err_t ICACHE_FLASH_ATTR sd_gpio_init() {

    if (CS < GPIO0 || CS >= GPIOMAX) {
        os_printf("Invalide GPIO_CS - %d, must be %d-%d. (%s:%u)\n", CS, GPIO0, GPIOMAX-1, __FILE__, __LINE__);
        return SD_CARD_ERROR_INIT_GPIO;
    } else if (MOSI < GPIO0 || MOSI >= GPIOMAX) {
        os_printf("Invalide GPIO_MOSI - %d, must be %d-%d. (%s:%u)\n", MOSI, GPIO0, GPIOMAX-1, __FILE__, __LINE__);
        return SD_CARD_ERROR_INIT_GPIO;
    } else if (SCLK < GPIO0 || SCLK >= GPIOMAX) {
        os_printf("Invalide GPIO_sclk - %d, must be %d-%d. (%s:%u)\n", SCLK, GPIO0, GPIOMAX-1, __FILE__, __LINE__);
        return SD_CARD_ERROR_INIT_GPIO;
    } else if (MISO < GPIO0 || MISO >= GPIOMAX) {
        os_printf("Invalide GPIO_MISO - %d, must be %d-%d. (%s:%u)\n", MISO, GPIO0, GPIOMAX-1, __FILE__, __LINE__);
        return SD_CARD_ERROR_INIT_GPIO;
    }

    if (pin_name[CS] == -1) {
        os_printf("Invalide GPIO_CS - %d. (%s:%u)\n", CS, __FILE__, __LINE__);
        return SD_CARD_ERROR_INIT_GPIO;
    }

    if (pin_name[MOSI] == -1) {
        os_printf("Invalide GPIO_MOSI - %d. (%s:%u)\n", MOSI, __FILE__, __LINE__);
        return SD_CARD_ERROR_INIT_GPIO;
    }

    if (pin_name[SCLK] == -1) {
        os_printf("Invalide GPIO_SCLK - %d. (%s:%u)\n", SCLK, __FILE__, __LINE__);
        return SD_CARD_ERROR_INIT_GPIO;
    }

    if (pin_name[MISO] == -1) {
        os_printf("Invalide GPIO_MISO - %d. (%s:%u)\n", MISO, __FILE__, __LINE__);
        return SD_CARD_ERROR_INIT_GPIO;
    }

    //CS
    PIN_FUNC_SELECT(pin_name[CS], func[CS]);
    PIN_PULLUP_EN(pin_name[CS]);
    //MOSI
    PIN_FUNC_SELECT(pin_name[MOSI], func[MOSI]);
    PIN_PULLUP_EN(pin_name[MOSI]);
    //SCLK
    PIN_FUNC_SELECT(pin_name[SCLK], func[SCLK]);
    PIN_PULLUP_EN(pin_name[SCLK]);
    //MISO
    PIN_FUNC_SELECT(pin_name[MISO], func[MISO]);
//    PIN_PULLUP_DIS(pin_name[MISO]);
    PIN_PULLUP_EN(pin_name[MISO]);
    GPIO_DIS_OUTPUT(GPIO_ID_PIN(MISO));

    return SD_CARD_OK;
}

static void ICACHE_FLASH_ATTR sd_write_byte(uint8_t  data) {
    sint8_t i;
    uint8_t tmp;
    for(i = 7; i >= 0; i--) {
        tmp = (data >> i) & 0x1;
        GPIO_OUTPUT_SET(MOSI, tmp);
        GPIO_OUTPUT_SET(SCLK, 0);
        GPIO_OUTPUT_SET(SCLK, 1);
    }
    GPIO_OUTPUT_SET(MOSI, 1);
}

static uint8_t ICACHE_FLASH_ATTR sd_read_byte() {
    uint8_t data = 0x00, i;
    for(i = 8; i; i--) {
        GPIO_OUTPUT_SET(SCLK, 0);
        GPIO_OUTPUT_SET(SCLK, 1);
        data <<= 1;
        if(GPIO_INPUT_GET(MISO)){
            data |= 0x1;
        }
    }
    GPIO_OUTPUT_SET(SCLK, 0);
    return data;
}

static uint8_t ICACHE_FLASH_ATTR sd_send_cmd(uint8_t command, uint32_t argument, uint8_t crc, uint32_t *response_not_r1) {
    sd_write_byte(command | 0x40);
    sd_write_byte((argument >> 24) & 0xFF);
    sd_write_byte((argument >> 16) & 0xFF);
    sd_write_byte((argument >> 8) & 0xFF);
    sd_write_byte(argument & 0xFF);
    sd_write_byte(crc);
    sd_write_byte(0xFF);
    uint8_t response = sd_read_byte();
    if (response_not_r1) {
        *response_not_r1 = 0;
        if (command == CMD8 || command == CMD58) {
            uint32_t ret = ((sd_read_byte() & 0xff) << 24) & 0xff000000;
            *response_not_r1 |= ret;
            ret = ((sd_read_byte() & 0xff) << 16) & 0xff0000;
            *response_not_r1 |= ret;
            ret = ((sd_read_byte() & 0xff) << 8) & 0xff00;
            *response_not_r1 |= ret;
            ret = sd_read_byte() & 0xff;
            *response_not_r1 |= ret;
        }
    }
    return response;
}

static void ICACHE_FLASH_ATTR sd_release() {
    GPIO_OUTPUT_SET(CS, 1);
    sd_write_byte(0xFF);
}

static void ICACHE_FLASH_ATTR sd_select() {
    GPIO_OUTPUT_SET(CS, 0);
}

static void ICACHE_FLASH_ATTR sd_csd_read() {
    uint8_t response_r1;
    uint32_t count;

    sd_select();

    response_r1 = sd_send_cmd(CMD9, 0, 0x01, NULL);

    if (response_r1 == R1_READY_STATE) {

        count = 0;
        do {
            response_r1 = sd_read_byte();
            count++;
            if (count > 1024) {
                os_printf("sd_csd_read: Read to CSD failed! (%s:%u)\n", __FILE__, __LINE__);
                sd_release();
                return;
            }
        } while (response_r1 != DATA_START_BLOCK);

        for(count = 0; count < CSD_SIZE; count++) {
            csd[count] = sd_read_byte();
        }

        sd_write_byte(0xFF);
        sd_write_byte(0xFF);

        sd_release();

        sdcard.csd_version = csd[0] >> 6;

        if (sdcard.csd_version == CSD_VER3) {
            sdcard.type = SD_CARD_TYPE_SD3UC;
        }

        os_printf("Detected %s\n", sd_type[sdcard.type]);

        if (sdcard.csd_version == CSD_VER1) {
            /* not tested */
            uint32_t c_size = ((csd[6] & 0x03) << 10) | (csd[7] << 2) | (csd[8] >> 6);
            uint32_t read_bl_len = csd[5] & 0x0F;
            uint32_t c_size_mult = ((csd[9] & 0x03) << 1) | ((csd[10] & 0x80) >> 7);
//            sdcard.capacity = (c_size + 1) << (c_size_mult + 2 + read_bl_len);
            sdcard.capacity = (c_size + 1) ;
            sdcard.capacity *= (1 << (c_size_mult + 2));
            sdcard.sector_size = 1 << read_bl_len;
            sdcard.capacity *= sdcard.sector_size;
        } else if (sdcard.csd_version == CSD_VER2) {
            sdcard.capacity = ((((csd[7] >> 2) << 16) | (csd[8] << 8) | csd[9])+1)*512;
            sdcard.capacity *= 1024ULL;
            sdcard.sector_size = 1 << (csd[5] & 0x0F);
        } else if (sdcard.csd_version == CSD_VER3) {
            /* not tested */
            sdcard.capacity = ((((csd[6] & 0x0F) << 24) | (csd[7] << 16) | (csd[8] << 8) | csd[9])+1)*512;
            sdcard.capacity *= 1024ULL;
            sdcard.sector_size = 1 << (csd[5] & 0x0F);
        } else {
            os_printf("sd_csd_read: Unknown CSD version. (%s:%u)\n", __FILE__, __LINE__);
            return;
        }

        os_printf("SD capacity %llu kBytes\n", (sdcard.capacity/1024));
        os_printf("SD sector size %d Bytes\n", sdcard.sector_size);
    }

}

sdcard_err_t ICACHE_FLASH_ATTR sd_init() {
    uint8_t response_r1;
    uint32_t count;
    uint32_t response_not_r1;

    os_bzero(&sdcard, sizeof(sdcard_t));
    sdcard.sector_size = 512;
    sdcard.sdcard_err = SD_CARD_ERROR_INIT;

    if (sd_gpio_init() != SD_CARD_OK) {
        os_printf("sd_init: Initialize gpio failed! (%s:%u)\n", __FILE__, __LINE__);
        return SD_CARD_ERROR_INIT_GPIO;
    }

    os_delay_us(1000);

    GPIO_OUTPUT_SET(CS, 1);
    for (int i = 0; i < 10; i++) {
        sd_write_byte(0xFF);
    }
    GPIO_OUTPUT_SET(CS, 0);

    count = 0;
    /* command CMD0 */
    do {
        response_r1 = sd_send_cmd(CMD0, 0, CMD0_CRC, NULL);
        count++;
        if(count >= 1024){
            sd_release();
            os_printf("sd_init: Command CMD0 failed! (%s:%u)\n", __FILE__, __LINE__);
            return SD_CARD_ERROR_CMD_EXECUTE;
        }
    } while(response_r1 != R1_IDLE_STATE);

    /* command CMD8 */
    response_r1 = sd_send_cmd(CMD8, CMD8_3V3_MODE_ARG, CMD8_CRC, &response_not_r1);
    if (response_r1 & R1_ILLEGAL_COMMAND) {
        /* SD Card Ver.1 */
        sdcard.type = SD_CARD_TYPE_SD1SC;
        count = 0;
        do {
            sd_send_cmd(CMD55, 0, 0XFF, NULL);
            response_r1 = sd_send_cmd(ACMD41, ACMD41_ARG_V1, 0XFF, NULL);
            count++;
            if (count >= 1024) {
                sd_release();
                os_printf("sd_init: Command ACMD41 execute failed! (%s:%u)\n", __FILE__, __LINE__);
                return SD_CARD_ERROR_CMD_EXECUTE;
            }
        } while (response_r1 != R1_READY_STATE || (response_r1 & R1_ILLEGAL_COMMAND));
        if (response_r1 == R1_READY_STATE) {
            sd_write_byte(0xFF);
            sd_send_cmd(CMD16, 512, 0XFF, NULL);
        } else /*if (response_r1 & R1_ILLEGAL_COMMAND)*/ {
            count = 0;
            do {
                response_r1 = sd_send_cmd(CMD1, 0, 0XFF, NULL);
                count++;
                if (count >= 1024) {
                    sd_release();
                    os_printf("sd_init: Command CMD1 execute failed! (%s:%u)\n", __FILE__, __LINE__);
                    return SD_CARD_ERROR_CMD_EXECUTE;
                }

            } while (response_r1 != R1_READY_STATE || (response_r1 & R1_ILLEGAL_COMMAND));
            if (response_r1 == R1_READY_STATE) {
                sd_write_byte(0xFF);
                sd_send_cmd(CMD16, 512, 0XFF, NULL);
            } else {
                sd_release();
                os_printf("sd_init: Not supported this card! (%s:%u)\n", __FILE__, __LINE__);
                return SD_CARD_ERROR_NOT_SUPPORT;
            }
        }
    } else {
        if (response_not_r1 == CMD8_3V3_MODE_ARG) {
            /* SD Card Ver.2 */
            sdcard.type = SD_CARD_TYPE_SD2SC;
            count = 0;
            do {
                sd_send_cmd(CMD55, 0, 0xFF, NULL);
                response_r1 = sd_send_cmd(ACMD41, ACMD41_ARG_V2, 0xFF, NULL);
                count++;
                if(count >= 1024) {
                    sd_release();
                    os_printf("sd_init: Command ACMD41 execute failed! (%s:%u)\n", __FILE__, __LINE__);
                    return SD_CARD_ERROR_CMD_EXECUTE;
                }
            } while (response_r1 != R1_READY_STATE);

            response_r1 = sd_send_cmd(CMD58, 0, 0xFF, &response_not_r1);
            if (response_not_r1 & CCS) {
                sdcard.type = SD_CARD_TYPE_SD2HC;
            } else {
                sd_write_byte(0xFF);
                sd_send_cmd(CMD16, 512, 0XFF, NULL);
            }
        } else {
            sd_release();
            os_printf("sd_init: Not supported this card! (%s:%u)\n", __FILE__, __LINE__);
            return SD_CARD_ERROR_NOT_SUPPORT;
        }
    }

    sd_release();

    sdcard.sdcard_init = true;
    sdcard.sdcard_err = SD_CARD_OK;

    sd_csd_read();

    os_timer_disarm(&sd_timer);
    os_timer_setfn(&sd_timer, (os_timer_func_t *)sd_timer_cb, NULL);
    os_timer_arm(&sd_timer, 1000, false);


    return SD_CARD_OK;
}

int ICACHE_FLASH_ATTR sd_read_sector(uint32_t start_block, uint8_t *buffer, uint32_t sector_count) {

    uint8_t response;
    int count;

    if (sector_count == 0) return 0;

    sd_select();
    while (sector_count--) {
        response = sd_send_cmd(CMD17, start_block++, 0x01, NULL);
        if (response != R1_READY_STATE) {
            sd_release();
            os_printf("sd_read_sector: Bad response 0x%x. (%s:%u)\n", response, __FILE__, __LINE__);
            sdcard.sdcard_err = SD_CARD_ERROR_READ;
            return 0;
        }

        count = 0;
        do {
            response = sd_read_byte();
            count++;
            if(count > 65535) {
                sd_release();
                os_printf("sd_read_sector: Timeout waiting for data ready. (%s:%u)\n", __FILE__, __LINE__);
                sdcard.sdcard_err = SD_CARD_ERROR_READ | SD_CARD_ERROR_TIMEOUT;
                return 0;
            }
//            system_soft_wdt_feed();
        } while(response != DATA_START_BLOCK);

        for(count = 0; count < 512; count++) {
            *(buffer + count) = sd_read_byte();
        }

        buffer += 512;

        sd_write_byte(0xFF);
        sd_write_byte(0xFF);
        sd_write_byte(0xFF);
    }
    sd_release();

    return 1;
}

int ICACHE_FLASH_ATTR sd_write_sector(uint32_t start_block, uint8_t *buffer, uint32_t sector_count) {

    uint8_t response;
    int count;

    sd_select();
    while (sector_count--) {
        count = 0;
        do {
            response = sd_send_cmd(CMD24, start_block++, 0x00, NULL);
            count++;
            if(count > 200) {
                sd_release();
                os_printf("sd_write_sector: Bad response 0x%x. (%s:%u)\n", response, __FILE__, __LINE__);
                sdcard.sdcard_err = SD_CARD_ERROR_WRITE;
                return 0;
            }
        } while(response != R1_READY_STATE);

        sd_write_byte(0xFF);
        sd_write_byte(0xFF);
        sd_write_byte(0xFF);

        sd_write_byte(DATA_START_BLOCK);
        for (count = 0; count < 512; count++) {
            sd_write_byte(*(buffer++));
        }
        sd_write_byte(0xFF);
        sd_write_byte(0xFF);
        response = sd_read_byte();
        if ((response & DATA_RES_MASK) != DATA_RES_ACCEPTED) {
            sd_release();
            os_printf("sd_write_sector: Data rejected 0x%x. (%s:%u)\n", response, __FILE__, __LINE__);
            sdcard.sdcard_err = SD_CARD_ERROR_WRITE;
            return 0;
        }

        count = 0;
        do {
            response = sd_read_byte();
            count++;
            if (count > 65535) {
                sd_release();
                os_printf("sd_write_sector: Timeout waiting for data write complete. (%s:%u)\n", __FILE__, __LINE__);
                sdcard.sdcard_err = SD_CARD_ERROR_WRITE | SD_CARD_ERROR_TIMEOUT;
                return 0;
            }
//            system_soft_wdt_feed();
        } while (response != 0xFF);
    }
    sd_release();
    return 1;
}

bool ICACHE_FLASH_ATTR get_sdcard_status() {
	return sdcard.sdcard_init;
}

uint64_t ICACHE_FLASH_ATTR sd_get_capacity() {
    return sdcard.capacity;
}

uint32_t ICACHE_FLASH_ATTR sd_get_sector_size() {
    return sdcard.sector_size;
}
