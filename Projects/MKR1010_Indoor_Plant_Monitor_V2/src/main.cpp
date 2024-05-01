/*
  Arduino/PlatformIO project implementing a plant monitoring IoT device.
  Sensors include soil moisture, temperature, relative humidity, light levels
  Data is transmission with MQTT over WiFi. Supports TLS. Does not support client certificates.
  Implements MQTT Discovery protocol for automatic device discovery and configuration for Home Assistant.

  Changes in V2.0:
  - More consistent and readable naming scheme
  - CHANGED temp & humi sensor from DHT22 to SHT31
  - ADDED Si1151 Sunlight sensor
  - 

  Board(s):
    - Arduino MKR WiFi 1010
    - Arduino Nano 33 IoT (platformio.ini env not configured)
  Sensors:
    - Up to 7 analog soil moisture sensors, capacitive or resistive
    - SHT31 Temperature & Humidity sensor (optional)
  Device designation:
    - Green[A-Z]

  Author: Ilari Mattsson
  Project MKR1010_Indoor_Plant_Monitor_V2
  File: main.cpp
  Version: 2.0
*/

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ArduinoMqttClient.h>
#include "Adafruit_SHT31.h"
#include "Si115X.h"
#include <WiFiNINA.h>
#include <utility/wifi_drv.h>
#include "arduino_secrets.h"
#include "local_utils.h"
#include <Mqtt_Utility.h>

// ------- Globals ------------
// > Macros
#define MST_PIN_1 A0
#define MST_PIN_2 A1
#define MST_PIN_3 A2
#define MST_PIN_4 A3
#define MST_PIN_5 A4
// #define MST_PIN_6 A5
// #define MST_PIN_7 A6
#define SHT31_ENABLED
#define SI1151_ENABLED
#define CASE_LED LED_BUILTIN
#define TOUCH_PIN (uint8_t)2u

// Built-in RGB LED pins (MKR 1010 WiFi ONLY)
#define RGB_R_PIN (uint8_t)25u
#define RGB_G_PIN (uint8_t)26u
#define RGB_B_PIN (uint8_t)27u

// > Configuration
const bool isCapacitive = true;
const uint32_t loopInterval = 300000;
uint32_t loopPrevious = 0;
bool isRGBSet = false;
const uint16_t sensorTimeout = 3600;

// > Sensors
mst_sen_arr* mstArray;
int mstArraySize;

#ifdef SI1151_ENABLED
Si115X si1151;
uint16_t sun;
#endif  // SI1151_ENABLED

#ifdef SHT31_ENABLED
Adafruit_SHT31 sht = Adafruit_SHT31();
float temp, hum;
#endif  // SHT31_ENABLED

// > Secrets (arduino_secrets.h)
char ssid[] = S_SSID;
char psk[] = S_PASS;
char host[] = S_MQTT_ADDR;
uint16_t port = S_MQTT_PORT;
char user[] = S_MQTT_USER;
char pass[] = S_MQTT_PASS;

// > Clients - Uncomment one wifiClient declaration (standard OR SSL)
WiFiClient wifiClient;
//WiFiSSLClient wifiClient;
MqttUtility mqttUtility(wifiClient);

// > Sensor const variables
const char stateTopic[] = "homeassistant/sensor/greenA/state";
const char deviceName[] = "GreenA";
const char deviceNameLower[] = "greenA";


// Function declarations
void measureData();
void sendData();
int calMoisture(uint8_t pin, bool calCapacitive, bool calBase);
void rgbLed(uint8_t r, uint8_t g, uint8_t b);

