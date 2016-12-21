#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Ticker.h>
#include <ESP8266HTTPUpdate.h>

#include "ntp.h"
#include "Wtv020sd16p.h"
#include "DeviceConfiguration.h"

#define PIN_WTV_RESET D1
#define PIN_WTV_CLOCK D3
#define PIN_WTV_DATA D2
#define PIN_WTV_BUSY D5
#define PIN_TARDIS_LED D8
#define PIN_PORTAL_MODE D6

#define ANALOG_MAX_VALUE 1024
#define DEFAULT_PULSE_DELAY_MS 15*1000

#define API_VERSION "1.0"
#define API_TOPIC_MESSAGES "messages"
#define API_TOPIC_ACTIONS "actions"
#define API_TOPIC_ERRORS "errors"

#define CHECK_VERSION_INTERVAL 60*30//60*60*4
#define NTP_SERVER "pool.ntp.org"

#define VERSION_CODE 1
#define HTTP_UPDATE_HOST "store.iot-experiments.com"
#define HTTP_UPDATE_PORT 80
#define HTTP_UPDATE_URL "/ESP/"

//////////

void configModeCallback(WiFiManager *myWiFiManager);
void mqttCallback(char* topic, byte* payload, unsigned int length);
void mqttReconnect();
void playMusic(unsigned short track);
void processLeds(unsigned long currentTime);
void processJsonMessage(JsonObject& root);
void sendStatus();
void checkVersion();
void busyCallback();
void checkVersionTickerCallback();

//////////

Wtv020sd16p wtv020sd16p(PIN_WTV_RESET, PIN_WTV_CLOCK, PIN_WTV_DATA, PIN_WTV_BUSY, busyCallback);
DeviceConfiguration deviceConfiguration;
WiFiClientSecure wifiClient;
PubSubClient mqttClient(wifiClient);

char mqttTopicMessages[255];
char mqttTopicActions[255];
char mqttTopicErrors[255];

unsigned long pulseTardisLedsEndMs = 0;
bool statusChanged = false;
bool shouldUpdateNtp = false;
bool shouldCheckVersion = false;

Ticker checkVersionTicker;

//////////

WiFiEventHandler disconnectedEventHandler = WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected& event) {
  Serial.println("Station disconnected");
  shouldUpdateNtp = false;
  checkVersionTicker.detach();
});

WiFiEventHandler gotIpEventHandler = WiFi.onStationModeGotIP([](const WiFiEventStationModeGotIP& event) {
  Serial.println("Station got IP");
  if(WiFi.getMode() == WIFI_STA) {
    shouldUpdateNtp = true;
    checkVersionTicker.attach(CHECK_VERSION_INTERVAL, checkVersionTickerCallback);
  }
});

//////////

void setup() {
  Serial.begin(115200);

  // - Pins
  pinMode(PIN_TARDIS_LED, OUTPUT);

  // - Audio module
  wtv020sd16p.reset();

  // - MQTT topics
  sprintf(mqttTopicMessages, "/v%s/%s/%d", API_VERSION, API_TOPIC_MESSAGES, ESP.getChipId());
  sprintf(mqttTopicActions, "/v%s/%s/%d", API_VERSION, API_TOPIC_ACTIONS, ESP.getChipId());
  sprintf(mqttTopicErrors, "/v%s/%s/%d", API_VERSION, API_TOPIC_ERRORS, ESP.getChipId());

  mqttClient.setCallback(mqttCallback);


  bool resetConfig = false;
  /*if(digitalRead(PIN_PORTAL_MODE) == HIGH) {
    Serial.println("Config button pressed during power-up : will reset configuration");
    resetConfig = true;
  }*/
  deviceConfiguration.startWifiConfiguration(true, resetConfig);

  Serial.println("End of setup");
}

void loop() {
  /*if (digitalRead(PIN_PORTAL_MODE) == HIGH) {
    Serial.println("Config button pressed");
    deviceConfiguration.startWifiConfiguration(false);
  }*/

  if (!mqttClient.connected()) {
    mqttReconnect();
  }
  mqttClient.loop();

  if(shouldUpdateNtp) {
    shouldUpdateNtp = false;
    updateNtp(String(NTP_SERVER));
  }

  if(shouldCheckVersion) {
    shouldCheckVersion = false;
    checkVersion();
  }

  const unsigned long currentTime = millis();
  processLeds(currentTime);

  if(statusChanged) {
    statusChanged = false;
    sendStatus();
  }
}

void configModeCallback(WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());

  Serial.println(myWiFiManager->getConfigPortalSSID());
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived on topic [");
  Serial.print(topic);
  Serial.print("] ");

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject((char*)payload);
  if(root.success()) {
    root.printTo(Serial);
    Serial.println();

    processJsonMessage(root);
    statusChanged = true;
  } else {
    mqttClient.publish(mqttTopicErrors, "Unable to parse the received JSON");
  }
}

