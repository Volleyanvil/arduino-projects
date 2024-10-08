/*
  Arduino/PlatformIO project implementing an environment and air quality monitoring sensor for indoor use.
  Data is transmission with MQTT over WiFi with support for TLS. Does not include client certificates support.
  Implements MQTT Discovery protocol for automatic device discovery and configuration for Home Assistant.

  Sensor calibration notes:
  - BME280 Temperature readout is consistently too high (~3,6'C) compared to a known good DHT22 sensor
  - BME280 Humidity readout is consistently too low (~14%) compared to a known good DHT22 sensor

  Changes:
  none

  Board(s):
    - Arduino MKR WiFi 1010 (platformio.ini env not configured)
    - Arduino Nano 33 IoT 
  Sensors:
    - DFRobot SEN0335 V2.0.0: ENS160 Air quality sensor + BME280 Environmental sensor
  Device designation:
    - Blue[A-Z]

  Author: Ilari Mattsson
  Project Nano IoT Indroor Air Sensor
  File: main.cpp
  Version: 1.0
*/

#include <Arduino.h>
#include <WiFiNINA.h>
#include <DFRobot_ENS160.h>
#include "DFRobot_BME280.h"
#include <Mqtt_Utility.h>
#include "arduino_secrets.h"

// ------- Globals ------------
// > Macros
#define CASE_LED 2 // LED_BUILTIN
#define ENS_ADDR 0x53
#define BME_ADDR 0x76

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
DFRobot_ENS160_I2C ens(&Wire, ENS_ADDR);
DFRobot_BME280_IIC bme(&Wire, BME_ADDR);
float temperature;
uint32_t humidity, pressure;
uint16_t volatite_organic_compounds, co2_concentration;
uint8_t air_quality_index, co2_level;

// > Calibration offsets
const float temperature_offset = -3.6;
const int humidity_offset = 14;

// > Clients
// Uncomment one wifiClient declaration (standard / SSL)
WiFiClient wifiClient;
//WiFiSSLClient wifiClient;
MqttUtility mqttUtility(wifiClient);

// > Sensor const variables
const char state_topic[] = "homeassistant/sensor/blueC/state";
const char device_name[] = "BlueC";
const char device_name_lower[] = "blueC";
const uint8_t num_of_sensors = 7;

// Homeassistant sensor device classes: https://www.home-assistant.io/integrations/sensor/#device-class
// Homeassistant JSON templating: https://www.home-assistant.io/docs/configuration/templating

const char *sensors[5][num_of_sensors] = { /*{{long name}, {short name}, {device class}, {unit}, {formatting}}*/
  { "Temperature", "Humidity", "Pressure", "AQI", "TVOC", "CO2 Concentration", "CO2 Level" },                     
  { "temp", "humi", "pres", "aqi", "tvoc", "co2c", "co2l" },                                                      
  { "temperature", "humidity", "pressure", "aqi", "volatile_organic_compounds_parts", "carbon_dioxide", "None" },
  { "°C", "%", "hPa", NULL, "ppb", "ppm", NULL },
  { " | round(1)", " | round(1)", " | float / 100 | round(2)" , "", "", "", "" }
};

// Function declarations
void measureData();
void sendData();

void setup() {
  pinMode(CASE_LED, OUTPUT);
  digitalWrite(CASE_LED, HIGH);
  delay(50);

  // Initialize sensors
  while (bme.begin() != DFRobot_BME280_IIC::eStatusOK) delay(2000);
  // while (!bme.init()) delay(1000);
  while (ens.begin() != NO_ERR) delay(3000);
  ens.setPWRMode(ENS160_STANDARD_MODE);  // ENS160_SLEEP_MODE | ENS160_IDLE_MODE | ENS160_STANDARD_MODE
  ens.setTempAndHum(bme.getHumidity() + humidity_offset, bme.getTemperature() + temperature_offset);

  // Initialize WiFi & MQTT
  mqttUtility.setWiFiNetwork(ssid, psk);
  mqttUtility.setMqttHost(host, port);
  mqttUtility.setWifiRetry(5);
  if (strlen(user) > 0 && strlen(pass) > 0) {
    mqttUtility.setMqttUser(user, pass);
    delay(50);
  }
  if(mqttUtility.begin() != CONN_CONNECTED) {
    while(1) {
      digitalWrite(CASE_LED, LOW);
      delay(800);
      digitalWrite(CASE_LED, HIGH);
      delay(200);
    }
  }
  delay(50);

  // Configure MQTT topics
  for (int i=0; i<num_of_sensors;i++) {
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
  /*uint8_t ensStatus = ens.getENS160Status();
  DFRobot_BME280::eStatus_t bmeStatus = bme.lastOperateStatus;
  if (bmeStatus != DFRobot_BME280::eStatusOK);*/

  float temp, humi;
  uint32_t pres;

  do {
    temp = bme.getTemperature();
    humi = bme.getHumidity();
    pres = bme.getPressure();
  } while (isnan(temp) || isnan(humi) || isnan(pres));

  temperature = temp + temperature_offset;
  humidity = humi + humidity_offset;
  pressure = bme.getPressure();

  uint16_t tvoc, eco2;
  uint8_t aqi;

  do {
    tvoc = ens.getTVOC();
    eco2 = ens.getECO2();
    aqi = ens.getAQI();
  } while (isnan(tvoc) || isnan(eco2) || isnan(aqi));

  volatite_organic_compounds = tvoc;
  co2_concentration = eco2;
  air_quality_index = aqi;

  if(co2_concentration < 600) co2_level = 1;
  else if (co2_concentration < 800) co2_level = 2;
  else if (co2_concentration < 1000) co2_level = 3;
  else if (co2_concentration < 1500) co2_level = 4;
  else co2_level = 5;

  return;
}

void sendData() {
    JsonDocument doc;
    doc[sensors[1][0]] = temperature;
    doc[sensors[1][1]] = humidity;
    doc[sensors[1][2]] = pressure;
    doc[sensors[1][3]] = air_quality_index;
    doc[sensors[1][4]] = volatite_organic_compounds;
    doc[sensors[1][5]] = co2_concentration;
    doc[sensors[1][6]] = co2_level;
    int len = measureJson(doc);
    char output[len++];
    serializeJson(doc, output, len);
    mqttUtility.checkConnection();
    mqttUtility.sendPackets(doc, state_topic);
    return;
}
