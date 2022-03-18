#ifndef USER_INCLUDE_SDCARD_H_
#define USER_INCLUDE_SDCARD_H_

#include "ff.h"

#define CS      15
#define MOSI    13
#define SCLK    14
#define MISO    12

// SD card commands
/** GO_IDLE_STATE - init card in spi mode if CS low */
#define CMD0    0X00
#define CMD0_CRC 0x95
/** SEND_OP_COND - Initiate initialization process for Ver.1 or MMC **/
#define CMD1    0x01
/** SEND_IF_COND - verify SD Memory Card interface operating condition.*/
#define CMD8    0X08
#define CMD8_CRC 0x87
#define CMD8_3V3_MODE_ARG 0x1AA
/** SEND_CSD - read the Card Specific Data (CSD register) */
#define CMD9    0X09
/** SEND_CID - read the card identification information (CID register) */
#define CMD10   0X0A
/** SEND_STATUS - read the card status register */
#define CMD13   0X0D
/** set block length*/
#define CMD16   0X10
/** READ_BLOCK - read a single data block from the card */
#define CMD17   0X11
/** WRITE_BLOCK - write a single data block to the card */
#define CMD24   0X18
/** WRITE_MULTIPLE_BLOCK - write blocks of data until a STOP_TRANSMISSION */
#define CMD25   0X19
/** ERASE_WR_BLK_START - sets the address of the first block to be erased */
#define CMD32   0X20
/** ERASE_WR_BLK_END - sets the address of the last block of the continuous range to be erased*/
#define CMD33   0X21
/** ERASE - erase all previously selected blocks */
#define CMD38   0X26
/** APP_CMD - escape for application specific command */
#define CMD55   0X37
/** READ_OCR - read the OCR register of a card */
#define CMD58   0X3A
/** CRC_ON_OFF - turn the CRC option on or off */
#define CMD59   0X3B
/** SET_WR_BLK_ERASE_COUNT - Set the number of write blocks to be pre-erased before writing */
#define ACMD23  0X17
/** SD_SEND_OP_COMD - Sends host capacity support information and activates the card's initialization process */
#define ACMD41          0X29
#define ACMD41_ARG_V1   0x00
#define ACMD41_ARG_V2   0X40000000
#define CMD_OK          0x01
//------------------------------------------------------------------------------
/** status for card in the ready state */
#define R1_READY_STATE          0X00
/** status for card in the idle state DATA_START_BLOCK*/
#define R1_IDLE_STATE           0X01
/** status bit for illegal command */
#define R1_ILLEGAL_COMMAND      0X04
/** start data token for read or write single block*/
#define DATA_START_BLOCK        0XFE
/** stop token for write multiple blocks*/
#define STOP_TRAN_TOKEN         0XFD
/** start data token for write multiple blocks*/
#define WRITE_MULTIPLE_TOKEN    0XFC
/** mask for data response tokens after a write block operation */
#define DATA_RES_MASK           0X1F
/** write data accepted token */
#define DATA_RES_ACCEPTED       0X05
//------------------------------------------------------------------------------
/** Set 30-th bit for check Card Capacity Status - CCS **/
#define CCS (1 << 30)
//------------------------------------------------------------------------------
#define CSD_VER1    0x00
#define CSD_VER2    0x01
#define CSD_VER3    0x02
#define CSD_SIZE    0x10
//------------------------------------------------------------------------------

typedef enum {
    SD_CARD_OK = 0,
    SD_CARD_ERROR_INIT          = 0x01,
    SD_CARD_ERROR_READ          = 0x02,
    SD_CARD_ERROR_WRITE         = 0x04,
    SD_CARD_ERROR_TIMEOUT       = 0x08,
    SD_CARD_ERROR_INIT_GPIO     = 0x10,
    SD_CARD_ERROR_ALLOCATE      = 0x20,
    SD_CARD_ERROR_NOT_SUPPORT   = 0x40,
    SD_CARD_ERROR_CMD_EXECUTE   = 0x80
} sdcard_err_t;

typedef enum {
    SD_CARD_TYPE_SD1SC = 1,
    SD_CARD_TYPE_SD2SC,
    SD_CARD_TYPE_SD2HC,
    SD_CARD_TYPE_SD3UC,
    SD_CARD_TYPE_MAX
} sdcard_type_t;

typedef struct {
    uint8_t         csd_version;
    sdcard_type_t   type;
    uint64_t        capacity;
    uint32_t        sector_size;
    bool            sdcard_init;
    sdcard_err_t    sdcard_err;
} sdcard_t;

sdcard_err_t sd_init();
int sd_read_sector(uint32_t start_block, uint8_t *buffer, uint32_t sector_count);
int sd_write_sector(uint32_t start_block, uint8_t *buffer, uint32_t sector_count);
bool get_sdcard_status();
uint64_t sd_get_capacity();
uint32_t sd_get_sector_size();

#endif /* USER_INCLUDE_SDCARD_H_ */
