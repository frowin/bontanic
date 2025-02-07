/*********
  Rui Santos & Sara Santos - Random Nerd Tutorials
  Modified for WiFiManager and fake sensor data
*********/
#include <Arduino.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <ArduinoOTA.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "LittleFS.h"
#include <Arduino_JSON.h>
#include <esp_system.h>

// Forward declaration of readHelloWorld
void readHelloWorld();

WiFiManager wm;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// Json Variable to Hold Sensor Readings
JSONVar readings;

// Timer variables
unsigned long lastTime = 0;
unsigned long timerDelay = 100;  // Send data every 2 seconds

// Get Sensor Readings and return JSON object
String getSensorReadings() {
    readings["temperature"] = String((millis() % 10) + 20);  // 20-30°C
    readings["humidity"] = String((millis() % 30) + 40);     // 40-70%
    readings["pressure"] = String((millis() % 100) + 900);   // 900-1000 hPa
    
    // System stats
    readings["heap"] = String(ESP.getFreeHeap() / 1024);     // Free heap in KB
    readings["cpu_freq"] = String(ESP.getCpuFreqMHz());      // CPU frequency
    readings["flash_size"] = String(ESP.getFlashChipSize() / (1024 * 1024)); // Flash size in MB
    readings["sketch_size"] = String(ESP.getSketchSize() / 1024);  // Sketch size in KB
    readings["free_sketch"] = String(ESP.getFreeSketchSpace() / 1024); // Free sketch space in KB
    
    String jsonString = JSON.stringify(readings);
    return jsonString;
}

// Initialize LittleFS
void initLittleFS() {
    if (!LittleFS.begin(true)) {
        Serial.println("An error has occurred while mounting LittleFS");
    }
    Serial.println("LittleFS mounted successfully");
}

void notifyClients(String sensorReadings) {
    ws.textAll(sensorReadings);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
        String sensorReadings = getSensorReadings();
        Serial.print(sensorReadings);
        notifyClients(sensorReadings);
    }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
            break;
        case WS_EVT_DISCONNECT:
            Serial.printf("WebSocket client #%u disconnected\n", client->id());
            break;
        case WS_EVT_DATA:
            handleWebSocketMessage(arg, data, len);
            break;
        case WS_EVT_PONG:
        case WS_EVT_ERROR:
            break;
    }
}

void initWebSocket() {
    ws.onEvent(onEvent);
    server.addHandler(&ws);
}

void setup() {
    Serial.begin(115200);
    
    // Set device hostname
    WiFi.setHostname("bontanic");
    
    bool res = wm.autoConnect("AutoConnectAP");
    if(!res) {
        Serial.println("Failed to connect");
    } else {
        Serial.println("Connected to WiFi");
        
        // OTA Setup
        ArduinoOTA.setHostname("bontanic"); // Same hostname as above
        ArduinoOTA.setMdnsEnabled(false); 

        // Optional password for OTA updates
        // ArduinoOTA.setPassword("admin");
        
        ArduinoOTA.onStart([]() {
            String type;
            if (ArduinoOTA.getCommand() == U_FLASH) {
                type = "sketch";
            } else { // U_FS
                type = "filesystem";
            }
            Serial.println("Start updating " + type);
        });
        
        ArduinoOTA.onEnd([]() {
            Serial.println("\nEnd");
        });
        
        ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
            Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
        });
        
        ArduinoOTA.onError([](ota_error_t error) {
            Serial.printf("Error[%u]: ", error);
            if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
            else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
            else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
            else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
            else if (error == OTA_END_ERROR) Serial.println("End Failed");
        });
        
        ArduinoOTA.begin();
        Serial.println("OTA Ready");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
    }

    initLittleFS();
    initWebSocket();

    // Web Server Root URL
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/index.html", "text/html");
    });

    server.serveStatic("/", LittleFS, "/");

    // Start server
    server.begin();

    readHelloWorld();
}

void loop() {
    ArduinoOTA.handle(); // Handle OTA updates
    //wm.startConfigPortal(); // Blocks until configuration is done or timeout

    if ((millis() - lastTime) > timerDelay) {
        String sensorReadings = getSensorReadings();
        Serial.print(sensorReadings);
        notifyClients(sensorReadings);
        lastTime = millis();
    }
    ws.cleanupClients();
}

void readHelloWorld() {
    if(!LittleFS.begin(true)){
    Serial.println("An Error has occurred while mounting LittleFS");
    return;
  }
  
  File file = LittleFS.open("/text.txt");
  if(!file){
    Serial.println("Failed to open file for reading");
    return;
  }
  
  Serial.println("File Content:");
  while(file.available()){
    Serial.write(file.read());
  }
  Serial.println("");
  Serial.println("File closed");
  file.close();
}