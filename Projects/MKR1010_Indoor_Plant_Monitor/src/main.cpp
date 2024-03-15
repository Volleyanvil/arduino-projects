/*
  Arduino/PlatformIO project implementing a plant monitoring sensor.
  Data is transmission with MQTT over WiFi. Supports TLS. Does not support client certificates.
  Implements MQTT Discovery protocol for automatic device discovery and configuration for Home Assistant.

  Changes:
  [1.1] --------------
  > Improvemets to dynamic moisture sensor configuration in setup()
    - Sensor specific strings are now placed in char arrays before assigning to mdev to avoid an issue with serializeJson() and char* assignment
    - Constant portions of concatenated strings are now assigned to const char arrays within scope for better parameter readability
    - Removed helper functions from string concatenation. Now calls snprintf directly instead
    - Removed static sized mdev definition and related methods from local Mqtt Utility implementation
  [1.2] --------------
  > Another fix to moisture sensor configuration
   i: Issue with serializeJson() persisted in some cases after fix in 1.1
    - Copying mdev pointer contents to method-scoped char arrays before assigning to JsonDocument prevents the issue
    - Improved some variable naming in setup() (dht json docs, moisture sensor char arrays)
  [1.3] --------------
  > Critical bug fix
    - Moisture sensor calibration cap values were being incorrectly saved to base

  Board(s):
    - Arduino MKR WiFi 1010
    - Arduino Nano 33 IoT (platformio.ini env not configured)
  Sensors:
    - Up to 7 analog soil moisture sensors, capacitive or resistive
    - DHT22 Temperature & Humidity sensor (optional)
  Device designation:
    - GreenB

  Author: Ilari Mattsson
  Project MKR1010_Indoor_Plant_Monitor
  File: main.cpp
  Version: 1.2
*/

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ArduinoMqttClient.h>
#include <DHT.h>
#include <WiFiNINA.h>
#include <utility/wifi_drv.h>
#include "arduino_secrets.h"
#include "local_utils.h"
#include <Mqtt_Utility.h>

// ------- Globals ------------
// > Pins
#define MST_COUNT 4 // Number of moisture sensors (only used for validation)
#define MST_PIN_1 A0
#define MST_PIN_2 A1
#define MST_PIN_3 A2
#define MST_PIN_4 A3
// #define MST_PIN_5 A4
// #define MST_PIN_6 A5
// #define MST_PIN_7 A6
#define CASE_LED LED_BUILTIN
#define DHTPIN 1
#define TOUCH_PIN 2
#define RGB_R_PIN 25
#define RGB_G_PIN 26
#define RGB_B_PIN 27
// > Configuration
const bool is_capacitive = true;
const long interval = 300000;
unsigned long previous = 0;
bool is_rgb_set = false;
const unsigned short sensor_timeout = 3600;
// > Sensors
mst_sen_arr* mst_arr;
int mst_arr_size;
DHT dht (DHTPIN, DHT22);
float temp, hum;

// ------- WiFi & MQTT --------
// > Secrets (include/arduino_secrets.h)
char ssid[] = S_SSID;
char psk[] = S_PASS;
char host[] = S_MQTT_ADDR;
uint16_t port = S_MQTT_PORT;
char user[] = S_MQTT_USER;
char pass[] = S_MQTT_PASS;
// > MQTT topics
const char state_topic[] = "homeassistant/sensor/greenB/state";
// > Global Classes
WiFiClient wifiClient;
// WiFiSSLClient wifiClient;
MqttClient mqttClient(wifiClient);
MqttUtility mqttUtil(wifiClient, mqttClient, ssid, psk, host, port);

// Function declarations
//
void measureData();
void sendData();
int setMoistureCap(uint8_t, bool);
int setMoistureBase(uint8_t, bool);
void rgbLed(uint8_t, uint8_t, uint8_t);
void makeSenArray();

