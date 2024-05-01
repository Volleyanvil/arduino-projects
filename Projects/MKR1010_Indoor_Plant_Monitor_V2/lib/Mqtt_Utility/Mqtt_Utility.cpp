/*
  Author: Ilari Mattsson
  Library: Mqtt Utility
  File: Mqtt_Utility.cpp
  Version: 1.1
*/

#include "Mqtt_Utility.h"
#include <ArduinoMqttClient.h>
#include <ArduinoJson.h>
#include <Client.h>
#include <WiFi.h>


MqttUtility::MqttUtility(Client* client):
  _wifiClient(client),
  _mqttClient(new MqttClient(client)),
  _ssid(NULL),
  _psk(NULL),
  _retry(0),
  _host("0.0.0.0"),
  _port(1883),
  _connected(false),
  _ownsMqttClient(true),
  _mqttErr(CONN_NO_ERR),
  _status(CONN_NO_ERR) {
}

MqttUtility::MqttUtility(Client& wifiClient):
  MqttUtility(&wifiClient){
}

MqttUtility::MqttUtility(Client* wifiClient, MqttClient* mqttClient, const char* ssid, const char* psk, const char* mqtt, uint16_t port):
  _wifiClient(wifiClient),
  _mqttClient(mqttClient),
  _ssid(ssid),
  _psk(psk),
  _retry(0),
  _host(mqtt),
  _port(port),
  _connected(false),
  _ownsMqttClient(false),
  _mqttErr(CONN_NO_ERR),
  _status(CONN_NO_ERR) {
}

MqttUtility::MqttUtility(Client& wifiClient, MqttClient& mqttClient, const char* ssid, const char* psk, const char* mqtt, uint16_t port):
  MqttUtility(&wifiClient, &mqttClient, ssid, psk, mqtt, port){
}

MqttUtility::~MqttUtility(){
  if (_ownsMqttClient) {
    delete _mqttClient;
  }
}

// ================================ Class public methods ========================================

int MqttUtility::begin(){
  if (_ssid == NULL || _host == NULL) return CONN_NO_PARAMS;

  int count = 0;  // Connection attempt counter
  if(_psk == NULL || strcmp(_psk, "") == 0){
    while (WiFi.begin(_ssid) != WL_CONNECTED) { 
      if (_retry > 0 && count >= _retry ) {
        _status = CONN_WIFI_TIMEOUT;
        return _status; 
      }
      count++;
      delay(5000); 
    }
  } else {
    while (WiFi.begin(_ssid, _psk) != WL_CONNECTED) { 
      if (_retry > 0 && count >= _retry ) {
        _status = CONN_WIFI_TIMEOUT;
        return _status; 
      }
      count++;
      delay(5000); 
    }
  }

  if (!_mqttClient->connect(_host, _port)) {
    WiFi.end();
    _mqttErr = _mqttClient->connectError();
    _status = CONN_ERR_MQTT;
    return _status;
  }

  _connected = true;
  _status = CONN_CONNECTED;
  return _status;
}

void MqttUtility::end(){
  if(getConnectionStatus() == CONN_OK){
    _mqttClient->stop();
    WiFi.end();
    _connected = false;
  }
}

const char* MqttUtility::version(){
  return LIB_VERSION;
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
  if (!_connected) return CONN_NOT_STARTED;
  int status = getConnectionStatus();
  return reconnect(status);
}

void MqttUtility::configureTopic(mdev devConf) {
  if (!_connected) return;

  // !!! IMPORTANT !!!
  // Assigning char pointer values to JsonDocument fields can cause undefined behavior with serializeJson()
  // >> serializeJson() can sometimes set output[0] out of bounds
  // To avoid the issue always copy string contents to arrays inside this method before assigning to JsonDocument
  // Reading pointer values functions as expected otherwise.

  char dev_cla[strlen(devConf.device_class)+1];
  snprintf(dev_cla, strlen(devConf.device_class) + 1, "%s", devConf.device_class);

  char name[strlen(devConf.name)+1];
  snprintf(name, strlen(devConf.name) + 1, "%s", devConf.name);

  char stat_t[strlen(devConf.state_topic)+1];
  snprintf(stat_t, strlen(devConf.state_topic) + 1, "%s", devConf.state_topic);

  char uniq_id[strlen(devConf.unique_id)+1];
  snprintf(uniq_id, strlen(devConf.unique_id) + 1, "%s", devConf.unique_id);

  char unit_of_meas[strlen(devConf.unit_of_measurement)+1];
  snprintf(unit_of_meas, strlen(devConf.unit_of_measurement) + 1, "%s", devConf.unit_of_measurement);

  char val_tpl[strlen(devConf.value_template)+1];
  snprintf(val_tpl, strlen(devConf.value_template) + 1, "%s", devConf.value_template);

  JsonDocument doc;
  // Do not set device class when value is "None": Generic sensor https://www.home-assistant.io/integrations/sensor/#device-class
  if(strcmp(dev_cla, "None") != 0) doc["dev_cla"] = dev_cla;
  doc["exp_aft"] = devConf.expires_after;
  doc["name"] = name;
  doc["stat_t"] = stat_t;
  doc["uniq_id"] = uniq_id;
  doc["unit_of_meas"] = unit_of_meas;
  doc["val_tpl"] = val_tpl;
  int len = measureJson(doc);                                 
  char output[len++];                                         
  serializeJson(doc, output, len);

  _mqttClient->beginMessage(devConf.configuration_topic, true);
  _mqttClient->print(output);
  _mqttClient->endMessage();

  return;
}

void MqttUtility::configureTopic(mdevs* devConf) {
  if (!_connected) return;

  JsonDocument doc;
  // Do not set device class when value is "None": Generic sensor https://www.home-assistant.io/integrations/sensor/#device-class
  if(devConf->device_class != "None") doc["dev_cla"] = devConf->device_class;                              
  doc["exp_aft"] = devConf->expires_after;                                       
  doc["name"] = devConf->name;                     
  doc["stat_t"] = devConf->state_topic;                                 
  doc["uniq_id"] = devConf->unique_id;                             
  doc["unit_of_meas"] = devConf->unit_of_measurement;                                  
  doc["val_tpl"] = devConf->value_template;  
  int len = measureJson(doc);                                 
  char output[len++];                                         
  serializeJson(doc, output, len);

  _mqttClient->beginMessage(devConf->configuration_topic, true);
  _mqttClient->print(output);
  _mqttClient->endMessage();

  return;
}

void MqttUtility::configureTopic(JsonDocument doc, const char* topic) {
  if (!_connected) return;
  int len = measureJson(doc);                                 
  char output[len++];                                         
  serializeJson(doc, output, len);

  _mqttClient->beginMessage(topic, true);
  _mqttClient->print(output);
  _mqttClient->endMessage();

  return;
}

void MqttUtility::setMqttHost(const char* address, uint16_t port) {
  _host = address;
  _port = port;
}

void MqttUtility::setWiFiNetwork(const char* ssid, const char* psk) {
  _ssid = ssid;
  _psk = psk;
}

bool MqttUtility::setWifiRetry(short i) {
  if (i > 100 || i < 0) return false;
  _retry = i;
  return true;
}

void MqttUtility::setMqttUser(const char* username, const char* password) {
  _user = username;
  _pass = password;
  return _mqttClient->setUsernamePassword(_user, _pass);
}

void MqttUtility::pollMqtt() {
  if (!_connected) return;
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

