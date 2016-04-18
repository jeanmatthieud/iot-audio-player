#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <PubSubClient.h>
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

static unsigned long firstPulse;
void loop() {
  if (!mqttClient.connected()) {
    mqttReconnect();
  }
  mqttClient.loop();

  firstPulse = millis();
  analogWrite(PIN_TARDIS_LED, sin(2 * PI / 4000 * millis()/* - firstPulse*/) * 127.5 + 127.5);

  //Plays audio file number 1 during 2 seconds.
  //delay(5000);
  //Pauses audio file number 1 during 2 seconds.
  //wtv020sd16p.pauseVoice();
  //delay(5000);
  //Stops current audio file playing.
  //wtv020sd16p.stopVoice();
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
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Play the sound
  if(!wtv020sd16p.isBusy()) {
    Serial.println("Let's play some music !");
    wtv020sd16p.asyncPlayVoice(9);
    delay(50);
  }
  delay(5);
}

void mqttReconnect() {
  // Loop until we're reconnected
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqttClient.connect("jmd")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      mqttClient.publish("/outTopic", "hello world");
      // ... and resubscribe
      mqttClient.subscribe("inTopic");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
