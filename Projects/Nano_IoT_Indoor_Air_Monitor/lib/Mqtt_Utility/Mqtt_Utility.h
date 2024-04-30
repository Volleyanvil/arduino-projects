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
  Version: 1
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

#define LIB_VERSION "1.1"

class MqttUtility {
public:
  MqttUtility(Client* client);
  MqttUtility(Client& client);
  MqttUtility(Client* wifiClient, MqttClient* mqttClient, const char* ssid, const char* psk, const char* host, uint16_t port);
  MqttUtility(Client& wifiClient, MqttClient& mqttClient, const char* ssid, const char* psk, const char* host, uint16_t port);
  ~MqttUtility();

  /**
   * Start connections
  */
  int begin();

  /**
   * End connections
  */
  void end();

  /**
   * Library version
  */
  static const char* version();

  /**
   * Publish JSON payload to topic
  */
  void sendPackets(JsonDocument doc, const char* topic);

  /**
   * Check connection status
  */
  uint16_t checkConnection();

  /**
   * Publish device configuration from simplified mdev struct
  */
  void configureTopic(mdev deviceConfig);

  /**
   * Publish device configuration from simplified mdev struct
  */
  void configureTopic(mdevs* devConf);

  /**
   * Publish device configuration from JSON document
  */
  void configureTopic(JsonDocument doc, const char* topic);

  /**
   * Set Mqtt host IP and port
  */
  void setMqttHost(const char* host, uint16_t port);

  /**
   * Set MQTT username and password
  */
  void setMqttUser(const char* username, const char* password);

  /**
   * Set WiFi network ssid & psk
  */
  void setWiFiNetwork(const char* ssid, const char* psk);

  /**
   * Set WiFi retry attempts, 0-100 (0 = retry forever).
  */
  bool setWifiRetry(short i);

  /**
   * Poll Mqtt connection
  */
  void pollMqtt();

  /**
   * Get status code
  */
  int16_t getStatus();

  /**
   * Get Mqtt error code
  */
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

  bool _connected;
  bool _ownsMqttClient;

  const char* _user;
  const char* _pass;

  int16_t _mqttErr;  // MqttClient Uses error codes -2 -> 5
  int16_t _status;
};

 #endif // MQTT_UTIL_H