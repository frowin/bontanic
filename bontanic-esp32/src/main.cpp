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
#define ENABLE_GxEPD2_GFX 0
#include <GxEPD2_BW.h>
#include <Fonts/FreeMonoBold9pt7b.h>
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

// 1.54" EPD Module
//GxEPD2_BW<GxEPD2_154, GxEPD2_154::HEIGHT> display(GxEPD2_154(/*CS=*/ 27, /*DC=*/ 14, /*RST=*/ 12, /*BUSY=*/ 13)); // GDEP015OC1 200x200, IL3829
GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display(GxEPD2_154_D67(/*CS=*/ 27, /*DC=*/ 14, /*RST=*/ 12, /*BUSY=*/ 13)); // GDEP015OC1 200x200, IL3829

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

void displayStatusBar() {
    display.setRotation(1);
    display.setFont(&FreeMonoBold9pt7b);
    display.setTextColor(GxEPD_BLACK);
    
    // Draw black top bar
    display.fillRect(0, 0, display.width(), 20, GxEPD_BLACK);
    
    // Display IP Address in white
    display.setTextColor(GxEPD_WHITE);
    String ip = WiFi.localIP().toString();
    display.setCursor(5, 15);
    display.print(ip);
    
    // Draw online indicator (small circle)
    if (WiFi.status() == WL_CONNECTED) {
        display.fillCircle(190, 10, 5, GxEPD_WHITE);
    }
}

void displayMessage(const char* message)
{
    display.setRotation(1);
    display.setFont(&FreeMonoBold9pt7b);
    display.setTextColor(GxEPD_BLACK);
    int16_t tbx, tby; 
    uint16_t tbw, tbh;
    display.getTextBounds(message, 0, 0, &tbx, &tby, &tbw, &tbh);
    // Adjust y position to account for status bar
    uint16_t x = ((display.width() - tbw) / 2) - tbx;
    uint16_t y = ((display.height() - tbh) / 2) - tby + 20; // Added +20 to move below status bar
    display.setFullWindow();
    display.firstPage();
    do
    {
        display.fillScreen(GxEPD_WHITE);
        displayStatusBar();  // Draw status bar first
        display.setTextColor(GxEPD_BLACK);
        display.setCursor(x, y);
        display.print(message);
    }
    while (display.nextPage());
}

void setup() {
    Serial.begin(115200);
    
    // Initialize display early
    display.init(115200, true, 2, false);
    displayMessage("Connecting...");
    
    // Set device hostname
    WiFi.setHostname("bontanic");
    
    bool res = wm.autoConnect("AutoConnectAP");
    if(!res) {
        displayMessage("WiFi Failed!");
    } else {
        displayMessage("Connected!");
        
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