#ifndef USER_INCLUDE_UTILS_H_
#define USER_INCLUDE_UTILS_H_

char* ICACHE_FLASH_ATTR get_ip_address(uint8_t mode);
char* ICACHE_FLASH_ATTR get_mac_address(uint8_t if_index);
size_t get_sd_free_space();

#endif /* USER_INCLUDE_UTILS_H_ */
