#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>

#include "WemoSwitch.h"
#include "WemoManager.h"
#include "CallbackFunction.h"

//on/off callbacks
void lightOn();
void lightOff();
void livingroomOn();
void livingroomOff();

ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

WemoManager wemoManager;
WemoSwitch *light = NULL;
WemoSwitch *livingroom = NULL;
int numWemos = 0;

const int ledPin = BUILTIN_LED;
const int lampPin = 2;

void startWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  Serial.print("Connecting Wifi: ");
  Serial.println(WIFI_SSID);
  
  WiFi.begin(WIFI_SSID, WIFI_KEY);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  
  WiFi.setAutoReconnect(HIGH);
  Serial.println();
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  IPAddress ip = WiFi.localIP();
  Serial.println(ip);
}

void updateWeMotes() {
  wemoManager.serverLoop();
}

void startWeMotes() {
  // work lamp
  pinMode(lampPin, OUTPUT); // initialize digital lampPin as an output
  digitalWrite(lampPin, HIGH); // boot up with lamp turned on
  
  // Format: Alexa invocation name, local port no, on callback, off callback
  light = new WemoSwitch("work lamp", 65500 + numWemos++, lightOn, lightOff);
  livingroom = new WemoSwitch("living room TV", 65500 + numWemos++, livingroomOn, livingroomOff);
  wemoManager.addDevice(*light);
  wemoManager.addDevice(*livingroom);
  wemoManager.begin();
}

void updateServer() {
  httpServer.handleClient();
}

void startServer() {
  httpUpdater.setup(&httpServer);
  
  httpServer.on("/", [](){/* do nothing */});
  httpServer.on("/favicon.ico", [](){/* do nothing */} );
  httpServer.onNotFound([](){/* do nothing */});
  httpServer.begin();
}

void setup()
{
  Serial.begin(74880);
  pinMode(0,OUTPUT);
  pinMode(1,OUTPUT);
  pinMode(2,OUTPUT);
  pinMode(3,OUTPUT);
  digitalWrite(0,LOW);
  digitalWrite(1,LOW);
  digitalWrite(2,LOW);
  digitalWrite(3,LOW);

  startWiFi();
  startWeMotes();
  startServer();
}

void loop()
{
  updateWeMotes();
  updateServer();
  delay(10);
}

void lightOn() {
    //Serial.println("Work Lamp on....");
    digitalWrite(lampPin, HIGH);
}

void lightOff() {
    //Serial.println("Work Lamp off...");
    digitalWrite(lampPin, LOW);
}

void livingroomOn() {
    //Serial.println("Living Room TV on....");
}

void livingroomOff() {
    //Serial.println("Living Room TV off...");
}

