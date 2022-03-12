#include <stdlib.h>
#include "osapi.h"
#include "user_interface.h"
#include "mem.h"
#include "upgrade.h"

#include "user_config.h"
#include "appdef.h"
#include "sdcard.h"
#include "utils.h"
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

static os_timer_t resetTimer;

//CgiUploadFlashDef upload_params={
//	.type=CGIFLASH_TYPE_FW,
//	.fw1Pos=0x1000,
//	.fw2Pos=FIRMWARE_POS,
//	.fwSize=FIRMWARE_SIZE,
//	.tagName=OTA_TAGNAME
//};


int ICACHE_FLASH_ATTR tpl_token(HttpdConnData *connData, char *token, void **arg);
typedef void (* TplCallback)(HttpdConnData *connData, char *token, void **arg);

HttpdBuiltInUrl builtInUrls[] = {
        {"/", cgiRedirect, "/index.html"},
//        {"/upload/*", cgi_upload, &upload_params},
        {"/upload/*", cgi_upload, NULL},
        {"/list", cgi_list, NULL},
//		{"/scripts.js", cgi_response, NULL},
        {"*", cgi_response, NULL},
        {NULL, NULL, NULL}
};

typedef struct {
    FIL    file;
    char  *tmp_name;
    char  *name;
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

int ICACHE_FLASH_ATTR httpd_response_with_tpl(HttpdConnData *connData) {
    char buff[RW_BUFF_LEN + 1];
    size_t len = 0, x, sp = 0;
    char *e=NULL, *err = NULL;
    FRESULT ret;

    html_data_t *html_data = connData->cgiData;

    ret = f_read(&(html_data->file), buff, RW_BUFF_LEN, &len);

    if (ret != FR_OK) {
        err = "Failed to read file";
        os_printf("%s '%s' from SD card. (%s:%u)\n", err, connData->url+1, __FILE__, __LINE__);
        httpdSendErr(connData, HTTPD_500_INTERNAL_SERVER_ERROR, err);
        return HTTPD_CGI_DONE;
    }

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

int ICACHE_FLASH_ATTR httpd_response_without_tpl(HttpdConnData *connData) {
    char buff[RW_BUFF_LEN], *err = NULL;
    size_t len = 0;
    FRESULT ret;

    html_data_t *html_data = connData->cgiData;

    ret = f_read(&(html_data->file), buff, RW_BUFF_LEN, &len);

    if (ret != FR_OK) {
        err = "Failed to read file";
        os_printf("%s '%s' from SD card. (%s:%u)\n", err, connData->url+1, __FILE__, __LINE__);
        httpdSendErr(connData, HTTPD_500_INTERNAL_SERVER_ERROR, err);
        return HTTPD_CGI_DONE;
    }

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
    char *err = NULL;

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
            err = "Error allocation memory";
            os_printf("%s. (%s:%u)\n", err, __FILE__, __LINE__);
            httpdSendErr(connData, HTTPD_500_INTERNAL_SERVER_ERROR, err);
            return HTTPD_CGI_DONE;
        }
        os_sprintf(buff, "%s%s", HTML_PATH, connData->url);
        html_data->tpl_arg=NULL;
        html_data->token_pos = -1;
        ret = f_open(&(html_data->file), buff, FA_READ);
        if (ret != FR_OK) {
            free(html_data);
            err = "File not found";
            os_printf("%s: '%s'. (%s:%u)\n", err, connData->url+1, __FILE__, __LINE__);
            httpdSendErr(connData, HTTPD_404_NOT_FOUND, NULL);
            return HTTPD_CGI_DONE;
        }
        connData->cgiData=html_data;
        httpdStartResponse(connData, 200);
        httpdHeader(connData, "Content-Type", httpdGetMimetype(connData->url));
        httpdEndHeaders(connData);
        return HTTPD_CGI_MORE;
    }

    if (connData->cgiArg == NULL) {
    	return httpd_response_without_tpl(connData);
    }

    return httpd_response_with_tpl(connData);
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

static void ICACHE_FLASH_ATTR resetTimerCb(void *arg) {
    os_delay_us(65535);
    system_upgrade_flag_set(UPGRADE_FLAG_FINISH);
    system_upgrade_reboot();
}

// Check that the header of the firmware blob looks like actual firmware...
LOCAL int ICACHE_FLASH_ATTR checkBinHeader(void *buf) {
    uint8_t *cd = (uint8_t *) buf;
    if (cd[0] != 0xEA)
        return 0;
    if (cd[1] != 4 || cd[2] > 3 || cd[3] > 0x40)
        return 0;
    if (((uint16_t *) buf)[3] != 0x4010)
        return 0;
    if (((uint32_t *) buf)[2] != 0)
        return 0;
    return 1;
}

static bool ICACHE_FLASH_ATTR check_bin_header(void *buf, uint32_t address) {
    uint8_t *cd = (uint8_t *) buf;
    if (cd[0] != 0xEA) return false;
    if (cd[1] != 4 || cd[2] > 3) return false;
    if (((uint16_t *) buf)[3] != 0x4010) return false;
    if (((uint32_t *) buf)[2] != 0) return false;
    if (address == 0x1000) {
        // image file must be user1.bin
        if (cd[3] != 1) return false;
    } else {
        // image file must be user2.bin
        if (cd[3] != 2) return false;
    }
    return true;
}

#define PAGELEN 256
//#define FILETYPE_ESPFS 0
//#define FILETYPE_FLASH 1
//#define FILETYPE_OTA 2

typedef struct {
	char page_data[PAGELEN];
	int page_pos;
	int address;
	int len;
} update_state_t;

//typedef struct __attribute__((packed)) {
//	char magic[4];
//	char tag[28];
//	int32_t len1;
//	int32_t len2;
//} ota_header;

static int httpd_ota_update(HttpdConnData *connData) {

    char *err = NULL;
    bool lastBuff = false;

    update_state_t *state = (update_state_t*)connData->cgiData;
    CgiUploadFlashDef *def = (CgiUploadFlashDef*)connData->cgiArg;

    if (connData->conn==NULL) {
        //Connection aborted. Clean up.
        if (state != NULL) free(state);
        return HTTPD_CGI_DONE;
    }

    char *data=connData->post->buff;
    int data_len=connData->post->buffLen;

	/* First call. Allocate and initialize state variable. */
    if (state == NULL) {
        os_printf("Firmware upload start.\n");

        if (connData->post->len > FIRMWARE_SIZE) {
            err = "Firmware image too large";
        }

        if (err) {
            os_printf("%s. (%s:%u)\n", err, __FILE__, __LINE__);
            httpdSendErr(connData, HTTPD_400_BAD_REQUEST, err);
            return HTTPD_CGI_DONE;
        }

        state = os_malloc(sizeof(update_state_t));
        if (state == NULL) {
            err = "Error allocation memory";
            os_printf("%s. (%s:%u)\n", err, __FILE__, __LINE__);
            httpdSendErr(connData, HTTPD_500_INTERNAL_SERVER_ERROR, err);
            return HTTPD_CGI_DONE;
        }

        memset(state, 0, sizeof(update_state_t));
        connData->cgiData = state;

        if (system_upgrade_userbin_check() == 1) {
            os_printf("Flashing user1.bin\n");
            state->address = 0x1000;
        } else {
            os_printf("Flashing user2.bin\n");
            state->address = FIRMWARE_POS;
        }

        if (!check_bin_header(data, state->address)) {
            err = "Invalid flash image type!";
            os_printf("%s. (%s:%u)\n", err, __FILE__, __LINE__);
            httpdSendErr(connData, HTTPD_400_BAD_REQUEST, err);
            return HTTPD_CGI_DONE;
        }

//        if (!checkBinHeader(data)) {
//            err = "Invalid flash image type!";
//            os_printf("%s. (%s:%u)\n", err, __FILE__, __LINE__);
//            httpdSendErr(connData, HTTPD_400_BAD_REQUEST, err);
//            return HTTPD_CGI_DONE;
//        }


        int countSector = connData->post->len / SPI_FLASH_SEC_SIZE;
        countSector += connData->post->len % SPI_FLASH_SEC_SIZE ? 1 : 0;

        int flashSector = state->address / SPI_FLASH_SEC_SIZE;

        ets_intr_lock();

        for (int i = 0; i < countSector; i++) {
            spi_flash_erase_sector(flashSector);
            flashSector++;
        }

        ets_intr_unlock();


    }


    while (data_len != 0) {

        bool write = false;
        if (state->page_pos == 0) {
            if (data_len >= PAGELEN) {
                os_memcpy(state->page_data, data, PAGELEN);
                write = true;
                state->len = PAGELEN;
                data += PAGELEN;
                data_len -= PAGELEN;
            } else {
                os_memcpy(state->page_data, data, data_len);
                state->page_pos = data_len;
                if (lastBuff) {
                    write = true;
                    state->len = data_len;
                }
                data_len = 0;
            }
        } else {
            int len = PAGELEN - state->page_pos;
            if (data_len >= len) {
                os_memcpy(&state->page_data[state->page_pos], data, len);
                write = true;
                state->len = PAGELEN;
                data += len;
                data_len -= len;
                state->page_pos = 0;
            } else {
                os_memcpy(&state->page_data[state->page_pos], data, len);
                state->page_pos += len;
                if (lastBuff) {
                    write = true;
                    state->len = len;
                }
            }
        }

        os_printf("address - 0x%x\n", state->address);

        if (write) {


            ets_intr_lock();

            spi_flash_write(state->address, (uint32 *)state->page_data, state->len);

            ets_intr_unlock();

            state->address += state->len;

        }
    }

    if (connData->post->len == connData->post->received) {
        //We're done! Format a response.
        os_printf("Upload done. Sending response.\n");
        httpdStartResponse(connData, 200);
        httpdHeader(connData, "Content-Type", "text/plain");
        httpdEndHeaders(connData);
        httpdSend(connData, "Upload done! Rebooting ...\n", -1);
        free(state);

//        os_timer_disarm(&resetTimer);
//        os_timer_setfn(&resetTimer, resetTimerCb, NULL);
//        os_timer_arm(&resetTimer, 1000, 0);

        os_printf("Rebooting...\n");

        return HTTPD_CGI_DONE;
    }

    return HTTPD_CGI_MORE;
}

static int ICACHE_FLASH_ATTR httpd_html_upload(HttpdConnData *connData, const char *full_name) {
    FRESULT ret;
    FILINFO finfo;
    char *name, buff[256], *err = NULL;

    html_data_t *html_data = connData->cgiData;

    if (connData->conn==NULL) {
        //Connection aborted. Clean up.
        f_close(&(html_data->file));
        free(html_data->name);
        free(html_data->tmp_name);
        free(html_data);
        return HTTPD_CGI_DONE;
    }

    //First call to this cgi
    if (html_data == NULL) {
        name = strrchr (full_name, '/');
        if (name) {
            name++;
            if (strlen(name) == 0) {
                err = "Empty uploading file name";
                os_printf("%s. (%s:%u)\n", err, __FILE__, __LINE__);
                httpdSendErr(connData, HTTPD_400_BAD_REQUEST, err);
                return HTTPD_CGI_DONE;
            }
        }

        // Disk is full?
        if (get_sd_free_space() < connData->post->len) {
            err = "Upload file too large";
            os_printf("%s. (%s:%u)\n", err, __FILE__, __LINE__);
            httpdSendErr(connData, HTTPD_400_BAD_REQUEST, err);
            return HTTPD_CGI_DONE;
        }

        html_data = malloc(sizeof(html_data_t));
        if (html_data == NULL) {
            err = "Error allocation memory";
            os_printf("%s. (%s:%u)\n", err, __FILE__, __LINE__);
            httpdSendErr(connData, HTTPD_500_INTERNAL_SERVER_ERROR, err);
            return HTTPD_CGI_DONE;
        }

        html_data->name = malloc(strlen(full_name)+1);
        if (html_data->name == NULL) {
            free(html_data);
            err = "Error allocation memory";
            os_printf("%s. (%s:%u)\n", err, __FILE__, __LINE__);
            httpdSendErr(connData, HTTPD_500_INTERNAL_SERVER_ERROR, err);
            return HTTPD_CGI_DONE;
        }
        strcpy(html_data->name, full_name);

        html_data->tmp_name = malloc(strlen(full_name)+5);
        if (html_data->tmp_name == NULL) {
            free(html_data->name);
            free(html_data);
            err = "Error allocation memory";
            os_printf("%s. (%s:%u)\n", err, __FILE__, __LINE__);
            httpdSendErr(connData, HTTPD_500_INTERNAL_SERVER_ERROR, err);
            return HTTPD_CGI_DONE;
        }
        sprintf(html_data->tmp_name, "%s.tmp", html_data->name);
        ret = f_open(&(html_data->file), html_data->tmp_name, FA_WRITE|FA_CREATE_ALWAYS);
        if (ret != FR_OK) {
            free(html_data->name);
            free(html_data->tmp_name);
            free(html_data);
            err = "Failed to create file";
            os_printf("%s \"%s\" (%s:%u)\n", err, html_data->tmp_name, __FILE__, __LINE__);
            httpdSendErr(connData, HTTPD_500_INTERNAL_SERVER_ERROR, err);
            return HTTPD_CGI_DONE;
        }
        connData->cgiData=html_data;
    }

    size_t len = 0;
    ret = f_write(&(html_data->file), connData->post->buff, connData->post->buffLen, &len);

//    for (int i = 0; i < connData->post->buffLen; i++) {
//        os_printf("%c", connData->post->buff[i]);
//    }

//    if (ret != FR_OK) {
//        free(html_data->name);
//        free(html_data->tmp_name);
//        free(html_data);
//        err = "Failed to write file to SD card";
//        os_printf("%s. (%s:%u)\n", err, __FILE__, __LINE__);
//        /* Respond with 500 Internal Server Error */
//        httpdSendErr(connData, HTTPD_500_INTERNAL_SERVER_ERROR, err);
//        return HTTPD_CGI_DONE;
//    }

    if (len > 0) {
        if (connData->post->len == connData->post->received) {
            f_close(&(html_data->file));

            os_printf("File transferred finished: %d bytes\n", connData->post->len);

            ret = f_stat(html_data->name, &finfo);

            if (ret == FR_OK) {
                f_unlink(html_data->name);
            }

            if (f_rename(html_data->tmp_name, html_data->name) != FR_OK) {
                os_printf("File rename \"%s\" to \"%s\" failed. (%s:%u)\n", html_data->tmp_name,
                                                                            html_data->name,
                                                                            __FILE__, __LINE__);
                httpdSendErr(connData, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to rename file");
                free(html_data->name);
                free(html_data->tmp_name);
                free(html_data);
                return HTTPD_CGI_DONE;
            }


            httpdStartResponse(connData, 200);
            httpdHeader(connData, "Content-Type", "text/plain");
            httpdEndHeaders(connData);

            name = strrchr (full_name, '/');
            if (name) name++;
            os_sprintf(buff, "File `%s` %d bytes uploaded successfully.", name?name:full_name, connData->post->len);
            httpdSend(connData, buff, strlen(buff));

            free(html_data->name);
            free(html_data->tmp_name);
            free(html_data);
            return HTTPD_CGI_DONE;

        }
    }


//    if (len > 0) {
//        if (len == connData->post->buffSize) {
//            return HTTPD_CGI_MORE;
//        }
//    }
//
//    if (connData->post->len != connData->post->received) {
//        err = "File transfer error";
//        os_printf("%s. %d out of %d. (%s:%u)\n", err, connData->post->len, connData->post->received, __FILE__, __LINE__);
//        /* Respond with 500 Internal Server Error */
//        httpdSendErr(connData, HTTPD_500_INTERNAL_SERVER_ERROR, err);
//        return HTTPD_CGI_DONE;
//    }

    return HTTPD_CGI_MORE;
}

int ICACHE_FLASH_ATTR cgi_upload(HttpdConnData *connData) {
    const char *full_path;
    char *err = NULL;

    if (connData->conn==NULL) {
        //Connection aborted. Clean up.
        return HTTPD_CGI_DONE;
    }

    if (connData->requestType != HTTPD_METHOD_POST) {
        err = "Method not allowed";
        os_printf("%s. (%s:%u)\n", err, __FILE__, __LINE__);
        httpdSendErr(connData, HTTPD_405_METHOD_NOT_ALLOWED, err);
        return HTTPD_CGI_DONE;
    }


    full_path = connData->url+strlen(PATH_UPLOAD)-1;

    if (strncmp(full_path, PATH_HTML, strlen(PATH_HTML)) == 0) {

        if (strlen(full_path+strlen(PATH_HTML)) >= FF_MAX_LFN) {
            err = "Filename too long";
            os_printf("%s. (%s:%u)\n", err, __FILE__, __LINE__);
            httpdSendErr(connData, HTTPD_400_BAD_REQUEST, err);
            return HTTPD_CGI_DONE;
        }

        return httpd_html_upload(connData, full_path);

    } else if (strncmp(full_path, PATH_IMAGE, strlen(PATH_IMAGE)) == 0) {
        if (strlen(full_path+strlen(PATH_IMAGE)) >= FF_MAX_LFN) {
            err = "Filename too long";
            os_printf("%s. (%s:%u)", err, __FILE__, __LINE__);
            return HTTPD_CGI_NOTFOUND;
        }

        return httpd_ota_update(connData);

    } else {
        err = "Invalid path";
        os_printf("%s: %s. (%s:%u)", err, connData->url, __FILE__, __LINE__);
        return HTTPD_CGI_NOTFOUND;
    }

    return HTTPD_CGI_MORE;
}

