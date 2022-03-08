#include "osapi.h"
#include "user_interface.h"
#include "user_config.h"
#include "mem.h"

#include "appdef.h"
#include "sdcard.h"
#include "ff.h"
#include "wifi.h"
#include "platform.h"
#include "httpd.h"
#include "cgiflash.h"
#include "http.h"

#ifndef OTA_TAGNAME
#define OTA_TAGNAME "generic"
#endif

#define RW_BUFF_LEN 1024

CgiUploadFlashDef uploadParams={
    .type=CGIFLASH_TYPE_FW,
    .fw1Pos=0x1000,
#if OTA_FLASH_MAP == 6 || OTA_FLASH_MAP == 3
    .fw2Pos=((OTA_FLASH_SIZE_K*512)/2)+0x1000,
    .fwSize=((OTA_FLASH_SIZE_K*512)/2)-0x7000,
#else
    #if OTA_FLASH_MAP == 4
    .fw2Pos=((OTA_FLASH_MAP/2*512)/2)+0x1000,
    .fwSize=((OTA_FLASH_MAP/2*512)/2)-0x7000,
    #else
    .fw2Pos=((OTA_FLASH_MAP*1024)/2)+0x1000,
    .fwSize=((OTA_FLASH_MAP*1024)/2)-0x7000,
    #endif
#endif
    .tagName=OTA_TAGNAME
};

int ICACHE_FLASH_ATTR tpl_token(HttpdConnData *connData, char *token, void **arg);
typedef void (* TplCallback)(HttpdConnData *connData, char *token, void **arg);

HttpdBuiltInUrl builtInUrls[] = {
        {"/", cgiRedirect, "/index.html"},
        {"/upload/*", cgi_upload, &uploadParams},
		{"/scripts.js", cgi_response, NULL},
        {"*", cgi_response, tpl_token},
        {NULL, NULL, NULL}
};

typedef struct {
    FIL file;
    void *tpl_arg;
    char token[64];
    int token_pos;
} html_data_t;

int ICACHE_FLASH_ATTR cgi_upload (HttpdConnData *connData) {
    if (connData->conn==NULL) {
        //Connection aborted. Clean up.
        return HTTPD_CGI_DONE;
    }
    return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR response_with_tpl(HttpdConnData *connData) {
    char buff[RW_BUFF_LEN + 1];
    size_t len = 0, x, sp = 0;
    char *e=NULL;
    FRESULT ret;

    html_data_t *html_data = connData->cgiData;

    ret = f_read(&(html_data->file), buff, OTA_BUF_LEN, &len);
    if (len>0) {
        sp=0;
        e=buff;
        for (x=0; x<len; x++) {
            if (html_data->token_pos==-1) {
                //Inside ordinary text.
                if (buff[x]=='%') {
                    //Send raw data up to now
                    if (sp!=0) httpdSend(connData, e, sp);
                    sp=0;
                    //Go collect token chars.
                    html_data->token_pos=0;
                } else {
                    sp++;
                }
            } else {
                if (buff[x]=='%') {
                    if (html_data->token_pos==0) {
                        //This is the second % of a %% escape string.
                        //Send a single % and resume with the normal program flow.
                        httpdSend(connData, "%", 1);
                    } else {
                        //This is an actual token.
                        html_data->token[html_data->token_pos++]=0; //zero-terminate token
                        ((TplCallback)(connData->cgiArg))(connData, html_data->token, &html_data->tpl_arg);
                    }
                    //Go collect normal chars again.
                    e=&buff[x+1];
                    html_data->token_pos=-1;
                } else {
                    if (html_data->token_pos<(sizeof(html_data->token)-1)) html_data->token[html_data->token_pos++]=buff[x];
                }
            }
        }
    }
    //Send remaining bit.
    if (sp!=0) httpdSend(connData, e, sp);
    if (len != RW_BUFF_LEN) {
        //We're done.
        ((TplCallback)(connData->cgiArg))(connData, NULL, &html_data->tpl_arg);
        f_close(&(html_data->file));
        free(html_data);
        return HTTPD_CGI_DONE;
    } else {
        //Ok, till next time.
        return HTTPD_CGI_MORE;
    }

}

int ICACHE_FLASH_ATTR response_without_tpl(HttpdConnData *connData) {
    char buff[RW_BUFF_LEN];
    size_t len = 0;
    FRESULT ret;

    html_data_t *html_data = connData->cgiData;

    ret = f_read(&(html_data->file), buff, OTA_BUF_LEN, &len);

    if (len > 0) {
    	httpdSend(connData, buff, len);

    	if (len == RW_BUFF_LEN) {
            return HTTPD_CGI_MORE;
    	} else {
            f_close(&(html_data->file));
            free(html_data);
            return HTTPD_CGI_DONE;
    	}
    }
    f_close(&(html_data->file));
    free(html_data);
    return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR cgi_response (HttpdConnData *connData) {
    char buff[256];
    FRESULT ret;

    html_data_t *html_data = connData->cgiData;

    if (connData->conn==NULL) {
        //Connection aborted. Clean up.
        f_close(&(html_data->file));
        free(html_data);
        return HTTPD_CGI_DONE;
    }

    if (html_data == NULL) {
        //First call to this cgi. Open the file so we can read it.
        html_data = malloc(sizeof(html_data_t));
        if (html_data == NULL) {
            return HTTPD_CGI_NOTFOUND;
        }
        os_sprintf(buff, "%s%s", HTML_PATH, connData->url);
        html_data->tpl_arg=NULL;
        html_data->token_pos = -1;
        ret = f_open(&(html_data->file), buff, FA_READ);
        if (ret != FR_OK) {
            free(html_data);
            return HTTPD_CGI_NOTFOUND;
        }
        connData->cgiData=html_data;
        httpdStartResponse(connData, 200);
        httpdHeader(connData, "Content-Type", httpdGetMimetype(connData->url));
        httpdEndHeaders(connData);
        return HTTPD_CGI_MORE;
    }

    if (connData->cgiArg == NULL) {
    	return response_without_tpl(connData);
    }

    return response_with_tpl(connData);
}

int ICACHE_FLASH_ATTR tpl_token(HttpdConnData *connData, char *token, void **arg) {
	char buff[128], *tmp;
	if (token == NULL)
		return HTTPD_CGI_DONE;

	os_strcpy(buff, "Unknown");
	if (os_strcmp(token, "upgfile") == 0) {
		if (system_upgrade_userbin_check() == 1)
			os_strcpy(buff, "user1.bin");
		else
			os_strcpy(buff, "user2.bin");
	}

	httpdSend(connData, buff, -1);

	return HTTPD_CGI_DONE;
}
