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
#include <Arduino_JSON.h>
#include <esp_system.h>
#define ENABLE_GxEPD2_GFX 0
#include <GxEPD2_BW.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <DHT.h>
#include "time.h"

// Add these forward declarations after the includes and before any other code
void addDataPoint(float temp, float humid, float soil);
void processAverages(float temperature, float humidity, float soil);
void rotateFile();

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

GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display(GxEPD2_154_D67(/*CS=*/ 27, /*DC=*/ 14, /*RST=*/ 12, /*BUSY=*/ 13)); // GDEP015OC1 200x200, IL3829

#define DHTPIN 25
#define DHTTYPE DHT22
#define SOIL_PIN 36

DHT dht(DHTPIN, DHTTYPE);

// Add these global variables for tracking changes
float lastTemp = 0;
float lastHumidity = 0;
int lastSoilMoisture = 0;
unsigned long lastDisplayUpdate = 0;
const unsigned long DISPLAY_UPDATE_INTERVAL = 60 * 1000;  // 1 minute

// Update timezone settings for Berlin
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;     // CET (UTC+1)
const int daylightOffset_sec = 3600;  // CEST adds +1 hour during summer

// Constants for data storage
const char* DATA_FILE = "/sensor_data.csv";
const size_t MAX_FILE_SIZE = 1024 * 1024;  // 1MB
const size_t MAX_DATA_LINES = 1440;  // 24 hours of minute data

const unsigned long READING_AVERAGING_WINDOW = 60 * 1000; // 1 minute

// Sensor thresholds
const float TEMP_THRESHOLD = 0.2;    // 0.2°C change
const float HUMID_THRESHOLD = 2.0;   // 2% humidity change
const float SOIL_THRESHOLD = 1.0;    // 1% soil moisture change

// Add these new variables for averaging (keeping all existing variables unchanged)
struct SensorAverages {
    float tempSum = 0;
    float humidSum = 0;
    float soilSum = 0;
    int count = 0;
};

SensorAverages averages;
unsigned long lastAverageStore = 0;


// Add these variables for tracking last stored values
float lastStoredTemp = 0;
float lastStoredHumidity = 0;
float lastStoredSoil = 0;

