#include <Arduino.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <ArduinoOTA.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "LittleFS.h"
#include <Arduino_JSON.h>

// Forward declaration of readHelloWorld
void readHelloWorld();

WiFiManager wm;


void setup() {

    Serial.begin(115200);
    
    WiFi.setHostname("bontanic"); // For ESP32
    
    bool res;
    res = wm.autoConnect("AutoConnectAP"); // anonymous ap
    if(!res) {
        Serial.println("Failed to connect");
        // ESP.restart();
    } 
    else {
        //if you get here you have connected to the WiFi    
        Serial.println("Connected to WiFi");
        
        // OTA Setup
        ArduinoOTA.setHostname("bontanic"); // Same hostname as above
        
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

    readHelloWorld();
}

void loop() {
    ArduinoOTA.handle(); // Handle OTA updates
    //wm.startConfigPortal(); // Blocks until configuration is done or timeout

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