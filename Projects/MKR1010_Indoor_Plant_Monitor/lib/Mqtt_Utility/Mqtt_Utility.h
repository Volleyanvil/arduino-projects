/*
  Arduino utility library for WiFi capable boards.
  A WiFi and Mqtt client wrapper class that implements commonly used methods used by
  arduino-based sensors to communicate with MQTT brokers over WiFi.

  [Version 1] Basic MQTT sender applications
  > Implements: 
    - Basic getter and setter methods
    - Connecting to WiFi and MQTT broker
    - Connection status checking and reconnecting
    - MQTT Discovery protocol device configuration publishing
    - Publishing JSON payloads to MQTT topic

  Author: Ilari Mattsson
  Library: Mqtt Utility
  File: Mqtt_Utility.h
  Version: 1*

  *Library has been modified in this implementationt
*/

#ifndef MQTT_UTIL_H
#define MQTT_UTIL_H

#include <ArduinoMqttClient.h>
#include <ArduinoJson.h>
#include <Client.h>
#include <WiFi.h>

extern "C" {  // External definitions using C calling conventions
	#include "utils/mqttutility_definitions.h"
}

class MqttUtility {
public:
  MqttUtility(Client* client);
  MqttUtility(Client& client);
  MqttUtility(Client* wifiClient, MqttClient* mqttClient, char* ssid, char* psk, char* host, uint16_t port);
  MqttUtility(Client& wifiClient, MqttClient& mqttClient, char* ssid, char* psk, char* host, uint16_t port);
  ~MqttUtility();

  int init();

  // void sendPackets(); // Using dynamic size data structure
  void sendPackets(JsonDocument doc, const char* topic);

  uint16_t checkConnection();

  void configureTopic(mdev* device_config);
  void configureTopic(mdevfs* device_config);
  void configureTopic(JsonDocument doc, const char* topic);

  void setMqttHost(char* host, uint16_t port);

  void setMqttUser(char* username, char* password);

  void setWiFiNetwork(char* ssid, char* psk);

  bool setWifiRetry(short i);

  void pollMqtt();

  int16_t getStatus();

  int16_t getMqttError();

private:
  int16_t getConnectionStatus() const;
  
  int16_t reconnect(int status);

  Client* _wifiClient;
  MqttClient* _mqttClient;

  const char* _ssid;
  const char* _psk;
  uint16_t _retry;  // Number of WiFi connection attempts [0-100], 0 = unlimited

  const char* _host;
  uint16_t _port;

  bool _init;

  const char* _user;
  const char* _pass;

  int16_t _mqttErr;  // MqttClient Uses error codes -2 -> 5
  int16_t _status;
};

 #endif // MQTT_UTIL_H