// Updated 24x24 icon definitions
const unsigned char thermometer_icon[] PROGMEM = {
    0x00, 0x1E, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x61, 0x80, 0x00, 0x61, 0x80,
    0x00, 0x61, 0x80, 0x00, 0x61, 0x80, 0x00, 0x61, 0x80, 0x00, 0x73, 0x80,
    0x00, 0x73, 0x80, 0x00, 0x73, 0x80, 0x00, 0x73, 0x80, 0x00, 0x73, 0x80,
    0x00, 0xFF, 0x80, 0x00, 0xFF, 0x80, 0x00, 0xFF, 0x80, 0x00, 0xFF, 0x80,
    0x00, 0x7F, 0x00, 0x00, 0x3E, 0x00, 0x00, 0x1C, 0x00, 0x00, 0x08, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

const unsigned char humidity_icon[] PROGMEM = {
    0x00, 0x18, 0x00, 0x00, 0x3C, 0x00, 0x00, 0x7E, 0x00, 0x00, 0xFF, 0x00,
    0x01, 0xFF, 0x80, 0x03, 0xFF, 0xC0, 0x07, 0xFF, 0xE0, 0x07, 0xFF, 0xE0,
    0x0F, 0xFF, 0xF0, 0x0F, 0xFF, 0xF0, 0x0F, 0xFF, 0xF0, 0x0F, 0xFF, 0xF0,
    0x0F, 0xFF, 0xF0, 0x07, 0xFF, 0xE0, 0x07, 0xFF, 0xE0, 0x03, 0xFF, 0xC0,
    0x03, 0xFF, 0xC0, 0x01, 0xFF, 0x80, 0x00, 0xFF, 0x00, 0x00, 0x7E, 0x00,
    0x00, 0x3C, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// Updated 24x24 soil icon to a plant design
const unsigned char soil_icon[] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// Add near other global variables
struct DataPoint {
    uint32_t timestamp;
    float temperature;
    float humidity;
    float soil;
};

const size_t BUFFER_SIZE = 1440;  // Store 24 hours worth of minute data
DataPoint dataBuffer[BUFFER_SIZE];
size_t bufferIndex = 0;
size_t totalStoredPoints = 0;



// Add this to your setup function
void setupStorage() {
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS Mount Failed");
        return;
    }
    
    // Create file with headers if it doesn't exist
    if (!LittleFS.exists(DATA_FILE)) {
        File file = LittleFS.open(DATA_FILE, "w");
        if (file) {
            file.println("Timestamp,Temperature,Humidity,Soil");
            file.close();
        }
    }
}

// Modify your existing data collection to use the new storage
void handleSensorData(float temp, float humid, float soil) {
    addDataPoint(temp, humid, soil);
}

// First declare displayTimeBar
void displayTimeBar() {
    display.setRotation(1);
    display.setFont(&FreeMonoBold9pt7b);
    display.setTextSize(1);  // Reset text size to normal for time
    
    // Draw black top bar
    display.fillRect(0, 0, display.width(), 20, GxEPD_BLACK);
    
    // Get current time
    struct tm timeinfo;
    if(getLocalTime(&timeinfo)) {
        char timeString[20];
        strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M", &timeinfo);
        
        // Display time in white
        display.setTextColor(GxEPD_WHITE);
        
        // Center the time text
        int16_t tbx, tby;
        uint16_t tbw, tbh;
        display.getTextBounds(timeString, 0, 0, &tbx, &tby, &tbw, &tbh);
        uint16_t x = ((display.width() - tbw) / 2) - tbx;
        
        display.setCursor(x, 15);
        display.print(timeString);
    }
}

// Then declare displayStatusBar
void displayStatusBar() {
    display.setRotation(1);
    display.setFont(&FreeMonoBold9pt7b);
    display.setTextSize(1);  // Reset text size to normal for IP
    
    // Draw black bottom bar
    display.fillRect(0, display.height() - 20, display.width(), 20, GxEPD_BLACK);
    
    // Display IP Address or status in white
    display.setTextColor(GxEPD_WHITE);
    if(WiFi.status() == WL_CONNECTED) {
        String ip = WiFi.localIP().toString();
        display.setCursor(5, display.height() - 5);
        display.print(ip);
        
        // Draw online indicator (small circle)
        display.fillCircle(190, display.height() - 10, 5, GxEPD_WHITE);
    } else {
        display.setCursor(5, display.height() - 5);
        display.print("Connecting...");
    }
}

// Then declare displayMessage
void displayMessage(const char* message)
{
    display.setRotation(1);
    display.setFont(&FreeMonoBold9pt7b);
    
    display.setFullWindow();
    display.firstPage();
    do
    {
        display.fillScreen(GxEPD_WHITE);
        displayTimeBar();  // Draw time bar at top
        displayStatusBar();  // Draw status bar at bottom
        
        // Set text color to black for main content
        display.setTextColor(GxEPD_BLACK);
        
        // Print each line
        int yPosition = 50;  // Start below top status bar
        String messageStr = String(message);
        int start = 0;
        int newline;
        
        while (start < messageStr.length()) {
            newline = messageStr.indexOf('\n', start);
            String line;
            if (newline == -1) {
                line = messageStr.substring(start);
                start = messageStr.length();
            } else {
                line = messageStr.substring(start, newline);
                start = newline + 1;
            }
            
            // Center each line
            int16_t tbx, tby;
            uint16_t tbw, tbh;
            display.getTextBounds(line.c_str(), 0, 0, &tbx, &tby, &tbw, &tbh);
            uint16_t x = ((display.width() - tbw) / 2) - tbx;
            
            // Ensure text doesn't overlap with bottom status bar
            if (yPosition < (display.height() - 30)) {
                display.setCursor(x, yPosition);
                display.print(line);
                yPosition += 20;
            }
        }
    }
    while (display.nextPage());
}

// Finally declare displaySensorData
void displaySensorData() {
    if ((millis() - lastDisplayUpdate) < DISPLAY_UPDATE_INTERVAL) return;
    
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();
    int soil = analogRead(SOIL_PIN);
    
    if (isnan(temperature) || isnan(humidity)) return;
    
    display.setRotation(1);
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        
        // Draw time bar at top
        displayTimeBar();
        
        // Set text properties for sensor readings
        display.setFont(&FreeMonoBold18pt7b);
        display.setTextColor(GxEPD_BLACK);
        display.setTextSize(1);
        
        // Temperature section
        display.drawBitmap(20, 40, thermometer_icon, 24, 24, GxEPD_BLACK);
        display.setCursor(50, 55);
        
        // Split temperature into integer and decimal parts
        int tempInt = (int)temperature;
        int tempDec = (int)((temperature - tempInt) * 10);
        
        // Print integer part
        display.print(tempInt);
        
        // Print decimal part at half size
        display.setFont(&FreeMonoBold12pt7b);
        display.print(".");
        display.print(tempDec);
        
        display.print(" "); 
        
        // Draw a small circle for degrees (4 pixel radius)
        display.fillCircle(display.getCursorX(), display.getCursorY() - 12, 4, GxEPD_BLACK);
        display.setCursor(display.getCursorX() + 10, display.getCursorY());
        
        // Print C
        display.setFont(&FreeMonoBold18pt7b);
        display.print("C");
        
        // Humidity section
        display.drawBitmap(20, 80, humidity_icon, 24, 24, GxEPD_BLACK);
        display.setCursor(50, 95);
        display.print(String(humidity, 0));
        display.print("%");
        
        // Soil moisture section
        display.drawBitmap(20, 120, soil_icon, 24, 24, GxEPD_BLACK);
        display.setCursor(50, 135);
        int soilPercent = map(soil, 4095, 0, 0, 100);
        display.print(soilPercent);
        display.print("%");
        
        // Draw status bar at bottom
        displayStatusBar();
        
    } while (display.nextPage());
    
    lastDisplayUpdate = millis();
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

// Add this before handleWebSocketMessage function
String getSensorReadings() {
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();
    int soilMoisture = analogRead(SOIL_PIN);
    
    if (isnan(temperature) || isnan(humidity)) {
        Serial.println("Failed to read from DHT sensor!");
        return "";
    }

    bool hasChanged = false;
    
    if (abs(temperature - lastTemp) > TEMP_THRESHOLD) {
        readings["temperature"] = String(temperature, 1);
        lastTemp = temperature;
        hasChanged = true;
    }
    
    if (abs(humidity - lastHumidity) > HUMID_THRESHOLD) {
        readings["humidity"] = String(humidity, 1);
        lastHumidity = humidity;
        hasChanged = true;
    }
    
    int soilPercent = map(soilMoisture, 4095, 0, 0, 100);
    if (abs(soilPercent - lastSoilMoisture) > SOIL_THRESHOLD) {
        readings["soil"] = String(soilPercent);
        lastSoilMoisture = soilPercent;
        hasChanged = true;
    }
    
    // Always include ESP32 stats
    readings["heap"] = String(ESP.getFreeHeap() / 1024);       // Convert to KB
    readings["cpu"] = String(ESP.getCpuFreqMHz());            // MHz
    readings["flash"] = String(ESP.getFlashChipSize() / (1024 * 1024)); // Convert to MB
    readings["sketch"] = String(ESP.getSketchSize() / 1024);   // Convert to KB
    readings["freespace"] = String(ESP.getFreeSketchSpace() / 1024);  // Convert to KB
    
    if (hasChanged) {
        processAverages(temperature, humidity, soilPercent);
    }

    return JSON.stringify(readings);
}

// Add this new function for handling averages
void processAverages(float temperature, float humidity, float soil) {
    if (!isnan(temperature) && !isnan(humidity)) {
        averages.tempSum += temperature;
        averages.humidSum += humidity;
        averages.soilSum += soil;
        averages.count++;
        
        // Check if it's time to calculate the average (every minute)
        if (millis() - lastAverageStore >= READING_AVERAGING_WINDOW) {
            float avgTemp = averages.tempSum / averages.count;
            float avgHumid = averages.humidSum / averages.count;
            float avgSoil = averages.soilSum / averages.count;
            
            // Only store if there's a significant change from last stored values
            bool shouldStore = false;
            
            if (abs(avgTemp - lastStoredTemp) >= TEMP_THRESHOLD ||
                abs(avgHumid - lastStoredHumidity) >= HUMID_THRESHOLD ||
                abs(avgSoil - lastStoredSoil) >= SOIL_THRESHOLD) {
                
                shouldStore = true;
                lastStoredTemp = avgTemp;
                lastStoredHumidity = avgHumid;
                lastStoredSoil = avgSoil;
                
                // Store the averages in CSV format
                addDataPoint(avgTemp, avgHumid, avgSoil);
            }
            
            // Reset the averages
            averages.tempSum = 0;
            averages.humidSum = 0;
            averages.soilSum = 0;
            averages.count = 0;
            lastAverageStore = millis();
            
            if (shouldStore) {
                Serial.println("Stored new data point due to significant change");
            }
        }
    }
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

// Add this before the AsyncWebServer server(80); line
void setupDataEndpoint(AsyncWebServer *server) {
    server->on("/downloadcsv", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!LittleFS.exists(DATA_FILE)) {
            request->send(404, "text/plain", "No data available");
            return;
        }

        AsyncWebServerResponse *response = request->beginResponse(LittleFS, DATA_FILE, "text/csv");
        response->addHeader("Content-Disposition", "attachment; filename=sensor_data.csv");
        response->addHeader("Access-Control-Allow-Origin", "*");
        request->send(response);
    });
}

