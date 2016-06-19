#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include "Wtv020sd16p.h"

#define PIN_WTV_RESET D1
#define PIN_WTV_CLOCK D3
#define PIN_WTV_DATA D2
#define PIN_WTV_BUSY D5
#define PIN_TARDIS_LED D4

#define API_VERSION "1.0"
#define API_TOPIC_MESSAGES "messages"
#define API_TOPIC_ACTIONS "actions"
#define API_TOPIC_ERRORS "errors"

//////////

void configModeCallback(WiFiManager *myWiFiManager);
void mqttCallback(char* topic, byte* payload, unsigned int length);
void mqttReconnect();
void playMusic(unsigned short track);
void processLeds(unsigned long currentTime);
void processJsonMessage(JsonObject& root);
void sendStatus();
void busyCallback();

//////////

Wtv020sd16p wtv020sd16p(PIN_WTV_RESET, PIN_WTV_CLOCK, PIN_WTV_DATA, PIN_WTV_BUSY, busyCallback);
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

const char mqttHost[] = "test.mosquitto.org";
int mqttPort = 1883;

char mqttTopicMessages[255];
char mqttTopicActions[255];
char mqttTopicErrors[255];

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

  // - MQTT client
  mqttClient.setServer(mqttHost, mqttPort);
  mqttClient.setCallback(mqttCallback);

  // - Wifi
  WiFiManager wifiManager;
  wifiManager.setAPCallback(configModeCallback);
  //wifiManager.setDebugOutput(false);
  //WiFiManagerParameter custom_mqttHost("server", "mqtt server", mqttHost, 40);
  //wifiManager.addParameter(&custom_mqttHost);
  wifiManager.autoConnect();

  Serial.println("End of setup");
}

void loop() {
  if (!mqttClient.connected()) {
    mqttReconnect();
  }
  mqttClient.loop();

  const unsigned long currentTime = millis();
  processLeds(currentTime);
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
  } else {
    mqttClient.publish(mqttTopicErrors, "Unable to parse the received JSON");
  }
}

void mqttReconnect() {
  // Loop until we're reconnected
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqttClient.connect("jmd")) {
      Serial.println(" Connected");
      // Once connected, publish an announcement...
      sendStatus();
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
  analogWrite(PIN_TARDIS_LED, sin(2 * PI / 4000 * (currentTime - pulseBeginTime)) * 127.5 + 127.5);
}

void processJsonMessage(JsonObject& root) {
  const char* actionName = root["name"].as<const char*>();
  if(actionName != NULL) {
    if(strcmpi(actionName, "play") == 0) {
      JsonObject& parameters = root["parameters"].asObject();
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
  if(mqttClient.connected()) {
    sendStatus();
  }
}

void sendStatus() {
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["sensor"] = "audio-output";
  root["busy"] = wtv020sd16p.isBusy();
  char buffer[256];
  root.printTo(buffer, sizeof(buffer));
  mqttClient.publish(mqttTopicMessages, buffer);
}
