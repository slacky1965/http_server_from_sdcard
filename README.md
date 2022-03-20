# web_server_uploader_esp8266 NONOS_SDK app (example)

Uploading OTA image file and any html files.

## Note

This example uses a modified `esp_http_server` component to implement the match uri

`/index.html`, `/favicon.ico`, `/style.css` and `scripts.js` can be overridden by uploading files with same names.

Tested on Windows 10 and Linux Debian

## Usage

* Get `toolchain` and `ESP8266_RTOS_SDK` from `espressif` web site - https://docs.espressif.com/projects/esp8266-rtos-sdk/en/latest/get-started/index.html
* `git clone https://github.com/slacky1965/web_server_uploader_esp8266.git`
* `cd web_server_uploader_esp8266`

* Open the project configuration menu `make menuconfig`

* Go to `Serial flasher config` and set `Default serial port`
* Go to `Example Connection Configuration`
	1. WIFI SSID: WIFI network to which your PC is also connected to.
	2. WIFI Password: WIFI password
	
* `make all` for compiling applications
* `make flash` to download the app
* `make storage` to create `spiffs` partition and then download it
* In order to test the file server demo
	1. run `make monitor` and note down the IP assigned to your ESP module. The default port is 80
	2. test the example interactively on a web browser (assuming IP is 192.168.100.40):
		- open path `http://192.168.100.40` or `http://192.168.100.40/index.html` to see an HTML web page with upload menu
		- use the file upload form on the webpage to select and upload a file to the server
		- uploading a firmware file or html files (\*.html, \*.css, \*.js or other)

* `make cleanall` - default clean and clean `mkspiffs`

	
