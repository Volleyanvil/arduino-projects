/*
  Author: Ilari Mattsson
  Library: Mqtt Utility
  File: Mqtt_Utility.cpp
  Version: 1
*/

#include "Mqtt_Utility.h"
#include <ArduinoMqttClient.h>
#include <ArduinoJson.h>
#include <Client.h>
#include <WiFi.h>


MqttUtility::MqttUtility(Client* client):
  MqttUtility(client, new MqttClient(client), NULL, NULL, NULL, 0) {
}

MqttUtility::MqttUtility(Client& wifiClient):
  MqttUtility(&wifiClient){
}

MqttUtility::MqttUtility(Client* wifiClient, MqttClient* mqttClient, char* ssid, char* psk, char* mqtt, uint16_t port):
  _wifiClient(wifiClient),
  _mqttClient(mqttClient),
  _ssid(ssid),
  _psk(psk),
  _retry(0),
  _host(mqtt),
  _port(port),
  _init(false),
  _mqttErr(CONN_NO_ERR),
  _status(CONN_NO_ERR) {
}

MqttUtility::MqttUtility(Client& wifiClient, MqttClient& mqttClient, char* ssid, char* psk, char* mqtt, uint16_t port):
  MqttUtility(&wifiClient, &mqttClient, ssid, psk, mqtt, port){
}

MqttUtility::~MqttUtility(){
  if (_mqttClient != nullptr) {
    delete _mqttClient;
  }
}

int MqttUtility::init(){
  if (_ssid == NULL || _psk == NULL ) return CONN_NO_PARAMS;
  
  int count = 0;

  while (WiFi.begin(_ssid, _psk) != WL_CONNECTED) { 
    if (_retry > 0 && count >= _retry ) {
      _status = CONN_WIFI_TIMEOUT;
      return _status; 
    }
    count++;
    delay(5000); 
  }

  if (!_mqttClient->connect(_host, _port)) {
    WiFi.end();
    _mqttErr = _mqttClient->connectError();
    _status = CONN_ERR_MQTT;
    return _status;
  }

  _init = true;
  _status = CONN_CONNECTED;
  return _status;
}

void MqttUtility::sendPackets(JsonDocument doc, const char* topic) {
  int len = measureJson(doc);
  char output[len++];
  serializeJson(doc, output, len);

  _mqttClient->beginMessage(topic);
  _mqttClient->print(output);
  _mqttClient->endMessage();

  return;
}

uint16_t MqttUtility::checkConnection() {
  if (!_init) return CONN_NOT_STARTED;
  int status = this->getConnectionStatus();
  return this->reconnect(status);
}

// Basic topic configuration
void MqttUtility::configureTopic(mdev* device_config) {
  if (!_init) return;

  // !!! IMPORTANT !!!
  // Assigning char* pointer values to JsonDocument fields can cause undefined behavior with serializeJson()
  // >> serializeJson() can sometimes set output[0] out of bounds
  // To avoid the issue always copy string contents to arrays inside this method before assigning to JsonDocument
  // Note: Accessing pointer contents functions as expected elsewhere.

  char dev_cla[strlen(device_config->device_class)+1];
  snprintf(dev_cla, strlen(device_config->device_class) + 1, "%s", device_config->device_class);

  char name[strlen(device_config->name)+1];
  snprintf(name, strlen(device_config->name) + 1, "%s", device_config->name);

  char stat_t[strlen(device_config->state_topic)+1];
  snprintf(stat_t, strlen(device_config->state_topic) + 1, "%s", device_config->state_topic);

  char uniq_id[strlen(device_config->unique_id)+1];
  snprintf(uniq_id, strlen(device_config->unique_id) + 1, "%s", device_config->unique_id);

  char unit_of_meas[strlen(device_config->unit_of_measurement)+1];
  snprintf(unit_of_meas, strlen(device_config->unit_of_measurement) + 1, "%s", device_config->unit_of_measurement);

  char val_tpl[strlen(device_config->value_template)+1];
  snprintf(val_tpl, strlen(device_config->value_template) + 1, "%s", device_config->value_template);

  JsonDocument doc;
  doc["dev_cla"] = dev_cla;
  doc["exp_aft"] = device_config->expires_after;
  doc["name"] = name;
  doc["stat_t"] = stat_t;
  doc["uniq_id"] = uniq_id;
  doc["unit_of_meas"] = unit_of_meas;
  doc["val_tpl"] = val_tpl;
  int len = measureJson(doc);
  char output[len++];
  serializeJson(doc, output, len);

  this->checkConnection();
  _mqttClient->beginMessage(device_config->configuration_topic, true);
  _mqttClient->print(output);
  _mqttClient->endMessage();

  return;
}

// Topic configuration using a JsonDocument
void MqttUtility::configureTopic(JsonDocument doc, const char* topic) {
  // Note: MqttUtility topic configuration using JsonDocument, topic parameters does not function as expected when tested
  //  - MQTT messages do not get sent, indicating a problem with accessing the topic parameter
  if (!_init) return;
  int len = measureJson(doc);                                 
  char output[len++];                                         
  serializeJson(doc, output, len);

  _mqttClient->beginMessage(topic, true);
  _mqttClient->print(output);
  _mqttClient->endMessage();

  return;
}

void MqttUtility::setMqttHost(char* address, uint16_t port) {
  _host = address;
  _port = port;
}

void MqttUtility::setWiFiNetwork(char* ssid, char* psk) {
  _ssid = ssid;
  _psk = psk;
}

bool MqttUtility::setWifiRetry(short i) {
  if (i > 100 || i < 0) return false;
  _retry = i;
  return true;
}

void MqttUtility::setMqttUser(char* username, char* password) {
  _user = username;
  _pass = password;
  return _mqttClient->setUsernamePassword(_user, _pass);
}

void MqttUtility::pollMqtt() {
  if (!_init) return;
  return _mqttClient->poll();
}

int16_t MqttUtility::getStatus() {
  return _status;
}

int16_t MqttUtility::getMqttError() {
  return _mqttErr;
}

// ================================ Class private methods ========================================

int16_t MqttUtility::getConnectionStatus() const {
  if (WiFi.status() == WL_CONNECTED) {
    if (_mqttClient->connected() == 1) return CONN_OK;
    else return CONN_NO_MQTT;
  } else return CONN_NO_WIFI;
}

int16_t MqttUtility::reconnect(int status) {
  switch (status) {
    case CONN_OK:
      break;
    case CONN_NO_MQTT:
      if (!_mqttClient->connect(_host, _port)) {
        WiFi.end();
        return CONN_ERR_MQTT;
      }
      break;
    case (CONN_NO_WIFI):
      int count = 0;
      while (WiFi.begin(_ssid, _psk) != WL_CONNECTED){ 
        if (_retry > 0 && _retry == count) return CONN_WIFI_TIMEOUT;
        count++;
        delay(5000);
      }
      if (!_mqttClient->connect(_host, _port)) {
        WiFi.end();
        return CONN_ERR_MQTT;
      }
      break;
  }
  return CONN_OK;
}

