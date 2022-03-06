#	 SPI_SIZE_MAP
# 	 0= 512KB( 256KB+ 256KB)
#    2=1024KB( 512KB+ 512KB)
#    3=2048KB( 512KB+ 512KB)
#    4=4096KB( 512KB+ 512KB)
#    5=2048KB(1024KB+1024KB)
#    6=4096KB(1024KB+1024KB)
#    7=4096KB(2048KB+2048KB) not support ,just for compatible with nodeMCU board
#    8=8192KB(1024KB+1024KB)
#    9=16384KB(1024KB+1024KB)

COMPILE ?= gcc
BOOT ?= new
APP ?= 1
SPI_SPEED ?= 40
SPI_MODE ?= QIO
SPI_SIZE_MAP ?= 6

ESPPORT ?= COM10
ESPBAUD ?= 921600

#############################################################
# Required variables for each makefile
# Discard this section from all parent makefiles
# Expected variables (with automatic defaults):
#   CSRCS (all "C" files in the dir)
#   SUBDIRS (all subdirs with a Makefile)
#   GEN_LIBS - list of libs to be generated ()
#   GEN_IMAGES - list of object file images to be generated ()
#   GEN_BINS - list of binaries to be generated ()
#   COMPONENTS_xxx - a list of libs/objs in the form
#     subdir/lib to be extracted and rolled up into
#     a generated lib/image xxx.a ()
#
TARGET = eagle
#FLAVOR = release
FLAVOR = debug


#EXTRA_CCFLAGS += -u


ifndef PDIR # {
GEN_IMAGES= eagle.app.v6.out
GEN_BINS= eagle.app.v6.bin
SUBDIRS=    \
	submodules_init \
	fatfs \
	libesphttpd \
	user

endif # } PDIR


APPDIR = .
LDDIR = ../ld

CCFLAGS += -Os -Wundef -std=c99

TARGET_LDFLAGS =		\
	-nostdlib		\
	-Wl,-EL \
	--longcalls \
	--text-section-literals

ifeq ($(FLAVOR),debug)
    TARGET_LDFLAGS += -g -O2
endif

ifeq ($(FLAVOR),release)
    TARGET_LDFLAGS += -g -O0
endif

COMPONENTS_eagle.app.v6 = \
	fatfs/libfatfs.a \
	libesphttpd/core/libesphttpd.a \
	user/libuser.a

LINKFLAGS_eagle.app.v6 = \
	-L../lib        \
	-nostdlib	\
    -T$(LD_FILE)   \
	-Wl,--no-check-sections	\
	-Wl,--gc-sections	\
    -u call_user_start	\
	-Wl,-static						\
	-Wl,--start-group					\
	-lc					\
	-lgcc					\
	-lhal					\
	-lphy	\
	-lpp	\
	-lnet80211	\
	-llwip	\
	-lwpa	\
	-lcrypto	\
	-lmain	\
	-ljson	\
	-lupgrade\
	-lmbedtls	\
	-lpwm	\
	-ldriver \
	-lsmartconfig \
	$(DEP_LIBS_eagle.app.v6)					\
	-Wl,--end-group

DEPENDS_eagle.app.v6 = \
                $(LD_FILE) \
                $(LDDIR)/eagle.rom.addr.v6.ld

#############################################################
# Configuration i.e. compile options etc.
# Target specific stuff (defines etc.) goes in here!
# Generally values applying to a tree are captured in the
#   makefile at its root level - these are then overridden
#   for a subtree within the makefile rooted therein
#

#UNIVERSAL_TARGET_DEFINES =		\

# Other potential configuration flags include:
#	-DTXRX_TXBUF_DEBUG
#	-DTXRX_RXBUF_DEBUG
#	-DWLAN_CONFIG_CCX
CONFIGURATION_DEFINES =	-DICACHE_FLASH \
                        -DGLOBAL_DEBUG_ON \
                        -DFATFS_DEF_UINT32_AS_INT \
                        -DUSE_FILELIB_STDIO_COMPAT_NAMES

DEFINES +=				\
	$(UNIVERSAL_TARGET_DEFINES)	\
	$(CONFIGURATION_DEFINES)

DDEFINES +=				\
	$(UNIVERSAL_TARGET_DEFINES)	\
	$(CONFIGURATION_DEFINES)


#############################################################
# Recursion Magic - Don't touch this!!
#
# Each subtree potentially has an include directory
#   corresponding to the common APIs applicable to modules
#   rooted at that subtree. Accordingly, the INCLUDE PATH
#   of a module can only contain the include directories up
#   its parent path, and not its siblings
#
# Required for each makefile to inherit from the parent
#

INCLUDES := $(INCLUDES) -I $(PDIR)include -I $(PDIR)fatfs/include -I $(PDIR)libesphttpd/include

PDIR := ../$(PDIR)
sinclude $(PDIR)Makefile

flash_size = 4MB-c1
blank_addr1 = 0x3fb000
blank_addr2 = 0x3fe000
initdata_addr = 0x3fc000


ifeq ($(SPI_SIZE_MAP), 0)
	flash_size = 512KB
	blank_addr1 = 0x7b000
	blank_addr2 = 0x7e000
	initdata_addr = 0x7c000
else
	ifeq ($(SPI_SIZE_MAP), 2)
		flash_size = 1MB
		blank_addr1 = 0xfb000
		blank_addr2 = 0xfe000
		initdata_addr = 0xfc000
	else
		ifeq ($(SPI_SIZE_MAP), 3)
			flash_size = 2MB-c1
			blank_addr1 = 0x1fb000
			blank_addr2 = 0x1fe000
			initdata_addr = 0x1fc000
		else
			ifeq ($(SPI_SIZE_MAP), 4)
				flash_size = 4MB-c1
				blank_addr1 = 0x3fb000
				blank_addr2 = 0x3fe000
				initdata_addr = 0x3fc000
			else
				ifeq ($(SPI_SIZE_MAP), 5)
					flash_size = 2MB-c1
					blank_addr1 = 0x1fb000
					blank_addr2 = 0x1fe000
					initdata_addr = 0x1fc000
				else
						ifeq ($(SPI_SIZE_MAP), 6)
							flash_size = 4MB-c1
							blank_addr1 = 0x3fb000
							blank_addr2 = 0x3fe000
							initdata_addr = 0x3fc000
					endif
				endif
			endif
		endif
	endif
endif


ifeq ($(app), 0)
	boot_name := eagle.flash.bin
	bin_path := ../bin
	bin_name := eagle.irom0text.bin
	bin_addr = 0x10000
else
    ifneq ($(boot), new)
		boot_name := boot_v1.2.bin
    else
		boot_name := boot_v1.7.bin
    endif

	bin_path := ../bin/upgrade
	bin_name := $(BIN_NAME).bin
	bin_addr := $(addr)
endif

spi_mode := $(shell echo $(SPI_MODE) | tr A-Z a-z)
spi_speed := $(SPI_SPEED)m

flash:
	esptool.py \
		--port $(ESPPORT) \
		--baud $(ESPBAUD) \
		--chip esp8266 \
		--before default_reset \
		--after hard_reset \
		write_flash -z \
		--flash_mode $(spi_mode) \
		--flash_freq $(spi_speed) \
		--flash_size $(flash_size) \
		0x00000 ../bin/$(boot_name) \
		$(bin_addr) $(bin_path)/$(bin_name) \
		$(blank_addr1) ../bin/blank.bin \
		$(initdata_addr) ../bin/esp_init_data_default_v08.bin \
		$(blank_addr2) ../bin/blank.bin
		
.PHONY: FORCE flash
FORCE:

