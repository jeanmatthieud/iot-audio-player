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

//////////

void configModeCallback(WiFiManager *myWiFiManager);
void mqttCallback(char* topic, byte* payload, unsigned int length);
void mqttReconnect();
void playMusic(unsigned short track);
void processLeds(unsigned long currentTime);
void processJsonMessage(JsonObject& root);

//////////

Wtv020sd16p wtv020sd16p(PIN_WTV_RESET, PIN_WTV_CLOCK, PIN_WTV_DATA, PIN_WTV_BUSY);
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

const char mqtt_server[] = "test.mosquitto.org";
int mqtt_port = 1883; // 1883

//////////

void setup() {
  Serial.begin(115200);

  pinMode(PIN_TARDIS_LED, OUTPUT);

  wtv020sd16p.reset();

  Serial.println("MQTT server configuration");
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(mqttCallback);

  Serial.println("WiFi configuration");
  WiFiManager wifiManager;
  wifiManager.setAPCallback(configModeCallback);
  //wifiManager.setDebugOutput(false);
  //WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  //wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.autoConnect();

  Serial.println("Setup done");
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
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject((char*)payload);
  if(root.success()) {
    root.printTo(Serial);
    Serial.println();

    processJsonMessage(root);
  } else {
    Serial.println("Unable to parse the received JSON");
  }
}

void mqttReconnect() {
  char topic[30];
  char chipIdStr[9];
  itoa(ESP.getChipId(), chipIdStr, 10);

  // Loop until we're reconnected
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqttClient.connect("jmd")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      mqttClient.publish("/objects", chipIdStr, true);
      // ... and resubscribe
      sprintf(topic, "/%s/%d", "object", ESP.getChipId());
      mqttClient.subscribe(topic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void processLeds(unsigned long currentTime) {
  static unsigned long firstPulse = currentTime;
  analogWrite(PIN_TARDIS_LED, sin(2 * PI / 4000 * (currentTime - firstPulse)) * 127.5 + 127.5);
}

void processJsonMessage(JsonObject& root) {
  const char* action = root.get<const char*>("action");
  if(action != NULL) {
    if(strcmpi(action, "play") == 0) {
      unsigned short track = root.get<unsigned short>("track");
      if(track >= 0 && track < 255) {
        playMusic(track);
      }
    } else if(strcmpi(action, "pause") == 0) {
      wtv020sd16p.pauseVoice();
    } else if(strcmpi(action, "stop") == 0) {
      wtv020sd16p.stopVoice();
    } else if(strcmpi(action, "mute") == 0) {
      wtv020sd16p.mute();
    } else if(strcmpi(action, "unmute") == 0) {
      wtv020sd16p.unmute();
    }
  } else {
    Serial.println("No action specified !");
  }
}

void playMusic(unsigned short track) {
  if(!wtv020sd16p.isBusy()) {
    wtv020sd16p.asyncPlayVoice(track);
    delay(100);
  }
  delay(5);
}