void setup() {
  pinMode(CASE_LED, OUTPUT);
  pinMode(TOUCH_PIN, INPUT);
  digitalWrite(CASE_LED, HIGH);
  delay(50);

  makeSenArray();
  delay(50);

  // mqttUtil.setMqttUser(user, pass);
  delay(50);
  if(mqttUtil.init() != CONN_CONNECTED) while(1);
  delay(50);

  int r_max, g_max, b_max;
  // Calibration - Base(Dry)
  rgbLed(100, 50, 0);
  r_max = 140;
  g_max = 70;
  b_max = 0;
  while(digitalRead(TOUCH_PIN) != HIGH);
  for (int i = 0; i < mst_arr_size; i++){
    rgbLed(r_max*(i+1)/mst_arr_size, g_max*(i+1)/mst_arr_size, b_max*(i+1)/mst_arr_size);
    (*mst_arr)[i].base = setMoistureBase((*mst_arr)[i].pin, is_capacitive);
    delay(100);
  }
  delay(50);
  rgbLed(0, 100, 0);
  delay(2000);
  rgbLed(0, 0, 0);
  delay(50);

  // Calibration - Cap(Wet)
  rgbLed(0, 100, 100);
  r_max = 0;
  g_max = 140;
  b_max = 140;
  while(digitalRead(TOUCH_PIN) != HIGH);
  for (int i = 0; i < mst_arr_size; i++){
    rgbLed(r_max*(i+1)/mst_arr_size, g_max*(i+1)/mst_arr_size, b_max*(i+1)/mst_arr_size);
    (*mst_arr)[i].cap = setMoistureCap((*mst_arr)[i].pin, is_capacitive);
    delay(100);
  }
  delay(50);
  rgbLed(0, 100, 0);
  delay(2000);
  rgbLed(0, 0, 0);
  delay(50);

  for (int i = 0; i < mst_arr_size; i++){
    const char name_h[]="Green B Soil Moisture", id_h[]="greenBsoil", val_h[]="{{ value_json.", val_t[]=" }}", conf_h[]="homeassistant/sensor/greenBM", conf_t[]="/config";

    char conf_topic[strlen(conf_h) + strlen((*mst_arr)[i].id) + strlen(conf_t) + 1];
    snprintf(conf_topic, strlen(conf_h) + strlen((*mst_arr)[i].id) + strlen(conf_t) + 1, "%s%s%s", conf_h, (*mst_arr)[i].id, conf_t);

    char name[strlen(name_h) + strlen((*mst_arr)[i].id) + 1];
    snprintf(name, strlen(name_h) + strlen((*mst_arr)[i].id) + 1, "%s%s", name_h, (*mst_arr)[i].id);

    char uniq_id[strlen(id_h) + strlen((*mst_arr)[i].id) + 1];
    snprintf(uniq_id, strlen(id_h) + strlen((*mst_arr)[i].id) + 1, "%s%s", id_h, (*mst_arr)[i].id);

    char val_tpl[strlen(val_h) + strlen((*mst_arr)[i].val_id) + strlen(val_t) + 1];
    snprintf(val_tpl, strlen(val_h) + strlen((*mst_arr)[i].val_id) + strlen(val_t) + 1, "%s%s%s", val_h, (*mst_arr)[i].val_id, val_t);

    mdev dev = { conf_topic, "moisture", sensor_timeout, name, state_topic, uniq_id, "%", val_tpl };
    mqttUtil.configureTopic(&dev);
  }

  #ifdef DHTPIN
  dht.begin();
  const char dht_temp_conf_t[] = "homeassistant/sensor/greenBT/config";
  const char dht_hum_conf_t[] = "homeassistant/sensor/greenBH/config";
  mdev dht_t_dev = { dht_temp_conf_t, "temperature", sensor_timeout, "GreenB Air Temperature", state_topic, "greenBtemp", "°C", "{{ value_json.temp | round(2) }}" };
  mqttUtil.configureTopic(&dht_t_dev);
  mdev dht_h_dev = { dht_hum_conf_t, "humidity", sensor_timeout, "GreenB Air Humidity", state_topic, "greenBhum", "%", "{{ value_json.hum | round(1) }}" };
  mqttUtil.configureTopic(&dht_h_dev);
  #endif
  
  delay(50);
  digitalWrite(CASE_LED, LOW);
}

void loop() {
  mqttUtil.pollMqtt();
  unsigned long current = millis();
  
  if (current - previous >= interval) {
    previous = current;
    digitalWrite(CASE_LED, HIGH);
    Serial.println();
    measureData();
    sendData();
    digitalWrite(CASE_LED, LOW);
  }
  delay(1000);
}


// Function definitions
//
void measureData() {
  short lp = 40;
  int raw;
  for(int k=0; k < mst_arr_size; k++){
    (*mst_arr)[k].sum = 0;
  }

  for(int i=0; i<lp; i++){
    for(int k=0; k < mst_arr_size; k++){
      // Constrain values to avoid mapping issues
      raw = constrain(analogRead((*mst_arr)[k].pin), (*mst_arr)[k].cap, (*mst_arr)[k].base);
      (*mst_arr)[k].sum += raw;
    }
    delay(100);
  }
  
  for(int k=0; k < mst_arr_size; k++){
    (*mst_arr)[k].val = map((*mst_arr)[k].sum/lp, (*mst_arr)[k].cap, (*mst_arr)[k].base, 100, 0);
  }

  do {
    temp = dht.readTemperature();
    hum = dht.readHumidity();
  } while (isnan(temp) || isnan(hum));
  return;
}

