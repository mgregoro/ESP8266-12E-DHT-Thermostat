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

#include <FS.h>

void turnHeatOn();
void turnHeatOff();
unsigned long heatRunningFor();
void pollTemperature();
String generateTemplateKeyValuePairs (const String& key);
void handleRoot();
bool isValidNumber(String str);
void handleUpdate();
void handleHeatStatus();
void handleGetCurrentTemp();
void considerFurnaceStateChange();
bool handleStaticFile(String path);
String contentType(String filename);