void setup() {
  analogReadResolution(10);
  pinMode(CASE_LED, OUTPUT);
  pinMode(TOUCH_PIN, INPUT);
  digitalWrite(CASE_LED, HIGH);
  delay(50);

  // Initialize WiFi & MQTT
  //
  mqttUtility.setWiFiNetwork(ssid, psk);
  mqttUtility.setMqttHost(host, port);
  mqttUtility.setWifiRetry(5);
  if (strlen(user) > 0 && strlen(pass) > 0) {
    mqttUtility.setMqttUser(user, pass);
    delay(50);
  }
  if(mqttUtility.begin() != CONN_CONNECTED) {
    rgbLed(100,0,0);
    while(1) {
      digitalWrite(CASE_LED, LOW);
      delay(1000);
      digitalWrite(CASE_LED, HIGH);
      delay(1000);
    }
  }
  delay(50);

  // Generate and configure moisture sensor array
  //
  makeSenArray();
  delay(50);

  int redMax, grnMax, bluMax;
  // Calibration - Base(Dry)
  rgbLed(100, 50, 0);
  redMax = 140, grnMax = 70, bluMax = 0;
  while(digitalRead(TOUCH_PIN) != HIGH);
  for (int i = 0; i < mstArraySize; i++){
    rgbLed(redMax*(i+1)/mstArraySize, grnMax*(i+1)/mstArraySize, bluMax*(i+1)/mstArraySize);
    (*mstArray)[i].base = calMoisture((*mstArray)[i].pin, isCapacitive, true);
    delay(100);
  }
  delay(50);
  rgbLed(0, 100, 0);
  delay(2000);
  rgbLed(0, 0, 0);
  delay(50);

  // Calibration - Cap(Wet)
  rgbLed(0, 100, 100);
  redMax = 0, grnMax = 140, bluMax = 140;
  while(digitalRead(TOUCH_PIN) != HIGH);
  for (int i = 0; i < mstArraySize; i++){
    rgbLed(redMax*(i+1)/mstArraySize, grnMax*(i+1)/mstArraySize, bluMax*(i+1)/mstArraySize);
    (*mstArray)[i].cap = calMoisture((*mstArray)[i].pin, isCapacitive, false);
    delay(100);
  }
  delay(50);
  rgbLed(0, 100, 0);
  delay(2000);
  rgbLed(0, 0, 0);
  delay(50);

  for (int i = 0; i < mstArraySize; i++){

    String name = String(deviceName) + " Soil Moisture";
    String uid = String(deviceNameLower) + "soil" + (*mstArray)[i].id;
    String valTpl = "{{ value_json." + String((*mstArray)[i].val_id) + " }}";
    String confTopic = "homeassistant/sensor/" + String(deviceNameLower) + "mst" + (*mstArray)[i].id + "/config";

    mdevs mstDev = { "moisture", sensorTimeout, name, String(stateTopic), uid, "%", valTpl, confTopic };
    mqttUtility.configureTopic(&mstDev);
  }
  delay(50);

  // Initialize and configure SHT31
  //
  #ifdef SHT31_ENABLED
  if(!sht.begin(SHT31_DEFAULT_ADDR)){
    rgbLed(100,50,0);
    while(1);
  }
  String shtTempName = String(deviceName) + " Air Temperature"; 
  String shtTempUid = String(deviceNameLower) + "temp";
  String shtTempValTpl = "{{ value_json.temp | round(1) }}";
  String shtTempConfTopic = "homeassistant/sensor/" + String(deviceNameLower) + "temp/config";

  mdevs shtTempDev = {"temperature", sensorTimeout, shtTempName, String(stateTopic), shtTempUid, "°C", shtTempValTpl, shtTempConfTopic};
  mqttUtility.configureTopic(&shtTempDev);

  String shtHumiName = String(deviceName) + " Air Humidity"; 
  String shtHumiUid = String(deviceNameLower) + "humi";
  String shtHumiValTpl = "{{ value_json.humi | round(1) }}";
  String shtHumiConfTopic = "homeassistant/sensor/" + String(deviceNameLower) + "humi/config";

  mdevs shtHumiDev = {"humidity", sensorTimeout, shtHumiName, String(stateTopic), shtHumiUid, "°C", shtHumiValTpl, shtHumiConfTopic};
  mqttUtility.configureTopic(&shtHumiDev);
  delay(50);
  #endif // SHT31_ENABLED

  // Initialize and configure Si1151
  //
  #ifdef SI1151_ENABLED
  if(!si1151.Begin()){
    rgbLed(100,0,50);
    while(1);
  }
  String siSunName = String(deviceName) + " Sunlight"; 
  String siSunUid = String(deviceNameLower) + "sun";
  String siSunValTpl = "{{ value_json.sun }}";
  String siSunConfTopic = "homeassistant/sensor/" + String(deviceNameLower) + "sun/config";

  mdevs shtTempDev = {"temperature", sensorTimeout, siSunName, String(stateTopic), siSunUid, "°C", siSunValTpl, siSunConfTopic};
  mqttUtility.configureTopic(&shtTempDev);
  delay(50);
  #endif // SI115_ENABLED

  digitalWrite(CASE_LED, LOW);
}

void loop() {
  mqttUtility.pollMqtt();
  unsigned long current = millis();
  
  if (current - loopPrevious >= loopInterval) {
    loopPrevious = current;
    digitalWrite(CASE_LED, HIGH);
    mqttUtility.checkConnection();
    measureData();
    sendData();
    digitalWrite(CASE_LED, LOW);
  }
  delay(1000);
}

