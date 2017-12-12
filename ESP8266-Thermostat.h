#include <pgmspace.h>

#ifdef ARDUINO_ARCH_ESP32
#include <WiFi.h>
#include <WiFiClient.h>
#include <ESPmDNS.h>
#include <ESP32WebServer.h>
#include <SPIFFS.h>
#define WEBSERVER ESP32WebServer
#endif

#ifdef ARDUINO_ARCH_ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#define WEBSERVER ESP8266WebServer
#endif

#define INITIALIZATION_TOPIC "thermostat/initialize/_thermostat"
#define HUMIDITY_TOPIC "sensor/humidity/_thermostat"
#define TEMPERATURE_TOPIC "sensor/temperature/_thermostat"
#define FURNACE_START_TOPIC "furnace/started/_thermostat"
#define FURNACE_STOP_TOPIC "furnace/stopped/_thermostat"
#define FURNACE_RUNTIME_TOPIC "furnace/lastruntime/_thermostat"

#include <PubSubClient.h>
#include <FS.h>
#include <SimpleDHT.h>
#include <ctype.h>
#include <string.h>

void turnHeatOn();
void turnHeatOff();
unsigned long heatRunningFor();
void pollTemperature();
String generateTemplateKeyValuePairs (const String& key);
void handleRoot();
bool isValidNumber(String str);
bool mqtt_connect();
void handleUpdate();
void handleHeatStatus();
void handleGetCurrentTemp();
String updatePropertiesFile();
void considerFurnaceStateChange();
bool handleStaticFile(String path);
String contentType(String filename);
bool publish(String topic, String payload);
void subCallback(char* topic, byte* payload, unsigned int length);
