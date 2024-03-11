/*
  Arduino/PlatformIO project implementing a plant monitoring sensor.
  Data is transmission with MQTT over WiFi. Supports TLS. Does not support client certificates.
  Implements MQTT Discovery protocol for automatic device discovery and configuration for Home Assistant.

  Board(s):
    - Arduino MKR WiFi 1010
    - Arduino Nano 33 IoT (platformio.ini env not configured)
  Sensors:
    - Up to 7 analog soil moisture sensors, capacitive or resistive
    - DHT22 Temperature & Humidity sensor (optional)
  Device designation:
    - GreenB

  Author: Ilari Mattsson
  Library: Mqtt Utility
  File: Mqtt_Utility.cpp
  Version: 1
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
// WiFiClient wifiClient;
WiFiSSLClient wifiClient;
MqttClient mqttClient(wifiClient);
MqttUtility mqttUtil(wifiClient, mqttClient, ssid, psk, host, port);

// Function declarations
//
void measureData();
void sendData();
char * makeConfTopic(char *, char *);
char * makeNameOrId(char *, char *);
char * makeValueTemplate(char *);
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

  mqttUtil.setMqttUser(user, pass);
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
    (*mst_arr)[i].base = setMoistureCap((*mst_arr)[i].pin, is_capacitive);
    delay(100);
  }
  delay(50);
  rgbLed(0, 100, 0);
  delay(2000);
  rgbLed(0, 0, 0);
  delay(50);

  for (int i = 0; i < mst_arr_size; i++){
    char* name = makeNameOrId((*mst_arr)[i].id, (char*)"GreenB Soil Moisture ");
    char* id = makeNameOrId((*mst_arr)[i].id, (char*)"greenBsoil");
    char* val_tpl = makeValueTemplate((*mst_arr)[i].val_id);
    char* conf_topic = makeConfTopic((*mst_arr)[i].id, (char*)"greenBM");
    mdev dev = { conf_topic, "moisture", sensor_timeout, name, state_topic, id, "%", val_tpl };
    mqttUtil.configureTopic(&dev);
    free(name);
    free(id);
    free(val_tpl);
    free(conf_topic);
  }
  /*
  // Alternative implementation using mdevfs structs with fixed-size char arrays
  for (int i = 0; i < mst_arr_size; i++){
    mdevfs dev;
    snprintf(dev.configuration_topic, B_CONF_T, "%s%s%s", "homeassistant/sensor/greenBM", (*mst_arr)[i].id, "/config");
    snprintf(dev.device_class, B_DEV_CLA, "%s", "moisture");
    dev.expires_after = sensor_timeout;
    snprintf(dev.name, B_NAME, "%s%s", "GreenB Soil Moisture ", (*mst_arr)[i].id);
    snprintf(dev.state_topic, B_STATE_T, "%s", state_topic);
    snprintf(dev.unique_id, B_UNIQ_ID, "%s%s", "greenBsoil", (*mst_arr)[i].id);
    snprintf(dev.unit_of_measurement, B_UNIT_OF_MEAS, "%s", "%");
    snprintf(dev.value_template, B_VAL_TPL, "%s%s%s", "{{ value_json.", (*mst_arr)[i].val_id, " }}");
    mqttUtil.configureTopic(&dev);
  }
  */
  #ifdef DHTPIN
  dht.begin();
  const char dht_temp_conf_t[] = "homeassistant/sensor/greenBT/config";
  const char dht_hum_conf_t[] = "homeassistant/sensor/greenBH/config";
  mdev dhttdev = { dht_temp_conf_t, "temperature", sensor_timeout, "GreenB Air Temperature", state_topic, "greenBtemp", "°C", "{{ value_json.temp | round(2) }}" };
  mqttUtil.configureTopic(&dhttdev);
  mdev dhthdev = { dht_hum_conf_t, "humidity", sensor_timeout, "GreenB Air Humidity", state_topic, "greenBhum", "%", "{{ value_json.hum | round(1) }}" };
  mqttUtil.configureTopic(&dhthdev);
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
  for(int k=0; k < MST_COUNT; k++){
    (*mst_arr)[k].sum = 0;
  }

  for(int i=0; i<lp; i++){
    for(int k=0; k < MST_COUNT; k++){
      // Constrain values to avoid mapping issues
      raw = constrain(analogRead((*mst_arr)[k].pin), (*mst_arr)[k].cap, (*mst_arr)[k].base);
      (*mst_arr)[k].sum += raw;
    }
    delay(100);
  }
  
  for(int k=0; k < MST_COUNT; k++){
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

char * makeConfTopic(char* id, char* body){
  char head[] = "homeassistant/sensor/";
  char tail[] = "/config";
  size_t bufs = strlen(head) + strlen(body) + strlen(id) + strlen(tail) + 1;
  char* arr = (char*)malloc(bufs * sizeof(char));
  snprintf(arr, bufs, "%s%s%s%s", head, body, id, tail);
  return arr;
}

char * makeNameOrId(char* id, char* body){
  size_t bufs = strlen(body) + strlen(id) + 1;
  char* arr = (char*)malloc(bufs * sizeof(char));
  snprintf(arr, bufs, "%s%s", body, id);
  return arr;
}

char * makeValueTemplate(char* val_id){
  char head[] = "{{ value_json.";
  char tail[] = " }}";
  size_t bufs = strlen(head) + strlen(val_id) + strlen(tail) + 1;
  char* arr = (char*)malloc(bufs * sizeof(char));
  snprintf(arr, bufs, "%s%s%s", head, val_id, tail);
  return arr;
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