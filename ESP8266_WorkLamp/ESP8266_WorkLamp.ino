#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>

#include <IRsend.h>

#include "WemoManager.h"
#include "CallbackFunction.h"

#include "wifi.h"

//on/off callbacks
void lightOn();
void lightOff();
void livingroomOn();
void livingroomOff();

ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

IRsend irsend(2);
uint16_t soundbarPower[77] = {4518, 4498,  520, 496,  506, 498,  506, 522,  480, 498,  504, 1526,  482, 1502,  504, 498,  506, 498,  504, 1502,  506, 1526,  482, 1526,  482, 1526,  482, 500,  504, 500,  504, 498,  506, 522,  482, 4522,  482, 522,  482, 522,  480, 524,  480, 500,  504, 498,  504, 500,  504, 522,  482, 498,  504, 1526,  482, 1504,  504, 1526,  480, 500,  506, 1526,  480, 1502,  504, 1526,  482, 1502,  504, 498,  506, 500,  506, 496,  506, 1528,  480};  // UNKNOWN CA31DA45
uint64_t emersonPower = 0x210704FB;
uint64_t stripOn = 0xF7C03F;
uint64_t stripOff = 0xF740BF;

WemoManager wemoManager;
WemoSwitch light;
WemoSwitch livingroom = NULL;

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

  irsend.begin();
}

void updateWeMotes() {
  wemoManager.serverLoop();
}

void startWeMotes() {
  // work lamp
  pinMode(lampPin, OUTPUT); // initialize digital lampPin as an output
  digitalWrite(lampPin, HIGH); // boot up with lamp turned on
  
  // Format: Alexa invocation name, local port no, on callback, off callback
  light = WemoSwitch("work lamp", 65500 + numWemos++, lightOn, lightOff);
  livingroom = new WemoSwitch("living room TV", 65500 + numWemos++, livingroomOn, livingroomOff);
  wemoManager.addDevice(light);
  wemoManager.addDevice(livingroom);
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
  irsend.begin();
  delay(100);
  Serial.begin(115200);
  digitalWrite(2,LOW);
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
  Serial.println("Work Lamp on....");
  digitalWrite(lampPin, HIGH);
}

void lightOff() {
  Serial.println("Work Lamp off...");
  digitalWrite(lampPin, LOW);
}

void livingroomOn() {
  Serial.println("Living Room TV on....");
  irsend.sendRaw(soundbarPower, 77, 38);
  delay(500);
  irsend.sendNEC(emersonPower, 32);
  irsend.sendNEC(stripOn, 32);
}

void livingroomOff() {
  Serial.println("Living Room TV off...");
  irsend.sendNEC(emersonPower, 32);
  irsend.sendNEC(stripOff, 32);
}

