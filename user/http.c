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

typedef struct {
    FIL fp;
    char token[64];
    int token_pos;
} html_data_t;

static int webserver_read_file(HttpdConnData *connData) {

    char buff[OTA_BUF_LEN + 1];
    size_t len;
    FRESULT ret;

    html_data_t *html_data = connData->cgiData;


    if (connData->conn==NULL) {
        //Connection aborted. Clean up.
        f_close(&(html_data->fp));
        free(html_data);
        return HTTPD_CGI_DONE;
    }

    if (html_data == NULL) {
        //First call to this cgi. Open the file so we can read it.
        html_data = os_malloc(sizeof(html_data_t));
        if (html_data == NULL) {
            return HTTPD_CGI_NOTFOUND;
        }
        os_sprintf(buff, "%s/%s", HTML_PATH, connData->url);
        ret = f_open(&(html_data->fp), buff, FA_READ);
//        tpd->tplArg=NULL;
        html_data->token_pos = -1;
        if (ret != FR_OK) {
//            espFsClose(tpd->file);
            free(html_data);
            return HTTPD_CGI_NOTFOUND;
        }
        connData->cgiData=html_data;
        httpdStartResponse(connData, 200);
        httpdHeader(connData, "Content-Type", httpdGetMimetype(connData->url));
        httpdEndHeaders(connData);
        return HTTPD_CGI_MORE;
    }

    ret = f_read(&(html_data->fp), buff, OTA_BUF_LEN, &len);
    if (len>0) {
        sp=0;
        e=buff;
        for (x=0; x<len; x++) {
            if (tpd->tokenPos==-1) {
                //Inside ordinary text.
                if (buff[x]=='%') {
                    //Send raw data up to now
                    if (sp!=0) httpdSend(connData, e, sp);
                    sp=0;
                    //Go collect token chars.
                    tpd->tokenPos=0;
                } else {
                    sp++;
                }
            } else {
                if (buff[x]=='%') {
                    if (tpd->tokenPos==0) {
                        //This is the second % of a %% escape string.
                        //Send a single % and resume with the normal program flow.
                        httpdSend(connData, "%", 1);
                    } else {
                        //This is an actual token.
                        tpd->token[tpd->tokenPos++]=0; //zero-terminate token
                        ((TplCallback)(connData->cgiArg))(connData, tpd->token, &tpd->tplArg);
                    }
                    //Go collect normal chars again.
                    e=&buff[x+1];
                    tpd->tokenPos=-1;
                } else {
                    if (tpd->tokenPos<(sizeof(tpd->token)-1)) tpd->token[tpd->tokenPos++]=buff[x];
                }
            }
        }
    }
    //Send remaining bit.
    if (sp!=0) httpdSend(connData, e, sp);
    if (len!=1024) {
        //We're done.
        ((TplCallback)(connData->cgiArg))(connData, NULL, &tpd->tplArg);
        espFsClose(tpd->file);
        free(tpd);
        return HTTPD_CGI_DONE;
    } else {
        //Ok, till next time.
        return HTTPD_CGI_MORE;
    }
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
