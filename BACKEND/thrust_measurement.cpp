#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <SD.h>
#include <SPI.h>
#include <WiFi.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>

#define LOAD_CELL_DOUT 2
#define LOAD_CELL_SCK 4
#define CS_PIN 5
 
// Network credentials
const char* ssid = "YourWiFiName";
const char* password = "YourWiFiPassword";
unsigned long testStartTime=0;
unsigned long testDuration=0;
bool testRunning=false;

// WebSocket server on port 81
WebSocketsServer webSocket = WebSocketsServer(81);
Adafruit_BMP280 bmp;
File dataFile;

// Add at the top with other global variables
StaticJsonDocument<200> jsonDoc;
char jsonBuffer[200];

struct RocketData {
    float thrust;
    float height;
    float pressure;
    float temperature;
    unsigned long timestamp;
    float stability;
    unsigned long testTime;
    bool isTestActive;
} rocketData;

float initialHeight = 0;
float maxHeight = 0;
unsigned long startTime = 0;
unsigned long thrustDuration = 0;
bool isThrusting = false;
float lastThrust = 0;
const float THRUST_THRESHOLD = 0.5; // Newtons
const int STABILITY_SAMPLES = 10;
float thrustReadings[STABILITY_SAMPLES];
int readingIndex = 0;

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("[%u] Disconnected!\n", num);
            break;
        case WStype_CONNECTED:
            Serial.printf("[%u] Connected!\n", num);
            break;
            case WStype_TEXT:
            String command = String((char*)payload);
            if(command == "START_TEST") {
                testStartTime = millis();
                testRunning = true;
            } else if(command == "STOP_TEST") {
                testRunning = false;
                testDuration = millis() - testStartTime;
            }
            break;
    }
}

void setup() {
    Serial.begin(115200);
    Wire.begin();

    // Connect to WiFi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    // Start WebSocket server
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);

    // Initialize BMP280 sensor
    if (!bmp.begin(0x76)) {
        Serial.println("BMP280 initialization failed!"); // Error handling for BMP280 initialization
        while (1); // Halt execution if sensor initialization fails
    }

    // Initialize SD card
    if (!SD.begin(CS_PIN)) {
        Serial.println("SD card initialization failed!"); // Error handling for SD card initialization
        while (1); // Halt execution if SD card initialization fails
    }

    // Create new file with incremental name and check for errors
    String fileName = "/flight_data_";
    int fileNumber = 0;
    while (SD.exists((fileName + fileNumber + ".csv").c_str())) {
        fileNumber++;
    }
    dataFile = SD.open(fileName + fileNumber + ".csv", FILE_WRITE); // Open file for writing
    if (!dataFile) {
        Serial.println("Failed to open file for writing!"); // Error handling for file opening
        while (1); // Halt execution if file opening fails
    }
    dataFile.println("Time,Thrust,Height,Pressure,Temperature,Stability");

    // Get initial height reading
    initialHeight = bmp.readAltitude(1013.25);
    startTime = millis();
}

float calculateStability() {
    float sum = 0;
    float mean = 0;
    float variance = 0;

    // Calculate mean
    for (int i = 0; i < STABILITY_SAMPLES; i++) {
        sum += thrustReadings[i];
    }
    mean = sum / STABILITY_SAMPLES;

    // Calculate variance
    for (int i = 0; i < STABILITY_SAMPLES; i++) {
        variance += pow(thrustReadings[i] - mean, 2);
    }
    variance /= STABILITY_SAMPLES;

    return sqrt(variance); // Return standard deviation as stability measure
}

void loop() {
    webSocket.loop(); // Handle WebSocket events
    unsigned long currentTime = millis() - startTime; // Calculate current time
    unsigned long testTime = testRunning ? (millis() - testStartTime) : testDuration; // Calculate test time

    // Read sensor data
    float currentThrust = analogRead(A0) * (50.0 / 4095.0); // Read thrust value
    //return sqrt(variance); // Return standard deviation as stability measure
}

void loop() {
    webSocket.loop(); // Handle WebSocket events
    unsigned long currentTime = millis() - startTime; // Calculate current time
    unsigned long testTime = testRunning ? (millis() - testStartTime) : testDuration; // Calculate test time



    
    // Read sensor data

    float currentThrust = analogRead(A0) * (50.0 / 4095.0); // Read thrust value
    float currentHeight = bmp.readAltitude(1013.25) - initialHeight; // Read height value
    float currentPressure = bmp.readPressure(); // Read pressure value
    float currentTemp = bmp.readTemperature(); // Read temperature value



    // Update thrust readings array for stability calculation
    thrustReadings[readingIndex] = currentThrust;
    readingIndex = (readingIndex + 1) % STABILITY_SAMPLES;

    // Calculate stability
    float stability = calculateStability();

    // Detect thrust start/end
    if (!isThrusting && currentThrust > THRUST_THRESHOLD) {
        isThrusting = true;
        startTime = millis();
    } else if (isThrusting && currentThrust < THRUST_THRESHOLD) {
        isThrusting = false;
        thrustDuration = millis() - startTime;
    }

    // Update max height
    if (currentHeight > maxHeight) {
        maxHeight = currentHeight;
    }

    jsonDoc.clear();
    jsonDoc["time"] = currentTime;
    jsonDoc["stability"] = stability;
    jsonDoc["testtime"] = testTime;
    jsonDoc["testRunning"] = testRunning;



    // Serialize JSON to buffer
    serializeJson(jsonDoc, jsonBuffer);

    // Send data through WebSocket
    webSocket.broadcastTXT(jsonBuffer);

    // Log data to SD card
    dataFile.printf("%lu,%.2f,%.2f,%.2f,%.2f,%.2f\n",
                    currentTime,
                    currentThrust,
                    currentHeight,
                    currentPressure,
                    currentTemp,
                    stability);

    // Print data to Serial for debugging
    Serial.printf("Time: %lu ms, Thrust: %.2f N, Height: %.2f m, Stability: %.2f\n",
                  currentTime,
                  currentThrust,
                  currentHeight,
                  currentPressure,
                  currentTemp,
                  stability);

    delay(100); // 10Hz sampling rate

    // Flush data to SD card every second
    if (currentTime % 1000 == 0) {
        dataFile.flush();
    }
}