void mqttReconnect() {
  mqttClient.setServer(deviceConfiguration.getMqttHost().c_str(), deviceConfiguration.getMqttPort());
  char mqttDeviceName[255];
  sprintf(mqttDeviceName, "esp-", ESP.getChipId());

  // Loop until we're reconnected
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqttClient.connect(mqttDeviceName)) {
      Serial.println(" Connected");
      // Once connected, publish an announcement...
      statusChanged = true;
      shouldUpdateNtp = true;
      // ... and subscribe to device actions
      mqttClient.subscribe(mqttTopicActions);
    } else {
      Serial.print(" Failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println("; try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void processLeds(unsigned long currentTime) {
  static unsigned long pulseBeginTime = currentTime;
  static bool pulseOn = false;
  if(pulseTardisLedsEndMs == -1 || currentTime < pulseTardisLedsEndMs) {
    analogWrite(PIN_TARDIS_LED, sin(2 * PI / 2000 * (currentTime - pulseBeginTime)) * (ANALOG_MAX_VALUE / 2) + (ANALOG_MAX_VALUE / 2));
    pulseOn = true;
  } else if(pulseOn) {
    pulseOn = false;
    statusChanged = true;
    analogWrite(PIN_TARDIS_LED, 0);
  }
}

void processJsonMessage(JsonObject& root) {
  const char* actionName = root["name"].as<const char*>();
  if(actionName != NULL) {
    if(strcmpi(actionName, "play") == 0) {
      JsonObject& parameters = root["params"].asObject();
      if(parameters != JsonObject::invalid()) {
        unsigned short track = parameters["track"].as<unsigned short>();
        if(track >= 0 && track < 255) {
          playMusic(track);
          return;
        }
      }
      mqttClient.publish(mqttTopicErrors, "Track number missing");
    } else if(strcmpi(actionName, "pause") == 0) {
      wtv020sd16p.pauseVoice();
    } else if(strcmpi(actionName, "stop") == 0) {
      wtv020sd16p.stopVoice();
    } else if(strcmpi(actionName, "mute") == 0) {
      wtv020sd16p.mute();
    } else if(strcmpi(actionName, "unmute") == 0) {
      wtv020sd16p.unmute();
    } else if(strcmpi(actionName, "volume") == 0) {
      JsonObject& parameters = root["parameters"].asObject();
      if(parameters != JsonObject::invalid()) {
        unsigned short value = parameters["value"].as<unsigned short>();
        if(value >= 0 && value < 8) {
          wtv020sd16p.setVolume(value);
        }
      }
    } else if(strcmpi(actionName, "pulse") == 0) {
      JsonObject& parameters = root["parameters"].asObject();
      if(parameters != JsonObject::invalid()) {
        int duration = parameters["duration"].as<int>();
        if(duration > 0) {
          pulseTardisLedsEndMs = millis() + duration;
        } else if(duration == -1) {
          pulseTardisLedsEndMs = -1;
        } else if(duration == 0) {
         pulseTardisLedsEndMs = 0;
        }
      } else {
        pulseTardisLedsEndMs = millis() + DEFAULT_PULSE_DELAY_MS;
      }
    } else {
      mqttClient.publish(mqttTopicErrors, "Action name not recognized");
    }
  } else {
    mqttClient.publish(mqttTopicErrors, "No action name specified");
  }
}

void playMusic(unsigned short track) {
  if(!wtv020sd16p.isBusy()) {
    wtv020sd16p.asyncPlayVoice(track);
    delay(100);
  }
  delay(5);
}

void busyCallback() {
  Serial.println("Busy state changed");
  statusChanged = true;
}

void checkVersionTickerCallback() {
  Serial.println("Should check version");
  shouldCheckVersion = true;
}

void sendStatus() {
  if(!mqttClient.connected()) {
    return;
  }

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["type"] = "tardis";
  root["name"] = deviceConfiguration.getName();
  root["time"] = getCurrentTimestamp();
  root["audio"] = wtv020sd16p.isBusy();
  root["pulse"] = pulseTardisLedsEndMs == -1 || pulseTardisLedsEndMs > millis();
  char buffer[512];
  root.printTo(buffer, sizeof(buffer));
  mqttClient.publish(mqttTopicMessages, buffer);
}

void checkVersion() {
  t_httpUpdate_return ret = ESPhttpUpdate.update(HTTP_UPDATE_HOST, HTTP_UPDATE_PORT, HTTP_UPDATE_URL + ESP.getChipId(), String(VERSION_CODE));
  switch(ret) {
    case HTTP_UPDATE_FAILED:
      Serial.println("[update] Update failed.");
      Serial.println(ESPhttpUpdate.getLastErrorString());
    break;
    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("[update] No update.");
    break;
    case HTTP_UPDATE_OK:
      Serial.println("[update] Update ok."); // may not be called; reboot the ESP
    break;
  }
}
