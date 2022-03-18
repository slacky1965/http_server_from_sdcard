#ifndef USER_INCLUDE_UTILS_H_
#define USER_INCLUDE_UTILS_H_

char* get_ip_address(uint8_t mode);
char* get_mac_address(uint8_t if_index);
uint32_t get_sd_free_space(uint32_t *total);

#endif /* USER_INCLUDE_UTILS_H_ */
