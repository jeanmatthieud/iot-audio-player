#include "DeviceConfiguration.h"
#include <FS.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>

class FunctorSaveConfigCallback {
  DeviceConfiguration * instance;
  public:
    FunctorSaveConfigCallback(DeviceConfiguration * instance) : instance(instance) { }
    void operator()(void) {
        instance->saveConfigCallback();
    }
};

FunctorSaveConfigCallback* functor = NULL;
DeviceConfiguration::DeviceConfiguration() {
  functor = new FunctorSaveConfigCallback(this);

  sprintf(mqttHost, "%s", "mqtt.iot-experiments.com");
  sprintf(mqttPort, "%d", 8883);
  sprintf(name, "%s", "Mon Tardis");
  sprintf(mqttLogin, "%s", (String("ESP") + ESP.getChipId()).c_str());
  sprintf(mqttPassword, "%s", "Tardis2017");
}

DeviceConfiguration::~DeviceConfiguration() {
  delete functor;
}

void DeviceConfiguration::startWifiConfiguration(bool autoConnect, bool resetConfig) {
  this->readConfigFile(resetConfig);

  // The extra parameters to be configured (can be either global or just in the setup)
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_mqtt_host("host", "MQTT host", this->mqttHost, FIELD_HOST_LENGTH);
  WiFiManagerParameter custom_mqtt_port("port", "MQTT port", this->mqttPort, FIELD_PORT_LENGTH);
  WiFiManagerParameter custom_mqtt_login("login", "MQTT login", this->mqttLogin, FIELD_LOGIN_LENGTH);
  WiFiManagerParameter custom_mqtt_password("password", "MQTT password", this->mqttPassword, FIELD_PASSWORD_LENGTH);
  WiFiManagerParameter custom_name("name", "Nom", this->name, FIELD_NAME_LENGTH);

  WiFiManager wifiManager;
  //wifiManager.setSaveConfigCallback((void(*)())functor);
  //wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
  wifiManager.addParameter(&custom_mqtt_host);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_mqtt_login);
  wifiManager.addParameter(&custom_mqtt_password);
  wifiManager.addParameter(&custom_name);
  // Reset settings for testing
  if(resetConfig) {
    wifiManager.resetSettings();
  }
  // Set minimum quality of signal so it ignores AP's under that quality (defaults to 8%)
  //wifiManager.setMinimumSignalQuality();
  //sets timeout until configuration portal gets turned off
  //wifiManager.setTimeout(120);

  String APName = String("Tardis") + " (" + ESP.getChipId() + ")";
  bool connectionResult = autoConnect ? wifiManager.autoConnect(APName.c_str()) : wifiManager.startConfigPortal(APName.c_str());
  if (!connectionResult) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    ESP.reset();
    delay(5000);
  }

  Serial.println("connected...yeey :)");

  strcpy(this->mqttHost, custom_mqtt_host.getValue());
  strcpy(this->mqttPort, custom_mqtt_port.getValue());
  strcpy(this->mqttLogin, custom_mqtt_login.getValue());
  strcpy(this->mqttPassword, custom_mqtt_password.getValue());
  strcpy(this->name, custom_name.getValue());

  //if (this->shouldSaveConfig) {
    this->saveConfigFile();
  //  this->shouldSaveConfig = false;
  //}

  Serial.println("ESP8266 IP");
  Serial.println(WiFi.localIP());
}

void DeviceConfiguration::readConfigFile(bool resetConfig) {
  if(resetConfig) {
    Serial.println("Format FS...");
    SPIFFS.format();
  }

  Serial.println("mounting FS...");
  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      Serial.println("Reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("Config file opened");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");
          strcpy(this->mqttHost, json["mqtt_host"]);
          strcpy(this->mqttPort, json["mqtt_port"]);
          strcpy(this->mqttLogin, json["mqtt_login"]);
          strcpy(this->mqttPassword, json["mqtt_password"]);
          strcpy(this->name, json["name"]);
        } else {
          Serial.println("Failed to load json config");
        }
      }
    }
  } else {
    Serial.println("Failed to mount FS");
  }
}

void DeviceConfiguration::saveConfigFile() {
  Serial.println("saving config");
  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  json["mqtt_host"] = this->mqttHost;
  json["mqtt_port"] = this->mqttPort;
  json["mqtt_login"] = this->mqttLogin;
  json["mqtt_password"] = this->mqttPassword;
  json["name"] = this->name;

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("failed to open config file for writing");
  }

  json.printTo(Serial);
  json.printTo(configFile);
  configFile.close();
}

void DeviceConfiguration::saveConfigCallback() {
  Serial.println("Should save config");
  this->shouldSaveConfig = true;
}
