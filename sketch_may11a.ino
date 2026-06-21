#include <math.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_BME280.h>
#include <Wire.h>
#include "thingProperties.h"  // For Arduino IoT Cloud

// WiFi credentials
const char* ssid     = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// Sensor setup
Adafruit_BME280 bme;
float seaLevelPressure = 1016;

// Sensor pins
const int relayPin = 26;  
const int voltageSensorPin = 32; 
float batteryVoltage = 0.0;
const float maxVoltage = 12.0;  
const float voltageDividerRatio = (3.3 / 4095.0) * (11.0 / 2.0); 

#define VOLTAGE_SENSOR_PIN 34
#define CURRENT_SENSOR_PIN 35

// Global variables

float pressure    = 0.0;
float windspeed   = 2.5; 


float powers[3] = {0.0, 0.0, 0.0};       // Example power values for delta calculation
float windSpeeds[3] = {0.0, 0.0, 0.0};   // Example wind speed history
float altitudes[3] = {0.0, 0.0, 0.0};    // Example altitude history

// Timing
unsigned long previousMillis = 0;
const long interval = 10000;  // 10 seconds

void setup() {
  
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);
  Serial.begin(115200);
  delay(100);

  // Arduino IoT Cloud setup
  initProperties();
  ArduinoCloud.begin(ArduinoIoTPreferredConnection);
  setDebugMessageLevel(2);
  ArduinoCloud.printDebugInfo();

  // Local WiFi connection for HTTP
  Serial.print("Connecting to WiFi ");
  Serial.print(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n Connected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // BME280 setup
  if (!bme.begin(0x76)) {
    Serial.println(" BME280 not found! Check wiring.");
    while (true) delay(10);
  }
  Serial.println(" BME280 initialized.");

  pinMode(2, OUTPUT); // For LED
  pinMode(4, OUTPUT); // Motor pins
  pinMode(5, OUTPUT);
}

void loop() {
  ArduinoCloud.update(); // Keep IoT Cloud updated

  readBME();
  readVoltage();
  readCurrentSensor();

  int sensorValue = analogRead(voltageSensorPin);
  batteryVoltage = sensorValue * voltageDividerRatio;

  Serial.print("Battery Voltage: ");
  Serial.println(batteryVoltage);

  if (batteryVoltage >= maxVoltage) {
    digitalWrite(relayPin, LOW);  // Stop charging
    Serial.println("Battery fully charged. Stopping charge.");
  } else {
    digitalWrite(relayPin, HIGH);  // Continue charging
    Serial.println("Charging...");
  }

  delay(1000); 

  
  // Shift historical data
  for (int i = 0; i < 2; i++) {
    powers[i] = powers[i+1];
    windSpeeds[i] = windSpeeds[i+1];
    altitudes[i] = altitudes[i+1];
  }

  powers[2] = voltage * current;
  
  windspeed = pow(voltage, 1.0/3.0) * 4.82;

  windSpeeds[2] = windspeed;
  altitudes[2] = altitude;

  unsigned long now = millis();
  if (now - previousMillis >= interval) {
    previousMillis = now;
    sendData();
    sendDataToServer();
    sendProcessedData();
  }
}

void readBME() {
  temperature = bme.readTemperature();
  pressure    = bme.readPressure() / 100.0F;
  humidity    = bme.readHumidity();
  altitude    = bme.readAltitude(seaLevelPressure);

  Serial.println("----- BME280 Readings -----");
  Serial.print("Temperature: "); Serial.println(temperature);
  Serial.print("Pressure: "); Serial.println(pressure);
  Serial.print("Humidity: "); Serial.println(humidity);
  Serial.print("Altitude: "); Serial.println(altitude);
}

void readVoltage() {
  int rawVoltage = analogRead(VOLTAGE_SENSOR_PIN);
  int voltage = (rawVoltage / 4095.0) * 19.7;
  Serial.print("Voltage: ");
  Serial.print(voltage);
  Serial.print(" V | Raw: ");
  Serial.println(rawVoltage);
}

void readCurrentSensor() {
  float current = readCurrent(CURRENT_SENSOR_PIN);  
  Serial.print("Current: ");
  Serial.print(current, 3);
  Serial.println(" A");
}

float readCurrent(int pin) {
  float sumCurrent = 0;
  const int samples = 1000;

  for (int i = 0; i < samples; i++) {
    int sensorValue = analogRead(pin);
    float voltage = (sensorValue * 3.3) / 4095.0;
    float current = (voltage - 3.3 / 2) / 0.185;  // Assuming ACS712-5A
    sumCurrent += current;
    delay(1);
  }
  return sumCurrent / samples;
}

// ------------------ DATA SENDING ------------------

void sendData() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected – skipping send.");
    return;
  }

  WiFiClient client;
  HTTPClient http;
  const char* url = "http://192.168.185.48:5000/data";

  Serial.print("[HTTP] Connecting to "); Serial.println(url);
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");

  String json = "{";
  json += "\"temperature\":" + String(temperature) + ",";
  json += "\"humidity\":" + String(humidity) + ",";
  json += "\"wind\":" + String(windspeed) + ",";
  json += "\"altitude\":" + String(altitude) + ",";
  json += "\"voltage\":" + String(voltage) + ",";
  json += "\"current\":" + String(current);
  json += "}";

  int httpResponseCode = http.POST(json);
  if (httpResponseCode > 0) {
    Serial.print("Data sent. Response: ");
    Serial.println(httpResponseCode);
  } else {
    Serial.print("Error sending data: ");
    Serial.println(httpResponseCode);
  }

  http.end();
}

void sendDataToServer() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  String url = "http://192.168.185.48:5000/data";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  String json = "{";
  json += "\"temperature\":" + String(temperature) + ",";
  json += "\"humidity\":" + String(humidity) + ",";
  json += "\"wind\":" + String(windspeed) + ",";
  json += "\"altitude\":" + String(altitude) + ",";
  json += "\"voltage\":" + String(voltage) + ",";
  json += "\"current\":" + String(current);
  json += "}";

  int response = http.POST(json);
  if (response > 0) {
    Serial.print("Data sent to server. Response: ");
    Serial.println(response);
  } else {
    Serial.print("Server error: ");
    Serial.println(response);
  }

  http.end();
}

void sendProcessedData() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  String url = "http://192.168.185.48:5000/processed";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  float powerChange = powers[2] - powers[1];

  String json = "{";
  json += "\"wind_speed\":" + String(windSpeeds[2]) + ",";
  json += "\"altitude\":" + String(altitudes[2]) + ",";
  json += "\"power_change\":" + String(powerChange);
  json += "}";

  int response = http.POST(json);
  if (response > 0) {
    Serial.print("Processed data sent. Response: ");
    Serial.println(response);
  } else {
    Serial.print("Error sending processed data: ");
    Serial.println(response);
  }

  http.end();
}


// ------------------ CONTROL ------------------

void controlMotor() {
  if (motor == true) {
    digitalWrite(4, HIGH);
    digitalWrite(5, HIGH);
  } else {
    digitalWrite(4, LOW);
    digitalWrite(5, LOW);
  }
}

void onMotorChange() {
  controlMotor();
}

void onLEDChange() {
  digitalWrite(2, led ? HIGH : LOW);
}


void onTemperatureChange() {
  Serial.println("Temperature changed!");
}