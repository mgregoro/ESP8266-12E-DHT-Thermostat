/* ESP8266/ESP32 Thermostat
 * (c) 2017 Michael Gregorowicz
 * Remixed from https://github.com/dmainmon/ESP8266-12E-DHT-Thermostat by Damon Borgnino
 * Changes include: not dependent on cloud services, mobile friendly UI
 */

#include "Configuration.h"
#include "ESP8266-Thermostat.h"
#include "ESPTemplateProcessor.h"

// Wifi Setup
const char* ssid     = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// Our hostname
const char* mdns_hostname = MDNS_HOSTNAME;

// MQTT setup
const char* mqttClientID = MQTT_CLIENTID;
const char* mqttUserID = MQTT_USER;
const char* mqttUserPassword = MQTT_PASSWORD;

// MQTT topics from this host
const char* _t_initialization_topic = INITIALIZATION_TOPIC;
const char* _t_humidity_topic = HUMIDITY_TOPIC;
const char* _t_temperature_topic = TEMPERATURE_TOPIC;
const char* _t_furnace_start_topic = FURNACE_START_TOPIC;
const char* _t_furnace_stop_topic = FURNACE_STOP_TOPIC;
const char* _t_furnace_runtime_topic = FURNACE_RUNTIME_TOPIC;

// Where we'll store our configuration (on flash)
const String propertiesFile = "prop.dat";

/* Hard coded temperature "skew" .. how far to overshoot the target so 
 * we don't flap the furnace on and off..
 */
const float ttSkew = 1.75;

/* Also hard coded a bias if you think your DHT is lying, this is very 
 * subjective, but, a compile time tuneable just for you.  Merry Christams
 */
const float temp_f_bias = 0;

/*
 * Some like it hot.  I like it at 69.  Author's privilege, but overridden by the UI
 */
float targetTemperature = 69;

// It's ok to poll the DHT22 every 10 seconds.  Some sensors won't like this though.
unsigned long pollInterval = 10000;

// State Variables
int pollId = 1;
bool heatOn = false;
unsigned long systemInitialized = 0;
unsigned long heatStarted = 0;
unsigned long heatStopped = 0;
unsigned long heatLastRanFor = 0;

// Set WiFi Mode, initialize the web server, DHT sensor, MQTT pubsub
WEBSERVER server(80);
WiFiClient espClient;
SimpleDHT22 dht22;
PubSubClient mqtt_c;

// These will have data from the sensors soon.  I promise.
float humidity = 0, temp_f = 0;  // Input from the DHT
String webString = "";   // To store rendered web pages
String webMessage = "";

unsigned long lastPollTime = 0;                     // will store last temp was read

/*
 * Arduino Functions
 */

void setup(void) {
    // You can open the Arduino IDE Serial Monitor window to see what the code is doing
    Serial.begin(115200);  // Serial connection from ESP-01 via 3.3v console cable
    pinMode(RELAYPIN, OUTPUT);
    
    // Connect to WiFi network
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    Serial.print("\n\r \n\rWorking to connect");

    // Wait for connection
    while (WiFi.status() != WL_CONNECTED) {
        delay(200);
        Serial.print(".");
    }

    Serial.print("Obtained IP address: ");
    Serial.println(WiFi.localIP());
    
    // set up mqtt
    mqtt_c.setClient(espClient);
    mqtt_c.setServer(MQTT_SERVER, 1883);
    mqtt_c.setCallback(subCallback);

    if (mqtt_connect()) {
        systemInitialized = millis();
        publish(_t_initialization_topic, String(systemInitialized, DEC));
    }

    if (SPIFFS.begin()) {
      Serial.println("[debug] SPIFFS filesystem initialized sucessfully");
    } else {
      Serial.println("[debug] Error initializing SPIFFS filesystem");
    }
    delay(10);

    bool writeProperties = false;

    File f = SPIFFS.open(propertiesFile, "r");

    if (!f) {
        // no properties file exists, create one and signal that we need to
        // write it out
        f = SPIFFS.open(propertiesFile, "w");
        writeProperties = true;
    }

    if (f && !writeProperties) {
        Serial.println("[debug] loading settings from properties file on SPIFFS");
        int tgt = f.readStringUntil(',').toInt();
        unsigned long pi = (unsigned long)f.readStringUntil('\n').toInt();

        if (tgt > 0) {
            targetTemperature = tgt;
        } else {
            writeProperties = true;
        }

        if (pi > 0) {
            pollInterval = pi;
        } else {
            writeProperties = true;
        }
    }

    if (writeProperties) {
        // write the defaults to the properties file
        Serial.println("[debug] created new configuration file from global defaults");

        // comma delimited upper,lower,dhtpollinterval
        f.print(targetTemperature);
        f.print(",");
        f.print(pollInterval);
        f.print("\n");
        f.close();
    }

    // Debug output current configuration values
    Serial.println("[debug] target temp: " + String(targetTemperature, DEC) + " °F");
    Serial.println("[debug] poll interval: " + String(pollInterval, DEC) + "ms");

    // Load in first values of temperature data
    pollTemperature();

    // start the furnace if required...
    considerFurnaceStateChange();

    // set the hostname
    MDNS.begin(mdns_hostname);

    Serial.print("[debug] service available at http://");
    Serial.print(mdns_hostname);
    Serial.println(".local/");

    // web client handlers
    server.on("/", HTTP_GET, handleRoot);
    server.on("/update", HTTP_POST, handleUpdate);
    server.on("/heat_status", HTTP_GET, handleHeatStatus);
    server.on("/cur_temp", HTTP_GET, handleGetCurrentTemp);

    server.onNotFound([]() {
        if (!handleStaticFile(server.uri())) {
            server.send(404, "text/plain", "<h1>File Not Found</h1><p>The requested resource was not found on this server.</p>");
        } 
    });

    // start the web server
    server.begin();
}

