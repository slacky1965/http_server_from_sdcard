#include "osapi.h"
#include "user_interface.h"
#include "mem.h"
#include "upgrade.h"
#include "json/jsonparse.h"

#include "user_config.h"
#include "appdef.h"
#include "sdcard.h"
#include "utils.h"
#include "wifi.h"
#include "platform.h"
#include "httpd.h"
#include "http.h"
#include "fs.h"

#define RW_BUFF_LEN 1024
#define PATH_HTML   "/html/"
#define PATH_IMAGE  "/image/"
#define PATH_UPLOAD "/upload/"

static os_timer_t resetTimer;

typedef void (* TplCallback)(HttpdConnData *connData, char *token, void **arg);

HttpdBuiltInUrl builtInUrls[] = {
        {"/", cgiRedirect, "/index.html"},
        {"/get_ota_filename", cgi_get_user_ota_filename, NULL},
        {"/upload/*", cgi_upload, NULL},
        {"/list", cgi_list, NULL},
        {"/delete", cgi_delete, NULL},
        {"*", cgi_response, NULL},
        {NULL, NULL, NULL}
};

static char *user_ota_file[] = {
        "",                         /* SPI_FLASH_SIZE_MAP 0   512KB  (256KB+ 256KB)    0x41000  */
        "",
        "userX.1024.new.2.bin",     /* SPI_FLASH_SIZE_MAP 2   1024KB (512KB+ 512KB)    0x81000  */
        "userX.2048.new.3.bin",     /* SPI_FLASH_SIZE_MAP 3   2048KB (512KB+ 512KB)    0x81000  */
        "userX.4096.new.4.bin",     /* SPI_FLASH_SIZE_MAP 4   4096KB (512KB+ 512KB)    0x81000  */
        "userX.2048.new.5.bin",     /* SPI_FLASH_SIZE_MAP 5   2048KB (1024KB+1024KB)   0x101000 */
        "userX.4096.new.6.bin",     /* SPI_FLASH_SIZE_MAP 6   4096KB (1024KB+1024KB)   0x101000 */
        "",
        "userX.8192.new.8.bin",     /* SPI_FLASH_SIZE_MAP 8   8192KB (1024KB+1024KB)   0x101000 */
        "userX.16384.new.9.bin"     /* SPI_FLASH_SIZE_MAP 9   16384KB(1024KB+1024KB)   0x101000 */
};

typedef struct {
    FILE  *file;
    char  *tmp_name;
    char  *name;
    void  *tpl_arg;
    char   token[64];
    size_t token_pos;
} html_data_t;

typedef struct {
    DIR    *dir;
    size_t gl_len;
    int count_files;
} dir_data_t;