void sendData() {
    JsonDocument doc;
    for(int i=0; i<mst_arr_size; i++){
      doc[(*mst_arr)[i].val_id] = (*mst_arr)[i].val;
    }
    #ifdef DHTPIN
    doc["temp"] = temp;
    doc["hum"] = hum;
    #endif
    int len = measureJson(doc);
    char output[len++];
    serializeJson(doc, output, len);
    mqttUtil.checkConnection();
    mqttUtil.sendPackets(doc, state_topic);
    return;
}

int setMoistureCap(uint8_t sensor_pin, bool is_capacitive){
  unsigned long start = millis();
  int moisture_cap;
  
  if(is_capacitive){
    moisture_cap = 0;
    while(millis() - start < 5000){
      int mst = analogRead(sensor_pin);
      if(mst > moisture_cap){
        moisture_cap = mst;
      }
      delay(100);
    }
  } else {
    moisture_cap = 2000;
    while(millis() - start < 5000){
      int mst = analogRead(sensor_pin);
      if(mst < moisture_cap){
        moisture_cap = mst;
      }
      delay(100);
    }
  }
  return moisture_cap;
}


int setMoistureBase(uint8_t sensor_pin, bool is_capacitive){
  unsigned long start = millis();
  int moisture_base;
  
  if(is_capacitive){
    moisture_base = 2000;
    while(millis() - start < 5000){
      int mst = analogRead(sensor_pin);
      if(mst < moisture_base){
        moisture_base = mst;
      }
      delay(100);
    }
  } else {
    moisture_base = 0;
    while(millis() - start < 5000){
      int mst = analogRead(sensor_pin);
      if(mst > moisture_base){
        moisture_base = mst;
      }
      delay(100);
    }
  }
  return moisture_base;
}

/**
 * WiFiNINA boards: control built-in RGB LED
*/
void rgbLed(uint8_t r, uint8_t g, uint8_t b) {
  if(!is_rgb_set){
    WiFiDrv::pinMode(RGB_R_PIN, OUTPUT);
    WiFiDrv::pinMode(RGB_G_PIN, OUTPUT);
    WiFiDrv::pinMode(RGB_B_PIN, OUTPUT);
    is_rgb_set = true;
  }
  WiFiDrv::analogWrite(RGB_R_PIN, r);
  WiFiDrv::analogWrite(RGB_G_PIN, g);
  WiFiDrv::analogWrite(RGB_B_PIN, b);
}

/**
 * Validate moisture sensor definitions then create a new array of sensor definitions and save to global.
 */
void makeSenArray() {
  // Verify sensor pin count
  int c = 0;
  #ifdef MST_PIN_1
  c++;
  #endif
  #ifdef MST_PIN_2
  c++;
  #endif
  #ifdef MST_PIN_3
  c++;
  #endif
  #ifdef MST_PIN_4
  c++;
  #endif
  #ifdef MST_PIN_5
  c++;
  #endif
  #ifdef MST_PIN_6
  c++;
  #endif
  #ifdef MST_PIN_7
  c++;
  #endif
  if(c!=MST_COUNT) {
    rgbLed(100,0,0);
    while(1);
  }

  // Gather pin assignments
  uint8_t pin_arr[c];
  int n = 0;
  #ifdef MST_PIN_1
  pin_arr[n]=MST_PIN_1;
  n++;
  #endif
  #ifdef MST_PIN_2
  pin_arr[n]=MST_PIN_2;
  n++;
  #endif
  #ifdef MST_PIN_3
  pin_arr[n]=MST_PIN_3;
  n++;
  #endif
  #ifdef MST_PIN_4
  pin_arr[n]=MST_PIN_4;
  n++;
  #endif
  #ifdef MST_PIN_5
  pin_arr[n]=MST_PIN_5;
  n++;
  #endif
  #ifdef MST_PIN_6
  pin_arr[n]=MST_PIN_6;
  n++;
  #endif
  #ifdef MST_PIN_7
  pin_arr[n]=MST_PIN_7;
  n++;
  #endif

  // Allocate and populate new sensor array
  mst_sen_arr* new_arr = (mst_sen_arr*)malloc(c * sizeof(mst_sen));
  for(int k = 0; k<c; k++){
    (*new_arr)[k] = { pin_arr[k], 0, 0, 0, 0 };
    snprintf((*new_arr)[k].id, sizeof((*new_arr)[k].id), "%d", k+1);
    snprintf((*new_arr)[k].val_id, sizeof((*new_arr)[k].val_id), "%s%d", "smst", k+1);
  }

  mst_arr_size = c; // Save array size to global
  mst_arr = new_arr;  // Save new array to global
  return;
}