void setup() {
    Serial.begin(115200);
    
    // Initialize LittleFS first, before anything else
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS Mount Failed");
    }
    
    // Initialize sensors
    dht.begin();
    pinMode(SOIL_PIN, INPUT);
    
    // Initialize storage system
    setupStorage();
    
    // Initialize display early
    display.init(115200, true, 2, false);
    
    // Take initial reading before WiFi setup
    delay(2000);  // Give sensors time to stabilize
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();
    int soil = analogRead(SOIL_PIN);
    
    if (!isnan(temperature) && !isnan(humidity)) {
        // Update the last known values
        lastTemp = temperature;
        lastHumidity = humidity;
        lastSoilMoisture = map(soil, 4095, 0, 0, 100);
        
        // Display the initial reading
        displaySensorData();
    } else {
        displayMessage("Sensor Error!");
    }
    
    // Set device hostname
    WiFi.setHostname("bontanic");
    
    bool res = wm.autoConnect("AutoConnectAP");
    if(!res) {
        // Only update status bar with error
        displayStatusBar();
    } else {
        // Update status bar with IP and refresh the sensor data display
        displayStatusBar();
        displaySensorData();  // Refresh the entire display after connection
        
        // Continue with remaining setup...
        ArduinoOTA.setHostname("bontanic");
        ArduinoOTA.setMdnsEnabled(false); 
        
        ArduinoOTA.begin();
        Serial.println("OTA Ready");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());

        // Initialize and get the time
        configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    }

    initLittleFS();
    initWebSocket();

    // Web Server Root URL
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/index.html", "text/html");
    });

    server.serveStatic("/", LittleFS, "/");

    setupDataEndpoint(&server);  // Add this line here

    // Start server
    server.begin();

    readHelloWorld();
}