void loop(void) {
    // everything's an unsigned long.
    if (((lastPollTime + pollInterval) <= millis()) || (lastPollTime == 0 && millis() > 0)) {
        // save the last time you read the sensor
        lastPollTime = millis();
        
        Serial.print("[debug] Polling Room Temp; PollID: #");
        Serial.println(pollId);
        pollTemperature();

        Serial.print("[debug] temperature: ");
        Serial.println(temp_f);
        Serial.print("[debug] humidity: ");
        Serial.println(humidity);
        
        Serial.print("[debug] furnace state: ");
        if (heatOn) {
            Serial.println("ON");
        } else {
            Serial.println("OFF");
        }
      
        considerFurnaceStateChange();

        pollId++;
    }
    

    if (!mqtt_c.loop()) {
        if (!mqtt_connect()) {
          delay(200);
          if (!mqtt_connect()) {
            Serial.println("[warning] error connecting to MQTT sevice");
          }
        }
    }

    server.handleClient();
}

/*
 * Callback for thermostat/settemp
 */

void subCallback(char* topic, byte* payload, unsigned int length) {
    if (String(topic).startsWith("command/settemp")) {
        unsigned char* p = (byte*)malloc(length);
        memcpy(p, payload, length);
        targetTemperature = String((char*)payload).toInt();
        updatePropertiesFile();
        pollTemperature();
    } else if (String(topic).startsWith("thermostat/temperature")) {
        unsigned char* p = (byte*)malloc(length);
        memcpy(p, payload, length);
        temp_f = (
            (temp_f + temp_f_bias + String((char*)payload).toFloat()) / 2
        );
    } else {
        Serial.println("Unhandled topic received: " + String(topic));
    }   
}

/*
 * Supplemental Functions
 */

void turnHeatOn() {
    digitalWrite(RELAYPIN, HIGH);
    heatOn = true;
    heatStarted = millis();
    publish(_t_furnace_start_topic, String(heatStarted, DEC));
}

void turnHeatOff() {
    digitalWrite(RELAYPIN, LOW);
    heatOn = false;
    heatStopped = millis();
    heatLastRanFor = ((heatStopped - heatStarted) / 1000);
    publish(_t_furnace_stop_topic, String(heatStopped, DEC));
    publish(_t_furnace_runtime_topic, String(heatLastRanFor, DEC));
}

unsigned long heatRunningFor() {
    if (heatOn) {
        return ((millis() - heatStarted) / 1000);
    }
    return 0;
}

bool mqtt_connect() {
    bool _connected = false;
    if (mqtt_c.connect(mqttClientID, mqttUserID, mqttUserPassword)) {
        _connected = true;
        Serial.println("[debug] MQTT connection established in 1 tries");
    } else {
        delay(200);
        if (mqtt_c.connect(mqttClientID, mqttUserID, mqttUserPassword)) {
            _connected = true;
            Serial.println("[debug] MQTT connection established in 2 tries");
        } else {
            delay(200);
            if (mqtt_c.connect(mqttClientID, mqttUserID, mqttUserPassword)) {
                _connected = true;
                Serial.println("[debug] MQTT connection established in 3 tries");
            }
        }
    }
    
    if (_connected) {
        mqtt_c.subscribe("command/settemp");
        mqtt_c.subscribe("thermostat/temperature");
    } else {
        Serial.println("[debug] mqtt_connect failed to connect after trying 3 times");
        Serial.print("[debug] mqtt client state: ");
        Serial.println(String(mqtt_c.state(), DEC));
    }
    
    return _connected;
}

