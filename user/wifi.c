#include "osapi.h"
#include "user_interface.h"
#include "mem.h"

#include "wifi.h"
#include "utils.h"

static os_timer_t sta_timer;

static void ICACHE_FLASH_ATTR sta_timer_cb() {

    struct scan_config scanSt;

    os_timer_disarm(&sta_timer);

    if (wifi_get_opmode() == SOFTAP_MODE) {
        os_printf("AP mode can't scan!!!\n");
        os_timer_arm(&sta_timer, 5000, false);
        return;
    }

    os_bzero(&scanSt, sizeof(struct scan_config));
    scanSt.ssid = (uint8_t*)MY_STA_SSID;


    os_timer_arm(&sta_timer, 30000, false);
}



static void ICACHE_FLASH_ATTR wifi_event_cb(System_Event_t *evt) {

    char* mac;
    static bool timerOn = false;

    switch (evt->event) {
    case EVENT_STAMODE_CONNECTED:
        timerOn = false;
        os_printf("Connected to %s!\n", MY_STA_SSID);
        break;
    case EVENT_STAMODE_GOT_IP:
        timerOn = false;
        os_printf("IP  address: %s\n", get_ip_address(STATION_IF));
        mac = get_mac_address(STATION_IF);
        os_printf("MAC address: %C%C:%C%C:%C%C:%C%C:%C%C:%C%C\n", mac[0],
                mac[1], mac[2], mac[3], mac[4], mac[5], mac[6], mac[7], mac[8],
                mac[9], mac[10], mac[11]);
        break;
    case EVENT_STAMODE_DISCONNECTED:
        if (!timerOn) {
            timerOn = true;
            os_timer_disarm(&sta_timer);
            os_timer_setfn(&sta_timer, (os_timer_func_t *)sta_timer_cb, (void *)0);
            os_timer_arm(&sta_timer, 5000, false);
        }
        break;
    default:
        break;
    }
}



void ICACHE_FLASH_ATTR start_wifi_sta() {

    char *mac, *hostName;

    struct station_config sta_config;

    os_printf("Start WiFi STA Mode\n");
    os_printf("Connecting to: %s \n", MY_STA_SSID); //wmConfig.staSsid

    wifi_station_disconnect();
    wifi_station_dhcpc_stop();

    wifi_set_phy_mode(PHY_MODE_11G);

    wifi_station_get_config(&sta_config);

    wifi_set_opmode_current(STATION_MODE);

    mac = get_mac_address(STATION_IF);

    hostName = os_malloc(strlen(MY_STA_SSID)+strlen(mac)+3);

    if (!hostName) {
        os_printf("start_wifi_sta: Error allocation memory. (%s:%u)", __FILE__, __LINE__);
        return;
    }

    os_timer_disarm(&sta_timer);

    os_sprintf(hostName, "%s-%s", MY_STA_SSID, mac);

    if (strlen(hostName) > 32)
        hostName[32] = 0;

    wifi_station_set_hostname(hostName);

    os_free(hostName);

    sta_config.bssid_set = 0;

    os_sprintf((char*)sta_config.ssid, "%s", MY_STA_SSID);

    os_sprintf((char*)sta_config.password, "%s", MY_STA_PASSWORD);

    wifi_station_set_config_current(&sta_config);
    wifi_station_connect();
    wifi_station_dhcpc_start();
    wifi_set_event_handler_cb(wifi_event_cb);

    os_delay_us(50000);
}

