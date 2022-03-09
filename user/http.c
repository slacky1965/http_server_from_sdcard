#include <stdlib.h>
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
#define PATH_HTML   "/html/"
#define PATH_IMAGE  "/image/"
#define PATH_UPLOAD "/upload/"


CgiUploadFlashDef upload_params={
	.type=CGIFLASH_TYPE_FW,
	.fw1Pos=0x1000,
	.fw2Pos=((OTA_FLASH_SIZE_K*1024)/2)+0x1000,
	.fwSize=((OTA_FLASH_SIZE_K*1024)/2)-0x1000,
	.tagName=OTA_TAGNAME
};


int ICACHE_FLASH_ATTR tpl_token(HttpdConnData *connData, char *token, void **arg);
typedef void (* TplCallback)(HttpdConnData *connData, char *token, void **arg);

HttpdBuiltInUrl builtInUrls[] = {
        {"/", cgiRedirect, "/index.html"},
        {"/upload/*", cgi_upload, &upload_params},
        {"/list", cgi_list, NULL},
//		{"/scripts.js", cgi_response, NULL},
        {"*", cgi_response, NULL},
        {NULL, NULL, NULL}
};

typedef struct {
    FIL    file;
    void  *tpl_arg;
    char   token[64];
    size_t token_pos;
} html_data_t;

typedef struct {
    DIR    dir;
    size_t gl_len;
} dir_data_t;

int ICACHE_FLASH_ATTR cgi_list(HttpdConnData *connData) {

    char buff[512];
    char spaces[16];
    FILINFO f_info;
    FRESULT ret;
    size_t len, total_len;
    FATFS *fs;
    uint32_t fre_clust, fre_sect;

    dir_data_t *dir_data = connData->cgiData;

    if (connData->requestType != HTTPD_METHOD_POST) {
        return HTTPD_CGI_NOTFOUND;
    }

    if (connData->conn==NULL) {
        //Connection aborted. Clean up.
        f_closedir(&(dir_data->dir));
        free(dir_data);
        return HTTPD_CGI_DONE;
    }

    if (dir_data == NULL) {
        //First call to this cgi. Open the file so we can read it.
        dir_data = malloc(sizeof(dir_data_t));
        if (dir_data == NULL) {
            return HTTPD_CGI_NOTFOUND;
        }
        ret = f_opendir(&(dir_data->dir), HTML_PATH);
        if (ret != FR_OK) {
            free(dir_data);
            return HTTPD_CGI_NOTFOUND;
        }
        dir_data->gl_len = 0;
        connData->cgiData=dir_data;
        httpdStartResponse(connData, 200);
        httpdHeader(connData, "Content-Type", httpdGetMimetype(connData->url));
        httpdEndHeaders(connData);
        sprintf(buff, "Directory: %s\n\n", HTML_PATH);
        httpdSend(connData, buff, strlen(buff));
        return HTTPD_CGI_MORE;
    }

    if (f_readdir(&(dir_data->dir), &f_info) == FR_OK && f_info.fname[0]) {
        sprintf(buff, "%ld", f_info.fsize);
        len = 7 - strlen(buff);
        memset(spaces, ' ', len);
        spaces[len] = 0;
        os_sprintf(buff, "  %s%ld    %s\n", spaces, f_info.fsize, f_info.fname);
        dir_data->gl_len += f_info.fsize;
        httpdSend(connData, buff, strlen(buff));
        return HTTPD_CGI_MORE;
    }

    f_closedir(&(dir_data->dir));
    free(dir_data);

    f_getfree("", &fre_clust, &fs);

    /* Get total sectors and free sectors */
    fre_sect = fre_clust * fs->csize;

    os_sprintf(buff, "\nUsed %d\tbytes\nFree %d\tbytes\n", dir_data->gl_len, fre_sect / 2);
    httpdSend(connData, buff, strlen(buff));

    return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR response_with_tpl(HttpdConnData *connData) {
    char buff[RW_BUFF_LEN + 1];
    size_t len = 0, x, sp = 0;
    char *e=NULL;
    FRESULT ret;

    html_data_t *html_data = connData->cgiData;

    ret = f_read(&(html_data->file), buff, RW_BUFF_LEN, &len);
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

    ret = f_read(&(html_data->file), buff, RW_BUFF_LEN, &len);

    os_printf("len: %d\n", len);

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

int ICACHE_FLASH_ATTR cgi_response(HttpdConnData *connData) {
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

#define PAGELEN 256

typedef struct {
	char page_data[PAGELEN];
	int page_pos;
	int address;
	int len;
} update_state_t;

static int ota_update(HttpdConnData *connData) {

	char *data;
    update_state_t *state = connData->cgiData;

    if (connData->conn==NULL) {
        //Connection aborted. Clean up.
        free(state);
        return HTTPD_CGI_DONE;
    }

	/* First call. Allocate and initialize state variable. */
    if (state == NULL) {
    	char *file_name_p; //, *file_name;
    	uint32_t ota_flash_map, ota_flash_size;
    	char *image_name, *str_ota_flash_map, *new, *str_ota_flash_size;
    	data = connData->post->buff;
    	char file_name[] = "user1.4096.new.6.bin";

    	image_name = strtok(file_name, ".");
    	if (image_name == NULL) {
    		//return err;
    	}

    	str_ota_flash_size = strtok(NULL, ".");
    	if (str_ota_flash_size == NULL) {
    		// return err;
    	}
    	ota_flash_size = atoi(str_ota_flash_size);

    	new = strtok(NULL, ".");
    	if (new == NULL) {
    		// return err;
    	}

    	str_ota_flash_map = strtok(NULL, ".");
    	if (str_ota_flash_map == NULL) {
    		//return err;
    	}
    	ota_flash_map = atoi(str_ota_flash_map);


    }

    return HTTPD_CGI_DONE;
}


int ICACHE_FLASH_ATTR cgi_upload(HttpdConnData *connData) {
    const char *full_path;
    char *err = NULL;

//    if (connData->conn==NULL) {
//        //Connection aborted. Clean up.
//        return HTTPD_CGI_DONE;
//    }

    full_path = connData->url+strlen(PATH_UPLOAD)-1;

    if (strncmp(full_path, PATH_HTML, strlen(PATH_HTML)) == 0) {
        if (strlen(full_path+strlen(PATH_HTML)) >= FF_MAX_LFN) {
            err = "Filename too long";
            os_printf("%s. (%s:%u)", err, __FILE__, __LINE__);
            return HTTPD_CGI_NOTFOUND;
        }

        os_printf("len: %d\n", connData->post->len);
        os_printf("buffLen: %d\n", connData->post->buffLen);
        os_printf("buffSize: %d\n", connData->post->buffSize);

//        return webserver_upload_html(req, full_path);

    } else if (strncmp(full_path, PATH_IMAGE, strlen(PATH_IMAGE)) == 0) {
        if (strlen(full_path+strlen(PATH_IMAGE)) >= FF_MAX_LFN) {
            err = "Filename too long";
            os_printf("%s. (%s:%u)", err, __FILE__, __LINE__);
            return HTTPD_CGI_NOTFOUND;
        }

        return ota_update(connData);

    } else {
        err = "Invalid path";
        os_printf("%s: %s. (%s:%u)", err, connData->url, __FILE__, __LINE__);
        return HTTPD_CGI_NOTFOUND;
    }

    return HTTPD_CGI_MORE;
}

