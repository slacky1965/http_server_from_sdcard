#ifndef USER_INCLUDE_HTTP_H_
#define USER_INCLUDE_HTTP_H_

extern HttpdBuiltInUrl builtInUrls[];

int ICACHE_FLASH_ATTR cgi_upload (HttpdConnData *connData);
int ICACHE_FLASH_ATTR cgi_response (HttpdConnData *connData);

#endif /* USER_INCLUDE_HTTP_H_ */