bool publish(String topic, String payload) {
    bool _published = false;
    if (mqtt_c.connected()) {
        mqtt_c.publish(topic.c_str(), payload.c_str());
        _published = true;
    } else {
        mqtt_c.disconnect();
        if (mqtt_connect()) {
            mqtt_c.publish(topic.c_str(), payload.c_str());
            _published = true;
        }
    }
    return _published;
}

// reads the temp and humidity from the DHT sensor and sets the global variables for both
void pollTemperature() {
  float t = 0, h = 0;
  int err = SimpleDHTErrSuccess;
  if ((err = dht22.read2(DHTPIN, &t, &h, NULL)) != SimpleDHTErrSuccess) {
    Serial.print("Read DHT22 failed err: ");
    Serial.println(err);
    delay(200);
    return;
  }
  
  // Check if any reads failed and report over debug if so.
  if (isnan(h)) {
    Serial.println("DHT sensor reported humidity as NaN; keeping old value: " + String(humidity, DEC));
    return;
  } else if (h > 100) {
    Serial.println("DHT sensor reported humidity as greater than 100%%; keeping old value: " + String(humidity, DEC));
    return;  
  } else {
    humidity = h;
  }
  
  publish(_t_humidity_topic, String(humidity, DEC));
  
  if (isnan(temp_f)) {
    Serial.println("DHT sensor reported Temp as NaN; keeping old value: " + String(temp_f, DEC));
  } else {
    // gotta do the ferenheit conversion here as SimpleDHT doesn't do it for us.
    temp_f = (t * 1.8) + 32 + temp_f_bias;
  }
  
  publish(_t_temperature_topic, String(temp_f, DEC));
}

/*
 * HTML Rendering Functions
 */

String generateTemplateKeyValuePairs (const String& key) {
    if (key == "CURRENT_TEMP") { 
        return String((int)temp_f, DEC);
    } else if (key == "TARGET_TEMP") {
        return String((int)targetTemperature, DEC);
    } else if (key == "POLL_INVERVALMS") {
        return String(pollInterval, DEC);
    } else if (key == "POLL_INTERVAL") {
        return String(pollInterval / 1000, DEC);
    } else if (key == "HEAT_STATUS") {
        return String(heatOn ? "ON" : "OFF");
    } else if (key == "HUMIDITY_PCT") {
        return String(humidity, DEC);
    } else if (key == "HEAT_RUN") {
        return (heatOn ? 
               String("Furnace has been running for " + String(heatRunningFor(), DEC) + " seconds") : 
               String("Furnace last ran for " + String(heatLastRanFor, DEC) + " seconds"));
    }
}

void handleRoot() {
    if (ESPTemplateProcessor(server).send(String("/template_root.htmlt"), generateTemplateKeyValuePairs)) {
        Serial.println("[debug] root template successfully rendered and sent to client");
    } else {
        Serial.println("[error] root template render error");
        server.send(500, "text/html", String("<h1>Internal Server Error</h1><p>The author of this application is an idiot, and ") + 
            String("now you are paying the price.  In fairness so is the President of the United States so... your fault too?</p>"));
    }
}

// examine if 
bool isValidNumber(String str) {
    bool oneDot = false;
    String dot = String(".");
    for (int i = 0; i < str.length(); i++) {
        if (!isDigit(str.charAt(i)) && !(String(str.charAt(i)) == dot)) {
            // 0-9 or . are all that we accept.  this not isValidNumber.
            return false;
        } else {
            if (String(str.charAt(i)) == dot) {
                if (oneDot == true) {
                    Serial.println("Yes.. there is.. there's got to be.");        
                    Serial.println("More than one dot?!" + String(str.charAt(i)));
                    // floating point numbers can't have more than one dot!
                    return false;
                } else {
                    Serial.println("We got our one allowed dot...");
                    oneDot = true;
                }
            }
        }
    }
    Serial.println("All clear.. Looks like we passed!");
    return true;
}

