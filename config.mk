THIS_DIR := $(realpath $(dir $(realpath $(lastword $(MAKEFILE_LIST)))))
ROOT := $(THIS_DIR)/..
FS_DIR := $(THIS_DIR)/data

ifeq ($(CHIP),esp32)
LIBS = $(ESP_LIBS)/WebServer \
  $(ESP_LIBS)/WiFi \
  $(ESP_LIBS)/ESPmDNS \
  $(ESP_LIBS)/SPIFFS \
  $(ESP_LIBS)/FS
else
LIBS = $(ESP_LIBS)/ESP8266WebServer \
  $(ESP_LIBS)/ESP8266WiFi \
  $(ESP_LIBS)/ESP8266mDNS 
endif

LIBS += $(ROOT)/libraries/SimpleDHT
LIBS += $(ROOT)/libraries/PubSubClient/src
	
UPLOAD_SPEED=115200