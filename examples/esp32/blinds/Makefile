PROJECT_NAME = blinds

CFLAGS += -I$(abspath ../../..) -DHOMEKIT_SHORT_APPLE_UUIDS -DHOMEKIT_MAX_CLIENTS=16

EXTRA_COMPONENT_DIRS += $(abspath ../../../components)

include $(IDF_PATH)/make/project.mk



# PROGRAM = blinds

# EXTRA_COMPONENTS = \
# 	extras/http-parser \
# 	extras/dhcpserver \
# 	extras/httpd \
# 	extras/mbedtls \
# 	$(abspath ../../components/wifi_config) \
# 	$(abspath ../../components/wolfssl) \
# 	$(abspath ../../components/cJSON) \
# 	$(abspath ../../components/homekit)

# FLASH_SIZE ?= 32

# EXTRA_CFLAGS += -I../.. -DHOMEKIT_SHORT_APPLE_UUIDS -DLWIP_HTTPD_CGI=1 -DLWIP_HTTPD_SSI=1 -DINCLUDE_eTaskGetState=1 -DENABLE_MOTOR_B=1 -I./fsdata

# include $(SDK_PATH)/common.mk

# monitor:
# 	$(FILTEROUTPUT) --port $(ESPPORT) --baud 115200 --elf $(PROGRAM_OUT)

html:
	@echo "Generating fsdata..."
	cd fsdata && ./makefsdata