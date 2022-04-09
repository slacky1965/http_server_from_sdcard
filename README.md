# http_server__from_sd_card NONOS_SDK app (example)

Uploading OTA image file and any html files.

## Note

This example uses a modified `esp_http_server` component to implement the match uri

`/index.html`, `/favicon.ico`, `/style.css` and `scripts.js` can be overridden by uploading files with same names.

## Usage

* `cd /you/path/ESP8266_NONOS_SDK`
* `git clone https://github.com/slacky1965/http_server_from_sdcard.git`
* `cd web_server_uploader_esp8266`

* Go to `project/user/include/wifi.h`
	1. edit to wifi.h for you settings.
	
* `make all` for compiling applications
* `make flash` to download the app
* In order to test the file server demo
	. test the example interactively on a web browser (assuming IP is 192.168.100.40):
		- open path `http://192.168.100.40` or `http://192.168.100.40/index.html` to see an HTML web page with upload menu
		- use the file upload form on the webpage to select and upload a file to the server
		- uploading a firmware file or html files (\*.html, \*.css, \*.js or other)

* `make clean` - default clean

	