// this is an AJAX call, one of two user-settable values.  Temperature and poll interval.
void handleUpdate() {
    String response = "";
    float tempTgt = 0;
    unsigned long sRate = 0;

    if (server.args() > 0 ) {
        for ( uint8_t i = 0; i < server.args(); i++ ) {
            
            // read in the target temperature
            if (server.argName(i) == "temp_target") {
                if (server.arg(i) != "") {
                    if (isValidNumber(server.arg(i))) {
                        tempTgt = server.arg(i).toFloat();
                    } else {
                        response += "Heat On must be a number";
                    }
                }
            }

            // read in the DHT sample rate (in seconds)
            if (server.argName(i) == "sample_rate") {
                if (server.arg(i) != "") {
                    if (isValidNumber(server.arg(i))) {
                        sRate = server.arg(i).toInt();
                        if (sRate < 5) {
                            response += "5 seconds is the minimum sensor poll interval";
                        }
                        if (sRate > 60) {
                            response += "60 seconds is the maximum sensor poll interval";
                        }
                    } else {
                        response += "Sample Rate must be a number";
                    }
                }
            }
        }
    }

    if (response == "") {
        bool something_changed = false;
        // No errors, let's set what's been passed and if anything changed...
        if (tempTgt > 0 && tempTgt != targetTemperature) {
            targetTemperature = tempTgt;
            response += "TGT_CHANGE_SUCCESS;";
            something_changed = true;
        }
        if (sRate > 0 && (sRate != (pollInterval / 1000))) {
            pollInterval = sRate * 1000;
            response += "PI_CHANGE_SUCCESS;";
            something_changed = true;
        }

        if (something_changed) {
            response += updatePropertiesFile();
        }
    } 

    pollTemperature();

    server.send(200, "text/plain", response);
}

String updatePropertiesFile() {
    String response = "";
    File f = SPIFFS.open(propertiesFile, "w");
    if (!f) {
        Serial.println("file open for properties failed");
    } else {
        // write the defaults to the properties file
        Serial.println("====== Writing to properties file ======");

        // comma delimited upper,lower,dhtpollinterval
        f.print(targetTemperature);
        f.print(",");
        f.print(pollInterval);
        f.print("\n");
        f.close();
        response += "CONFIG_WRITE_SUCCESS";
    }
    return response;
}

void handleHeatStatus() {
    if (heatOn) {
        server.send(200, "text/plain", String("ON"));
    } else {
        server.send(200, "text/plain", String("OFF"));
    }
}

// semicolon delimited informational tidbits...
void handleGetCurrentTemp() {
    server.send(200, "text/plain",     
        String(
            String((int)temp_f, DEC) + ";" + String((int)humidity, DEC) + ";" + 
            String((int)targetTemperature, DEC) + ";" + String((int)pollInterval / 1000, DEC) + ";" +
            (
                heatOn ? 
                    String("Furnace has been running for " + String(heatRunningFor(), DEC) + " seconds") : 
                    String("Furnace last ran for " + String(heatLastRanFor, DEC) + " seconds")
            )
        )
    );
}

void considerFurnaceStateChange() {
    if ((temp_f <= targetTemperature) && !heatOn) {
        if (temp_f == 0 && humidity == 0) {
          turnHeatOff();
          Serial.println("[info] temperature and humidity are both zero; assuming improper reading from DHT, ensuring furnace remains OFF until we get a proper reading");
        } else {
          turnHeatOn();
          Serial.println("[info] below targetTemperature; turning ON furnace..");
        }
    } else if (heatOn && (temp_f >= (targetTemperature + ttSkew))) {
        turnHeatOff();
        Serial.println("[info] targetTemperature met; turning OFF furnace..");
    } else {
        Serial.println("[info] furnace state change not required");
    }
}

// 404 handler passes through to static file handler
bool handleStaticFile(String path) {
    if (path.endsWith("/")) {
        path += "index.html";
    }
  
    String ct = contentType(path);
  
    if (SPIFFS.exists(path)) {
        // no raw templates, and no downloading config files.
        if (!(path.endsWith(".htmlt") || path.endsWith(".dat"))) {   
            File file = SPIFFS.open(path, "r");
            size_t sent = server.streamFile(file, ct);
            file.close();
            return true;
        }
    } 
    
    // no static file found... or illegal file 
    return false;
}

String contentType(String filename) {
  if(filename.endsWith(".htm")) return String("text/html");
  else if(filename.endsWith(".html")) return String("text/html");
  else if(filename.endsWith(".htmlt")) return String("text/html");
  else if(filename.endsWith(".css")) return String("text/css");
  else if(filename.endsWith(".js")) return String("application/javascript");
  else if(filename.endsWith(".png")) return String("image/png");
  else if(filename.endsWith(".gif")) return String("image/gif");
  else if(filename.endsWith(".jpg")) return String("image/jpeg");
  else if(filename.endsWith(".ico")) return String("image/x-icon");
  else if(filename.endsWith(".xml")) return String("text/xml");
  else if(filename.endsWith(".pdf")) return String("application/x-pdf");
  else if(filename.endsWith(".zip")) return String("application/x-zip");
  else if(filename.endsWith(".gz")) return String("application/x-gzip");
  return String("text/plain");
}
