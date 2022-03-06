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

HttpdBuiltInUrl builtInUrls[]={
        {"/", cgiRedirect, "/index.html"},
        {"/upload/*", cgi_upload, &uploadParams},
        {"*", cgi_response, NULL},
        {NULL, NULL, NULL}
};

static int webserver_read_file(HttpdConnData *connData) {

    char *send_buff = NULL, *subst_token = NULL, token[32], buff[OTA_BUF_LEN + 1];
    size_t size;
    int i, pos_token, send_len = 0;

    sprintf(buff, "%s%s", webserver_html_path, req->uri);

    FILE *f = fopen(buff, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Cannot open file %s", buff);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not Found");
        return ESP_FAIL;
    }

    char *type = http_content_type(buff);
    httpd_resp_set_type(req, type);

    pos_token = -1;

    do {
        size = fread(buff, 1, sizeof(buff) - 1, f);
        if (size > 0) {
            send_len = 0;
            send_buff = buff;
            for (i = 0; i < size; i++) {
                if (pos_token == -1) {
                    if (buff[i] == '%') {
                        if (send_len != 0)
                            httpd_resp_send_chunk(req, send_buff, send_len);
                        send_len = 0;
                        pos_token = 0;
                    } else {
                        send_len++;
                    }
                } else {
                    if (buff[i] == '%') {
                        if (pos_token == 0) {
                            //This is the second % of a %% escape string.
                            //Send a single % and resume with the normal program flow.
                            httpd_resp_send_chunk(req, "%", 1);
                        } else {
                            //This is an actual token.
                            token[pos_token++] = 0; //zero-terminate token
                            // Call function check token
                            subst_token = webserver_subst_token_to_response(token);
                            if (strlen(subst_token)) {
                                httpd_resp_send_chunk(req, subst_token, strlen(subst_token));
                            }
                        }
                        //Go collect normal chars again.
                        send_buff = &buff[i + 1];
                        pos_token = -1;
                    } else {
                        if (pos_token < (sizeof(token) - 1))
                            token[pos_token++] = buff[i];
                    }
                }
            }
        }
        if (send_len != 0) {
            httpd_resp_send_chunk(req, send_buff, send_len);
        }
    } while (size == sizeof(buff) - 1);

    fclose(f);

    return ESP_OK;
}



int ICACHE_FLASH_ATTR cgi_upload (HttpdConnData *connData) {
    if (connData->conn==NULL) {
        //Connection aborted. Clean up.
        return HTTPD_CGI_DONE;
    }
    return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR cgi_response (HttpdConnData *connData) {
    if (connData->conn==NULL) {
        //Connection aborted. Clean up.
        return HTTPD_CGI_DONE;
    }

    os_printf("cgi_response\n");
    return HTTPD_CGI_DONE;
}

//int ICACHE_FLASH_ATTR cgiGreetUser(HttpdConnData *connData) {
//    int len;            //length of user name
//    char name[128];     //Temporary buffer for name
//    char output[256];   //Temporary buffer for HTML output
//
//    //If the browser unexpectedly closes the connection, the CGI will be called
//    //with connData->conn=NULL. We can use this to clean up any data. It's not really
//    //used in this simple CGI function.
//    if (connData->conn==NULL) {
//        //Connection aborted. Clean up.
//        return HTTPD_CGI_DONE;
//    }
//
//    if (connData->requestType!=HTTPD_METHOD_GET) {
//        //Sorry, we only accept GET requests.
//        httpdStartResponse(connData, 406);  //http error code 'unacceptable'
//        httpdEndHeaders(connData);
//        return HTTPD_CGI_DONE;
//    }
//
//    //Look for the 'name' GET value. If found, urldecode it and return it into the 'name' var.
//    len=httpdFindArg(connData->getArgs, "name", name, sizeof(name));
//    if (len==-1) {
//        //If the result of httpdFindArg is -1, the variable isn't found in the data.
//        strcpy(name, "unknown person");
//    } else {
//        //If len isn't -1, the variable is found and is copied to the 'name' variable
//    }
//
//    //Generate the header
//    //We want the header to start with HTTP code 200, which means the document is found.
//    httpdStartResponse(connData, 200);
//    //We are going to send some HTML.
//    httpdHeader(connData, "Content-Type", "text/html");
//    //No more headers.
//    httpdEndHeaders(connData);
//
//    //We're going to send the HTML as two pieces: a head and a body. We could've also done
//    //it in one go, but this demonstrates multiple ways of calling httpdSend.
//    //Send the HTML head. Using -1 as the length will make httpdSend take the length
//    //of the zero-terminated string it's passed as the amount of data to send.
//    httpdSend(connData, "<html><head><title>Page</title></head>", -1)
//    //Generate the HTML body.
//    len=sprintf(output, "<body><p>Hello, %s!</p></body></html>", name);
//    //Send HTML body to webbrowser. We use the length as calculated by sprintf here.
//    //Using -1 again would also have worked, but this is more efficient.
//    httpdSend(connData, output, len);
//
//    //All done.
//    return HTTPD_CGI_DONE;
//}