void loop() {
    ArduinoOTA.handle();  // This must stay!
    
    if ((millis() - lastTime) > timerDelay) {
        String readings = getSensorReadings();
        
        // Add this line to process averages using the current readings
        if (readings.length() > 0) {
            float temp = dht.readTemperature();
            float humid = dht.readHumidity();
            int soil = analogRead(SOIL_PIN);
            processAverages(temp, humid, soil);
        }
        
        if (readings.length() > 0) {  // Only send if there are changes
            notifyClients(readings);
            displaySensorData();  // Will only update if 30s have passed
        }
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

void addDataPoint(float temp, float humid, float soil) {
    if (isnan(temp) || isnan(humid)) {
        Serial.println("Invalid sensor readings");
        return;
    }

    File file = LittleFS.open(DATA_FILE, "a");
    if (!file) {
        Serial.println("Failed to open file for writing");
        return;
    }

    // Get current time
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time");
        file.close();
        return;
    }

    // Check file size before writing
    if (file.size() >= MAX_FILE_SIZE) {
        file.close();
        rotateFile();
        file = LittleFS.open(DATA_FILE, "a");
    }

    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M", &timeinfo);
    
    // Write data in CSV format
    file.printf("%s,%.2f,%.2f,%.2f\n", timestamp, temp, humid, soil);
    file.close();
}

void rotateFile() {
    File oldFile = LittleFS.open(DATA_FILE, "r");
    if (!oldFile) {
        Serial.println("Failed to open file for rotation");
        return;
    }

    // Create temporary file
    File tempFile = LittleFS.open("/temp.csv", "w");
    if (!tempFile) {
        Serial.println("Failed to create temp file");
        oldFile.close();
        return;
    }

    // Write header
    tempFile.println("Timestamp,Temperature,Humidity,Soil");

    // Count lines and store last MAX_DATA_LINES
    String lines[MAX_DATA_LINES];
    size_t lineCount = 0;
    
    // Skip header
    oldFile.readStringUntil('\n');

    // Read all lines
    while (oldFile.available()) {
        String line = oldFile.readStringUntil('\n');
        if (line.length() > 0) {
            lines[lineCount % MAX_DATA_LINES] = line;
            lineCount++;
        }
    }

    // Write only the last MAX_DATA_LINES lines
    size_t startIdx = (lineCount > MAX_DATA_LINES) ? (lineCount % MAX_DATA_LINES) : 0;
    size_t numLines = min(lineCount, MAX_DATA_LINES);
    
    for (size_t i = 0; i < numLines; i++) {
        size_t idx = (startIdx + i) % MAX_DATA_LINES;
        tempFile.println(lines[idx]);
    }

    oldFile.close();
    tempFile.close();

    // Replace old file with new file
    LittleFS.remove(DATA_FILE);
    LittleFS.rename("/temp.csv", DATA_FILE);
}