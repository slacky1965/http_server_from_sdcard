#ifndef USER_INCLUDE_HTTP_H_
#define USER_INCLUDE_HTTP_H_

#define OTA_BUF_LEN 512
#define HTML_PATH "/html"
#define JSON_DEL_FILES_ARRAY_NAME "Files"

extern HttpdBuiltInUrl builtInUrls[];

int tpl_token(HttpdConnData *connData, char *token, void **arg);
int cgi_upload(HttpdConnData *connData);
int cgi_response(HttpdConnData *connData);
int cgi_list(HttpdConnData *connData);
int cgi_delete(HttpdConnData *connData);
int cgi_get_user_ota_filename(HttpdConnData *connData);

#endif /* USER_INCLUDE_HTTP_H_ */
