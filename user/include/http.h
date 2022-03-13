#ifndef USER_INCLUDE_HTTP_H_
#define USER_INCLUDE_HTTP_H_

#define OTA_BUF_LEN 512
#define HTML_PATH "/html"

extern HttpdBuiltInUrl builtInUrls[];

int ICACHE_FLASH_ATTR cgi_upload(HttpdConnData *connData);
int ICACHE_FLASH_ATTR cgi_response(HttpdConnData *connData);
int ICACHE_FLASH_ATTR cgi_list(HttpdConnData *connData);
int ICACHE_FLASH_ATTR cgi_get_user_ota_filename(HttpdConnData *connData);

#endif /* USER_INCLUDE_HTTP_H_ */
