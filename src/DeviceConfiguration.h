#ifndef _GATEWAY_CONFIGURATION_H_
#define _GATEWAY_CONFIGURATION_H_

#include <Arduino.h>

#define FIELD_HOST_LENGTH 50
#define FIELD_PORT_LENGTH 6 + 1
#define FIELD_NAME_LENGTH 30 + 1
#define FIELD_LOGIN_LENGTH 30 + 1
#define FIELD_PASSWORD_LENGTH 30 + 1

class DeviceConfiguration
{
public:
  DeviceConfiguration();
  ~DeviceConfiguration();

  void startWifiConfiguration(bool autoConnect, bool resetConfig = false);
  void saveConfigCallback();

  String getMqttHost() {
    return String(this->mqttHost);
  }
  uint16_t getMqttPort() {
    return (uint16_t)atoi(this->mqttPort);
  }
  String getMqttLogin() {
    return String(this->mqttLogin);
  }
  String getMqttPassword() {
    return String(this->mqttPassword);
  }
  String getName() {
    return String(this->name);
  }

private:
  bool shouldSaveConfig = false;
  char mqttHost[FIELD_HOST_LENGTH];
  char mqttPort[FIELD_PORT_LENGTH];
  char mqttLogin[FIELD_LOGIN_LENGTH];
  char mqttPassword[FIELD_PASSWORD_LENGTH];
  char name[FIELD_NAME_LENGTH];

  void readConfigFile(bool resetConfig = false);
  void saveConfigFile();
};

#endif
