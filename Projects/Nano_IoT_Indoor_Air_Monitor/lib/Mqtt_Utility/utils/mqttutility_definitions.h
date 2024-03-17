/*
  Author: Ilari Mattsson
  Library: Mqtt Utility
  File: mqttutility_definitions.h
*/

typedef struct mqtt_device_configuration {
  const char* device_class;
  unsigned short expires_after;
  const char* name;
  const char* state_topic;
  const char* unique_id;
  const char* unit_of_measurement;
  const char* value_template;
  const char* configuration_topic;
} mdev;

typedef struct mqtt_device_configuration_str {
  String device_class;
  unsigned short expires_after;
  String name;
  String state_topic;
  String unique_id;
  String unit_of_measurement;
  String value_template;
  String configuration_topic;
} mdevs;

typedef enum {
    CONN_NO_ERR = 123,
    CONN_NO_PARAMS = 50,
    CONN_OK = 0,
    CONN_NO_MQTT,
    CONN_NO_WIFI,
    CONN_CONNECTED,
    CONN_ERR_MQTT,
    CONN_WIFI_TIMEOUT,
    CONN_NOT_STARTED
} util_conn_status;
