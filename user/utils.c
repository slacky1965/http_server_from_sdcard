#include "osapi.h"
#include "user_interface.h"

#include "utils.h"
#include "ff.h"

static char buff[32] = {0};

char* ICACHE_FLASH_ATTR get_ip_address(uint8_t mode) {
    struct ip_info ip;
    wifi_get_ip_info(mode, &ip);
    os_sprintf(buff, "%d.%d.%d.%d", ip.ip.addr & 0xFF, (ip.ip.addr >> 8) & 0xFF,
            (ip.ip.addr >> 16) & 0xFF, (ip.ip.addr >> 24) & 0xFF);

    return buff;
}



char* ICACHE_FLASH_ATTR get_mac_address(uint8_t if_index) {
    uint8_t mac[6];
    wifi_get_macaddr(if_index, mac);
//  os_sprintf(buff, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    os_sprintf(buff, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    return buff;
}

uint32_t ICACHE_FLASH_ATTR get_sd_free_space(uint32_t *total) {
    FATFS *fs;
    DWORD free_clust, free_size, total_size;


    /* Get volume information and free clusters of drive 1 */
    if (f_getfree("", &free_clust, &fs) != FR_OK) {
        os_printf("f_getfree return error\n");
        return 0;
    }

    /* Get total sectors and free sectors */
    total_size = (fs->n_fatent - 2) * fs->csize / 2;
    free_size = free_clust * fs->csize / 2;

    /* Print the free space (assuming 512 bytes/sector) */
//    os_printf("%10lu KiB total drive space.\n%10lu KiB available.\n", total_size, free_size);

    if(total) *total = total_size;

    return free_size;
}

