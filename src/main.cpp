#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include "Wtv020sd16p.h"

#define PIN_WTV_RESET D1
#define PIN_WTV_CLOCK D3
#define PIN_WTV_DATA D2
#define PIN_WTV_BUSY D5
#define PIN_TARDIS_LED D4

//////////

void configModeCallback(WiFiManager *myWiFiManager);

//////////

Wtv020sd16p wtv020sd16p(PIN_WTV_RESET, PIN_WTV_CLOCK, PIN_WTV_DATA, PIN_WTV_BUSY);

//////////

void setup() {
  Serial.begin(115200);

  pinMode(PIN_TARDIS_LED, OUTPUT);

  wtv020sd16p.reset();

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
  if(!wtv020sd16p.isBusy()) {
    Serial.println("Let's play some music !");
    wtv020sd16p.asyncPlayVoice(9);
    delay(50);
  }
  delay(5);

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
