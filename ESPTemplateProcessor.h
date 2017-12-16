#ifndef ESP_TEMPLATE_PROCESSOR_H
#define ESP_TEMPLATE_PROCESSOR_H

typedef String ProcessorCallback(const String& key);

class ESPTemplateProcessor {
  public:
    ESPTemplateProcessor(WEBSERVER& _server) :
      server(_server)
    {
    }

    bool send(const String& filePath, ProcessorCallback& processor)
    {
      // Open file.
      if(!SPIFFS.exists(filePath)) {
        Serial.print("Cannot process "); Serial.print(filePath); Serial.println(": Does not exist.");
        return false;
      }

      File file = SPIFFS.open(filePath, "r");
      if (!file) {
        Serial.print("Cannot process "); Serial.print(filePath); Serial.println(": Failed to open.");
        return false;
      }

      server.setContentLength(CONTENT_LENGTH_UNKNOWN);
      server.sendHeader("Content-Type","text/html",true);
      server.sendHeader("Cache-Control","no-cache");
      server.send(200);
      //server.sendContent(<chunk>)

      // Process!
      static const uint16_t MAX = 2048;
      static const uint8_t MAXKEY = 20;
      String buffer;
      int keyBufferLen = 0;
      int bufferLen = 0;
      String keyBuffer;
      int val;
      char ch;
      while ((val = file.read()) != -1) {
        ch = char(val);
        
        // Lookup expansion.
        if (ch == '%') {
          // Clear out buffer.
          server.sendContent(buffer);
          buffer = "";
          bufferLen = 0;

          // Process substitution.
          keyBuffer = "";
          bool found = false;
          while (!found && (val = file.read()) != -1) {
            ch = char(val);
            if (ch == '%') {
              found = true;
            } else {
              keyBuffer += ch;
              keyBufferLen++;
              if (keyBufferLen > MAXKEY) {
                // the key was too long so add this to the "regular" content buffer and break out.
                buffer += "%" + keyBuffer;
                keyBufferLen = 0;
                break;
              }
            }
          }
          
          if (found) {
            // Get substitution value
            String processed = processor(keyBuffer);
            
            // empty out the key buffer
            keyBufferLen = 0;
            keyBuffer = "";
            
            Serial.print("Lookup '"); Serial.print(keyBuffer); Serial.print("' received: "); Serial.println(processed);
            server.sendContent(processed);
          } else if (val == -1 && !found) {
            Serial.print("Cannot process "); Serial.print(filePath); Serial.println(": Unable to parse.");
            return false;
          } else {
            continue;
          }

        } else {
          bufferLen++;
          buffer += ch;
          if (bufferLen >= MAX) {
            server.sendContent(buffer);
            bufferLen = 0;
            buffer = "";
          }
        }
      }

      if (val == -1) {
        server.sendContent(buffer);
        server.sendContent("");
        return true;
      } else {
        Serial.print("Failed to process '"); Serial.print(filePath); Serial.println("': Didn't reach the end of the file.");
      }
    }


  private:
    WEBSERVER &server;
};
#endif
