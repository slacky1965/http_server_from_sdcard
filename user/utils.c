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

size_t get_sd_free_space() {
    FATFS *fs;
    size_t fre_clust;

    f_getfree("", &fre_clust, &fs);


    return ((fre_clust * fs->csize) / 2);
}