void measureData() {
  short loops = 40;
  int raw;
  
  // Reset moisture sensort sums to 0
  for(int i=0; i < mstArraySize; i++){
    (*mstArray)[i].sum = 0;
  }

  for(int i=0; i < loops; i++){
    for(int k=0; k < mstArraySize; k++){
      // Constrain values within calibrated range.
      raw = constrain(analogRead((*mstArray)[k].pin), (*mstArray)[k].cap, (*mstArray)[k].base);
      (*mstArray)[k].sum += raw;
    }
    delay(100);
  }

    for(int k=0; k < mstArraySize; k++){
    (*mstArray)[k].val = map((*mstArray)[k].sum/loops, (*mstArray)[k].cap, (*mstArray)[k].base, 100, 0);
  }

  #ifdef SHT31_ENABLED
  do {
    temp = sht.readTemperature();
    hum = sht.readHumidity();
  } while (isnan(temp) || isnan(hum));
  #endif

  #ifdef SI1151_ENABLED
  do {
    sun = si1151.ReadVisible();
  } while (isnan(sun));
  #endif

  return;
}

void sendData() {
    JsonDocument doc;
    // Add moisture sensors (for loop)
    for(int i=0; i<mstArraySize; i++){
      doc[(*mstArray)[i].val_id] = (*mstArray)[i].val;
    }
    // Add other sensors
    #ifdef SHT31_ENABLED
    doc["temp"] = temp;
    doc["hum"] = hum;
    #endif
    #ifdef SI1151_ENABLED
    doc["sun"] = sun;
    #endif
    int len = measureJson(doc);
    char output[len++];
    serializeJson(doc, output, len);
    mqttUtility.checkConnection();
    mqttUtility.sendPackets(doc, stateTopic);
    return;
}

/**
 * Simple analog sensor calibration tool
 * params: 
 *   uint8_t pin: analog sensor pin to use, e.g. A0, A1, ..., A6. Board specific.
 *   bool calCapacitive: determines if calibrated sensor is capacitive (true) or resistive (false)
 *   bool calBase: determines whether calibration is for base/wet (true) or cap/dry (false)
 * returns: int value: calibration value
*/
int calMoisture(uint8_t pin, bool calCapacitive, bool calBase){
  uint32_t start = millis();
  int value, duration = 5000;
  bool useGreaterThan;

  if(calCapacitive){
    if(calBase) value = 1023;
    else value = 0;
  } else {
    if(calBase) value = 0;
    else value = 1023;
  }

  if (value == 0) useGreaterThan = true;
  else useGreaterThan = false;

  while(millis() - start < duration){
    int readout = analogRead(pin);
    if(useGreaterThan){ 
      if(readout > value) value = readout;
    }else{
      if(readout < value) value = readout;
    }
    delay(100);
  }

  return value;
}

/**
 * WiFiNINA boards, MKR 1010 WiFi: control built-in RGB LED
 * params: uint8_t r, g, b: per-color write values to use
 * returns: null
*/
void rgbLed(uint8_t r, uint8_t g, uint8_t b) {
  if(!isRGBSet){
    WiFiDrv::pinMode(RGB_R_PIN, OUTPUT);
    WiFiDrv::pinMode(RGB_G_PIN, OUTPUT);
    WiFiDrv::pinMode(RGB_B_PIN, OUTPUT);
    isRGBSet = true;
  }
  WiFiDrv::analogWrite(RGB_R_PIN, r);
  WiFiDrv::analogWrite(RGB_G_PIN, g);
  WiFiDrv::analogWrite(RGB_B_PIN, b);
}

/**
 * Count enabled moisture sensor pins and create a new array of sensor definitions.
*/
void makeSenArray() {

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
  mst_sen_arr* newMstArray = (mst_sen_arr*)malloc(c * sizeof(mst_sen));
  for(int k = 0; k<c; k++){
    (*newMstArray)[k] = { pin_arr[k], 0, 0, 0, 0 };
    snprintf((*newMstArray)[k].id, sizeof((*newMstArray)[k].id), "%d", k+1);
    snprintf((*newMstArray)[k].val_id, sizeof((*newMstArray)[k].val_id), "%s%d", "smst", k+1);
  }

  mstArraySize = c; // Save array size to global
  mstArray = newMstArray;  // Save new array to global
  return;
}