int ICACHE_FLASH_ATTR cgi_list(HttpdConnData *connData) {

    char buff[512];
    char *err = NULL;
    struct dirent *de;

    dir_data_t *dir_data = connData->cgiData;

    if (connData->conn==NULL) {
        //Connection aborted. Clean up.
        closedir(dir_data->dir);
        os_free(dir_data);
        return HTTPD_CGI_DONE;
    }

    if (dir_data == NULL) {
        //First call to this cgi. Open the file so we can read it.

        if (connData->requestType != HTTPD_METHOD_POST) {
            err = "Method not allowed";
            httpd_printf("%s. (%s:%u)\n", err, __FILE__, __LINE__);
            httpdSendErr(connData, HTTPD_405_METHOD_NOT_ALLOWED, err);
            return HTTPD_CGI_DONE;
        }

        dir_data = os_malloc(sizeof(dir_data_t));
        if (dir_data == NULL) {
            err = "Error allocation memory";
            httpd_printf("%s. (%s:%u)\n", err, __FILE__, __LINE__);
            httpdSendErr(connData, HTTPD_500_INTERNAL_SERVER_ERROR, err);
            return HTTPD_CGI_DONE;
        }

        dir_data->dir = opendir(HTML_PATH);
        if (dir_data->dir == NULL) {
            err = "Failed to open dir \"", HTML_PATH, "\"";
            httpd_printf("%s. (%s:%u)\n", err, __FILE__, __LINE__);
            httpdSendErr(connData, HTTPD_500_INTERNAL_SERVER_ERROR, err);
            os_free(dir_data);
            return HTTPD_CGI_DONE;
        }

        dir_data->gl_len = 0;
        dir_data->count_files = 0;
        connData->cgiData=dir_data;
        httpdStartResponse(connData, 200);
        httpdHeader(connData, "Content-Type", /*"text/plain"*/ httpdGetMimetype(connData->url));
        httpdEndHeaders(connData);
        sprintf(buff, "\nDirectory: %s\n\n", HTML_PATH);
        httpdSend(connData, buff, strlen(buff));
        return HTTPD_CGI_MORE;
    }


    if ((de = readdir(dir_data->dir))) {
        os_sprintf(buff, "<input type=\"checkbox\" name=\"file%d\" value=\"%s\"> %11u    %s\n",
                   dir_data->count_files++, de->d_name, de->finfo.fsize, de->d_name);
        dir_data->gl_len += de->finfo.fsize;
        httpdSend(connData, buff, strlen(buff));
        return HTTPD_CGI_MORE;
    }

    closedir(dir_data->dir);
    os_free(dir_data);

    uint32_t free_size, total_size;

    free_size = get_sd_free_space(&total_size);

    os_sprintf(buff, "\nUsed %9u    kBytes\nFree %9u    kBytes\n", total_size-free_size, free_size);
    httpdSend(connData, buff, strlen(buff));

    return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR httpd_html_delete(const char *file_name) {
    char buff[FF_MAX_LFN+32];
    os_sprintf(buff, "%s/%s", HTML_PATH, file_name);
    return unlink(buff);
}

int ICACHE_FLASH_ATTR cgi_delete(HttpdConnData *connData) {

    struct jsonparse_state parser;
    char buff[FF_MAX_LFN+1] = {0}, *err = NULL;
    bool json_key = false;
    int type;

    if (connData->conn==NULL) {
        //Connection aborted. Clean up.
        return HTTPD_CGI_DONE;
    }

    if (connData->requestType != HTTPD_METHOD_POST) {
        err = "Method not allowed";
        httpd_printf("%s. (%s:%u)\n", err, __FILE__, __LINE__);
        httpdSendErr(connData, HTTPD_405_METHOD_NOT_ALLOWED, err);
        return HTTPD_CGI_DONE;
    }

    jsonparse_setup(&parser, connData->post->buff, connData->post->buffLen);

    while((type = jsonparse_next(&parser)) != 0) {
        switch (type) {
            case JSON_TYPE_PAIR_NAME:
                if (jsonparse_strcmp_value(&parser, JSON_DEL_FILES_ARRAY_NAME) == 0) {
                    json_key = true;
                }
                break;
            case JSON_TYPE_STRING:
                if (json_key) {
                    jsonparse_copy_value(&parser, buff, sizeof(buff));
                    if (httpd_html_delete(buff) != 0) {
                        err = "File to delete fail";
                        httpd_printf("%s (%s). (%s:%u)\n", err, buff, __FILE__, __LINE__);
                        httpdSendErr(connData, HTTPD_500_INTERNAL_SERVER_ERROR, err);
                        return HTTPD_CGI_DONE;
                    }
                }
                break;
            default:
                break;
        }
    }

    httpdStartResponse(connData, 200);
    httpdHeader(connData, "Content-Type", httpdGetMimetype(connData->url));
    httpdEndHeaders(connData);
    httpdSend(connData, "Delete files successful", -1);
    return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR httpd_response_with_tpl(HttpdConnData *connData) {
    char buff[RW_BUFF_LEN + 1];
    size_t len = 0, x, sp = 0;
    char *e=NULL, *err = NULL;

    html_data_t *html_data = connData->cgiData;

    len = fread(buff, sizeof(uint8_t), RW_BUFF_LEN, html_data->file);

    if (len == 0) {
        err = "Failed to read file";
        httpd_printf("%s '%s' from SD card. (%s:%u)\n", err, connData->url+1, __FILE__, __LINE__);
        httpdSendErr(connData, HTTPD_500_INTERNAL_SERVER_ERROR, err);
        return HTTPD_CGI_DONE;
    }

    sp = 0;
    e = buff;
    for (x = 0; x < len; x++) {
        if (html_data->token_pos == -1) {
            //Inside ordinary text.
            if (buff[x] == '%') {
                //Send raw data up to now
                if (sp != 0)
                    httpdSend(connData, e, sp);
                sp = 0;
                //Go collect token chars.
                html_data->token_pos = 0;
            } else {
                sp++;
            }
        } else {
            if (buff[x] == '%') {
                if (html_data->token_pos == 0) {
                    //This is the second % of a %% escape string.
                    //Send a single % and resume with the normal program flow.
                    httpdSend(connData, "%", 1);
                } else {
                    //This is an actual token.
                    html_data->token[html_data->token_pos++] = 0; //zero-terminate token
                    ((TplCallback) (connData->cgiArg))(connData, html_data->token, &html_data->tpl_arg);
                }
                //Go collect normal chars again.
                e = &buff[x + 1];
                html_data->token_pos = -1;
            } else {
                if (html_data->token_pos < (sizeof(html_data->token) - 1))
                    html_data->token[html_data->token_pos++] = buff[x];
            }
        }
    }
    //Send remaining bit.
    if (sp!=0) httpdSend(connData, e, sp);
    if (len != RW_BUFF_LEN) {
        //We're done.
        ((TplCallback)(connData->cgiArg))(connData, NULL, &html_data->tpl_arg);
        fclose(html_data->file);
        os_free(html_data);
        return HTTPD_CGI_DONE;
    } else {
        //Ok, till next time.
        return HTTPD_CGI_MORE;
    }

}

int ICACHE_FLASH_ATTR httpd_response_without_tpl(HttpdConnData *connData) {
    char buff[RW_BUFF_LEN], *err = NULL;
    size_t len = 0;

    html_data_t *html_data = connData->cgiData;

    len = fread(buff, sizeof(uint8_t), RW_BUFF_LEN, html_data->file);

    if (len == 0) {
        err = "Failed to read file";
        httpd_printf("%s '%s' from SD card. (%s:%u)\n", err, connData->url+1, __FILE__, __LINE__);
        httpdSendErr(connData, HTTPD_500_INTERNAL_SERVER_ERROR, err);
        return HTTPD_CGI_DONE;
    }

    httpdSend(connData, buff, len);

    if (len == RW_BUFF_LEN) {
        return HTTPD_CGI_MORE;
    } else {
        fclose(html_data->file);
        os_free(html_data);
        return HTTPD_CGI_DONE;
    }

    fclose(html_data->file);
    os_free(html_data);
    return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR cgi_response(HttpdConnData *connData) {
    char buff[256];
    char *err = NULL;

    html_data_t *html_data = connData->cgiData;

    if (connData->conn==NULL) {
        //Connection aborted. Clean up.
        fclose(html_data->file);
        os_free(html_data);
        return HTTPD_CGI_DONE;
    }

    if (html_data == NULL) {
        //First call to this cgi. Open the file so we can read it.
        html_data = os_malloc(sizeof(html_data_t));
        if (html_data == NULL) {
            err = "Error allocation memory";
            httpd_printf("%s. (%s:%u)\n", err, __FILE__, __LINE__);
            httpdSendErr(connData, HTTPD_500_INTERNAL_SERVER_ERROR, err);
            return HTTPD_CGI_DONE;
        }
        os_sprintf(buff, "%s%s", HTML_PATH, connData->url);
        html_data->tpl_arg=NULL;
        html_data->token_pos = -1;
        html_data->file = fopen(buff, "r");
        if (html_data->file == NULL) {
            os_free(html_data);
            err = "File not found";
            httpd_printf("%s: '%s'. (%s:%u)\n", err, connData->url+1, __FILE__, __LINE__);
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
	char buff[128];
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

static int ICACHE_FLASH_ATTR httpd_ota_update(HttpdConnData *connData) {

    char *err = NULL;
    char buff[256], last_buff[4] = {0};
    char *name;

    size_t *address = (size_t*)connData->cgiData;

    if (connData->conn==NULL) {
        //Connection aborted. Clean up.
        if (address != NULL) os_free(address);
        return HTTPD_CGI_DONE;
    }

    char *data=connData->post->buff;
    int data_len=connData->post->buffLen;

	/* First call. Allocate and initialize address variable. */
    if (address == NULL) {
        httpd_printf("Firmware upload start.\n");

        name = strrchr (connData->url, '/');
        if (name) {
            name++;
            if (strlen(name) == 0) {
                err = "Empty uploading file name";
                httpd_printf("%s. (%s:%u)\n", err, __FILE__, __LINE__);
                httpdSendErr(connData, HTTPD_400_BAD_REQUEST, err);
                return HTTPD_CGI_DONE;
            }
        }

        if (connData->post->len > FIRMWARE_SIZE) {
            err = "Firmware image too large";
            httpd_printf("%s. (%s:%u)\n", err, __FILE__, __LINE__);
            httpdSendErr(connData, HTTPD_400_BAD_REQUEST, err);
            return HTTPD_CGI_DONE;
        }

        address = os_malloc(sizeof(size_t));
        if (address == NULL) {
            err = "Error allocation memory";
            httpd_printf("%s. (%s:%u)\n", err, __FILE__, __LINE__);
            httpdSendErr(connData, HTTPD_500_INTERNAL_SERVER_ERROR, err);
            return HTTPD_CGI_DONE;
        }

        *address = 0;
        connData->cgiData = address;

        if (system_upgrade_userbin_check() == 1) {
            /* file name must be user1 */
            user_ota_file[SPI_FLASH_SIZE_MAP][4] = '1';
            httpd_printf("Flashing %s\n", user_ota_file[SPI_FLASH_SIZE_MAP]);
            *address = 0x1000;
        } else {
            /* file name must be user2 */
            user_ota_file[SPI_FLASH_SIZE_MAP][4] = '2';
            httpd_printf("Flashing %s\n", user_ota_file[SPI_FLASH_SIZE_MAP]);
            *address = FIRMWARE_POS;
        }

        if (strcasecmp(name, user_ota_file[SPI_FLASH_SIZE_MAP]) != 0) {
            err = "Invalid file name!";
            httpd_printf("%s. (%s:%u)\n", err, __FILE__, __LINE__);
            httpdSendErr(connData, HTTPD_400_BAD_REQUEST, err);
            os_free(address);
            return HTTPD_CGI_DONE;
        }

        if (!check_bin_header(data, *address)) {
            err = "Invalid flash image type!";
            httpd_printf("%s. (%s:%u)\n", err, __FILE__, __LINE__);
            httpdSendErr(connData, HTTPD_400_BAD_REQUEST, err);
            os_free(address);
            return HTTPD_CGI_DONE;
        }

        int countSector = connData->post->len / SPI_FLASH_SEC_SIZE;
        countSector += connData->post->len % SPI_FLASH_SEC_SIZE ? 1 : 0;

        int flashSector = *address / SPI_FLASH_SEC_SIZE;

        ets_intr_lock();

        for (int i = 0; i < countSector; i++) {
            spi_flash_erase_sector(flashSector);
            flashSector++;
        }

        ets_intr_unlock();


    }

    /* HTTPD_MAX_POST_LEN must be a multiple of four is always!!! */
    if (connData->post->buffLen < connData->post->buffSize) {
        data_len = (connData->post->buffLen / 4) * 4;

        if (connData->post->buffLen % 4) {
            os_memcpy(last_buff, connData->post->buff+data_len, connData->post->buffLen-data_len);
        }
    }

    ets_intr_lock();

    spi_flash_write(*address, (uint32*)data, data_len);

    *address += data_len;

    if (last_buff[0] != 0) {
        spi_flash_write(*address, (uint32*)last_buff, sizeof(last_buff));
        *address += sizeof(last_buff);
    }

    ets_intr_unlock();


    if (connData->post->len == connData->post->received) {
        httpd_printf("Upload done. Sending response.\n");
        httpdStartResponse(connData, 200);
        httpdHeader(connData, "Content-Type", "text/plain");
        httpdEndHeaders(connData);
        name = strrchr (connData->url, '/');
        if (name) name++;
        os_sprintf(buff, "File `%s` %d bytes uploaded successfully.\nRestart system...\n", name, connData->post->received);
        httpdSend(connData, buff, strlen(buff));
        os_free(address);

        os_timer_disarm(&resetTimer);
        os_timer_setfn(&resetTimer, resetTimerCb, NULL);
        os_timer_arm(&resetTimer, 1000, 0);

        httpd_printf("Rebooting...\n");

        return HTTPD_CGI_DONE;
    }

    return HTTPD_CGI_MORE;
}

static int ICACHE_FLASH_ATTR httpd_html_upload(HttpdConnData *connData, const char *full_name) {
    STAT finfo;
    char *name, buff[256], *err = NULL;

    html_data_t *html_data = connData->cgiData;

    if (connData->conn==NULL) {
        //Connection aborted. Clean up.
        fclose(html_data->file);
        os_free(html_data->name);
        os_free(html_data->tmp_name);
        os_free(html_data);
        return HTTPD_CGI_DONE;
    }

    //First call to this cgi
    if (html_data == NULL) {
        name = strrchr (full_name, '/');
        if (name) {
            name++;
            if (strlen(name) == 0) {
                err = "Empty uploading file name";
                httpd_printf("%s. (%s:%u)\n", err, __FILE__, __LINE__);
                httpdSendErr(connData, HTTPD_400_BAD_REQUEST, err);
                return HTTPD_CGI_DONE;
            }
        }

        // Disk is full?
        uint64_t free_size = get_sd_free_space(NULL) * 1024;
        if (free_size < connData->post->len) {
            err = "Upload file too large";
            httpd_printf("%s. (%s:%u)\n", err, __FILE__, __LINE__);
            httpdSendErr(connData, HTTPD_400_BAD_REQUEST, err);
            return HTTPD_CGI_DONE;
        }

        if (connData->post->buffLen == 0) {
            err = "File empty";
            httpd_printf("%s. (%s:%u)\n", err, __FILE__, __LINE__);
            httpdSendErr(connData, HTTPD_400_BAD_REQUEST, err);
            return HTTPD_CGI_DONE;
        }


        html_data = os_malloc(sizeof(html_data_t));
        if (html_data == NULL) {
            err = "Error allocation memory";
            httpd_printf("%s. (%s:%u)\n", err, __FILE__, __LINE__);
            httpdSendErr(connData, HTTPD_500_INTERNAL_SERVER_ERROR, err);
            return HTTPD_CGI_DONE;
        }

        html_data->name = os_malloc(strlen(full_name)+1);
        if (html_data->name == NULL) {
            os_free(html_data);
            err = "Error allocation memory";
            httpd_printf("%s. (%s:%u)\n", err, __FILE__, __LINE__);
            httpdSendErr(connData, HTTPD_500_INTERNAL_SERVER_ERROR, err);
            return HTTPD_CGI_DONE;
        }
        strcpy(html_data->name, full_name);

        html_data->tmp_name = os_malloc(strlen(full_name)+5);
        if (html_data->tmp_name == NULL) {
            os_free(html_data->name);
            os_free(html_data);
            err = "Error allocation memory";
            httpd_printf("%s. (%s:%u)\n", err, __FILE__, __LINE__);
            httpdSendErr(connData, HTTPD_500_INTERNAL_SERVER_ERROR, err);
            return HTTPD_CGI_DONE;
        }
        sprintf(html_data->tmp_name, "%s.tmp", html_data->name);
        html_data->file = fopen(html_data->tmp_name, "w");
        if (html_data->file == NULL) {
            os_free(html_data->name);
            os_free(html_data->tmp_name);
            os_free(html_data);
            err = "Failed to create file";
            httpd_printf("%s \"%s\" (%s:%u)\n", err, html_data->tmp_name, __FILE__, __LINE__);
            httpdSendErr(connData, HTTPD_500_INTERNAL_SERVER_ERROR, err);
            return HTTPD_CGI_DONE;
        }
        connData->cgiData=html_data;
    }

    size_t len = fwrite(connData->post->buff, sizeof(uint8_t), connData->post->buffLen, html_data->file);

    if (len == 0) {
        fclose(html_data->file);
        os_free(html_data->name);
        os_free(html_data->tmp_name);
        os_free(html_data);
        err = "Failed to write file to SD card";
        httpd_printf("%s. (%s:%u)\n", err, __FILE__, __LINE__);
        /* Respond with 500 Internal Server Error */
        httpdSendErr(connData, HTTPD_500_INTERNAL_SERVER_ERROR, err);
        return HTTPD_CGI_DONE;
    }

    if (connData->post->len == connData->post->received) {
        fclose(html_data->file);

        httpd_printf("File transferred finished: %d bytes\n", connData->post->len);

        if (stat(html_data->name, &finfo) == 0) {
            unlink(html_data->name);
        }

        if (rename(html_data->tmp_name, html_data->name) != 0) {
            httpd_printf("File rename \"%s\" to \"%s\" failed. (%s:%u)\n",
                          html_data->tmp_name, html_data->name, __FILE__, __LINE__);
            httpdSendErr(connData, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to rename file");
            os_free(html_data->name);
            os_free(html_data->tmp_name);
            os_free(html_data);
            return HTTPD_CGI_DONE;
        }

        httpdStartResponse(connData, 200);
        httpdHeader(connData, "Content-Type", "text/plain");
        httpdEndHeaders(connData);

        name = strrchr(connData->url, '/');
        if (name)
            name++;
        os_sprintf(buff, "File `%s` %d bytes uploaded successfully.",
                name ? name : full_name, connData->post->len);
        httpdSend(connData, buff, strlen(buff));

        os_free(html_data->name);
        os_free(html_data->tmp_name);
        os_free(html_data);
        return HTTPD_CGI_DONE;

    }

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
        httpd_printf("%s. (%s:%u)\n", err, __FILE__, __LINE__);
        httpdSendErr(connData, HTTPD_405_METHOD_NOT_ALLOWED, err);
        return HTTPD_CGI_DONE;
    }


    full_path = connData->url+strlen(PATH_UPLOAD)-1;

    if (strncmp(full_path, PATH_HTML, strlen(PATH_HTML)) == 0) {

        if (strlen(full_path+strlen(PATH_HTML)) >= FF_MAX_LFN) {
            err = "Filename too long";
            httpd_printf("%s. (%s:%u)\n", err, __FILE__, __LINE__);
            httpdSendErr(connData, HTTPD_400_BAD_REQUEST, err);
            return HTTPD_CGI_DONE;
        }

        return httpd_html_upload(connData, full_path);

    } else if (strncmp(full_path, PATH_IMAGE, strlen(PATH_IMAGE)) == 0) {
        if (strlen(full_path+strlen(PATH_IMAGE)) >= FF_MAX_LFN) {
            err = "Filename too long";
            httpd_printf("%s. (%s:%u)", err, __FILE__, __LINE__);
            return HTTPD_CGI_NOTFOUND;
        }

        return httpd_ota_update(connData);

    } else {
        err = "Invalid path";
        httpd_printf("%s: %s. (%s:%u)", err, connData->url, __FILE__, __LINE__);
        return HTTPD_CGI_NOTFOUND;
    }

    return HTTPD_CGI_MORE;
}

int ICACHE_FLASH_ATTR cgi_get_user_ota_filename(HttpdConnData *connData) {

    char msg[32];

    if (connData->conn==NULL) {
        //Connection aborted. Clean up.
        return HTTPD_CGI_DONE;
    }

    if (system_upgrade_userbin_check() == 1) {
        user_ota_file[SPI_FLASH_SIZE_MAP][4] = '1';
    } else {
        user_ota_file[SPI_FLASH_SIZE_MAP][4] = '2';
    }

    httpdStartResponse(connData, 200);
    httpdHeader(connData, "Content-Type", "application/json");
    httpdEndHeaders(connData);
    os_sprintf(msg, "{\"ota_filename\": \"%s\"}", user_ota_file[SPI_FLASH_SIZE_MAP]);
    httpdSend(connData, msg, strlen(msg));

    return HTTPD_CGI_DONE;
}
