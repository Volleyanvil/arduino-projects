/*
  Arduino/PlatformIO project implementing a basic air temperature and humidity sensor for indoor use.
  Data is transmitted using MQTT over WiFi/HTTP with support for secure connections (TLS) w/o client certificates.
  Implements MQTT Discovery protocol for automatic device discovery and configuration on supported platforms.

  Changes:

  Board(s):
    - Arduino MKR WiFi 1010 (platformio.ini env not configured)
    - Arduino Nano 33 IoT 
  Sensors:
    - SHT31 Temperature and humidity sensor
  Device designation:
    - Blue[A-Z]

  Author: Ilari Mattsson
  Project Nano IoT Simple Climate
  File: main.cpp
  Version: 1.0
*/

#include <Arduino.h>
#include <WiFiNINA.h>
#include <SHT31.h>
#include <Mqtt_Utility.h>
#include "arduino_secrets.h"


// ------- Globals --------------------------
// > Macros
#define CASE_LED 2

// > Secrets
char ssid[] = S_SSID;
char psk[] = S_PASS;
char host[] = S_MQTT_ADDR;
uint16_t port = S_MQTT_PORT;
char user[] = S_MQTT_USER;
char pass[] = S_MQTT_PASS;

// > Configuration variables
const uint32_t interval = 300000;
uint32_t previous = 0;
const uint16_t sensor_timeout = 3600;

// > Sensors
SHT31 sht31 = SHT31();
float temperature, humidity;

// > Calibration offsets
const float temperature_offset = 0, humidity_offset = 0;

// > Clients
// Uncomment only one Client
WiFiClient wifiClient;
//WiFiSSLClient wifiClient;
MqttUtility mqttUtility(wifiClient);

// > Sensor const variables
const char state_topic[] = "homeassistant/sensor/blueA/state";
const char device_name[] = "BlueA";
const char device_name_lower[] = "blueA";
const uint8_t num_of_sensors = 2;

// Homeassistant sensor device classes: https://www.home-assistant.io/integrations/sensor/#device-class
// Homeassistant JSON templating: https://www.home-assistant.io/docs/configuration/templating

// {{long name}, {short name}, {device class}, {unit}, {formatting}}
const char *sensors[5][num_of_sensors] = { 
  { "Temperature", "Humidity" },                     
  { "temp", "humi" },                                                      
  { "temperature", "humidity" },
  { "°C", "%" },
  { " | round(1)", " | round(1)" }
};


// ------- Function declarations ------------
void measureData();
void sendData();


// ------- Implementation -------------------
void setup() {
  pinMode(CASE_LED, OUTPUT);
  digitalWrite(CASE_LED, HIGH);
  delay(50);

  // Initialize sensors
  while(!sht31.begin()) delay(1000);

  // Initialize WiFi & MQTT
  mqttUtility.setWiFiNetwork(ssid, psk);
  mqttUtility.setMqttHost(host, port);
  mqttUtility.setWifiRetry(5);
  if (strlen(user) > 0 && strlen(pass) > 0) {
    mqttUtility.setMqttUser(user, pass);
  }
  delay(50);
  if (mqttUtility.begin() != CONN_CONNECTED) {
    while(1) {
      digitalWrite(CASE_LED, LOW);
      delay(800);
      digitalWrite(CASE_LED, HIGH);
      delay(200);
    }
  }
  delay(50);

  // Configure MQTT topics
  for (int i=0; i<num_of_sensors; i++) {
    String name = String(device_name) + " " + String(sensors[0][i]);
    String uid = String(device_name_lower) + String(sensors[1][i]);
    String val_tpl = String("{{ value_json.") + String(sensors[1][i]) + String(sensors[4][i]) + String(" }}");
    String conf_topic = "homeassistant/sensor/" + uid + "/config";
    mdevs dev = {String(sensors[2][i]), sensor_timeout, name, String(state_topic), uid, String(sensors[3][i]), val_tpl, conf_topic};
    mqttUtility.configureTopic(&dev);
  }
  
  digitalWrite(CASE_LED, LOW);
}


void loop() {
  mqttUtility.pollMqtt();
  unsigned long current = millis();
  
  if (current - previous >= interval) {
    previous = current;
    digitalWrite(CASE_LED, HIGH);
    mqttUtility.checkConnection();
    measureData();
    sendData();
    digitalWrite(CASE_LED, LOW);
  }
  delay(1000);
}


void measureData() {
  float temperature_raw, humidity_raw;

  do {
    temperature_raw = sht31.getTemperature();
    humidity_raw = sht31.getHumidity();
  } while (isnan(temperature_raw) || isnan(humidity_raw));

  temperature = temperature_raw + temperature_offset;
  humidity = humidity_raw + humidity_offset;

  return;
}


void sendData() {
    JsonDocument doc;
    doc[sensors[1][0]] = temperature;
    doc[sensors[1][1]] = humidity;
    int len = measureJson(doc);
    char output[len++];
    serializeJson(doc, output, len);
    mqttUtility.checkConnection();
    mqttUtility.sendPackets(doc, state_topic);
    return;
}
