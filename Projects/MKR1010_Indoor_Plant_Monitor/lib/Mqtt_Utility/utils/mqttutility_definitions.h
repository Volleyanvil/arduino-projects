/*
  Author: Ilari Mattsson
  Library: Mqtt Utility
  File: mqttutility_definitions.h
*/

typedef struct mqtt_device_configuration {
  const char* configuration_topic;
  const char* device_class;
  unsigned short expires_after;
  const char* name;
  const char* state_topic;
  const char* unique_id;
  const char* unit_of_measurement;
  const char* value_template;
} mdev;

enum mdevfs_buffers{
  B_CONF_T = 128,
  B_DEV_CLA = 32,
  B_NAME = 64,
  B_STATE_T = 128,
  B_UNIQ_ID = 16,
  B_UNIT_OF_MEAS = 8,
  B_VAL_TPL = 128
};

typedef struct mqtt_device_fixed_size_configuration {
  char configuration_topic[B_CONF_T];
  char device_class[B_DEV_CLA];
  unsigned short expires_after;
  char name[B_NAME];
  char state_topic[B_STATE_T];
  char unique_id[B_UNIQ_ID];
  char unit_of_measurement[B_UNIT_OF_MEAS];
  char value_template[B_VAL_TPL];
} mdevfs;

typedef struct {
  const char* key;
  const char* value;
} pair;

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
