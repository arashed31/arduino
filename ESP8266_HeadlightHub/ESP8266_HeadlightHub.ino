#include <WiFiUdp.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <Ticker.h>
#include "wifi.h"
extern "C" {
#include "user_interface.h"
}

/* ============================ WiFi  Section =============================== */
void startWiFi() {
  //WiFi.mode(WIFI_STA);
  while(!WiFi.mode(WIFI_AP));
  while(!WiFi.setPhyMode(WIFI_PHY_MODE_11B));
  WiFi.setOutputPower(20.5);
  delay(100);
  WiFi.hostname(WIFI_HOST_NAME);
  //WiFi.begin(WIFI_SSID, WIFI_KEY);
  WiFi.softAP(WIFI_SSID, WIFI_KEY, 1, 1); //create hidden network
}

/* ============================ WiFi  Section =============================== */
/* =========================== Server  Section ============================== */
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

volatile uint32_t hexcolor;
volatile int headlightShow, httpRespond;

String STA_CLIENTS;
struct station_info *stat_info;
struct ip_addr *IPaddress;
IPAddress address;

void handleRoot() {
  String message = WIFI_HELLO_STATEMENT;
  message += "\n0x";
  message += String(hexcolor, HEX);
  httpServer.send(200, "text/plain", message);
  //Serial.println(message);
}

void handleHelpMenu() {
  char help_menu[] = "Help Menu:\n"
                     "1) /help - displays this help page"
                     "2) /setrgb?r=X&g=Y&b=Z - sets RGB color by Red = X, Green = Y, and Blue = Z (0 to 255 decimal values)\n"
                     "3) /sethex?hex=0x000000 - sets RGB color by HEX value\n";
  httpServer.send(200, "text/plain", help_menu);
  //Serial.print(help_menu);
}

void handleClients()
{
  STA_CLIENTS = "Clients:\n";
  stat_info = wifi_softap_get_station_info();
  while (stat_info != NULL)
  {
    IPaddress = &stat_info->ip;
    address = (IPaddress->addr >> 24) & 0xff;
    STA_CLIENTS += '\t';
    STA_CLIENTS += address;
    STA_CLIENTS += '\n';
    stat_info = STAILQ_NEXT(stat_info, next);
  }
  httpServer.send(200, "text/plain", STA_CLIENTS);
}

void handleColorRGB() {
  uint32_t r = httpServer.arg("r").toInt();
  uint32_t g = httpServer.arg("g").toInt();
  uint32_t b = httpServer.arg("b").toInt();

  hexcolor = (r << 16) | (g << 8) | (b);
  headlightShow = 1;
  httpServer.send(200, "text/plain", "OK");
  //Serial.println(hexcolor,HEX);
}

void handleColorHEX() {
  char colorBuffer[12];
  httpServer.arg(0).toCharArray(colorBuffer, 12);

  hexcolor = strtol(colorBuffer, NULL, 16);
  headlightShow = 1;
  httpServer.send(200, "text/plain", "OK");
  //Serial.println(hexcolor,HEX);
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += httpServer.uri();
  message += "\nMethod: ";
  message += httpServer.method() == HTTP_GET ? "GET" : "POST";
  message += "\nArguments: ";
  message += httpServer.args();
  message += "\n";
  for (uint8_t i = 0; i < httpServer.args(); i++) {
    message += " " + httpServer.argName(i) + ": " + httpServer.arg(i) + "\n";
  }
  httpServer.send(404, "text/plain", message);
  //Serial.println(message);
}

void startServer() {
  httpRespond = 0;
  httpUpdater.setup(&httpServer, "/update", UPDATE_USR, UPDATE_KEY);
  httpServer.on("/", handleRoot);
  httpServer.on("/favicon.ico", [](){ httpServer.send(404, "text/plain", ""); });
  httpServer.on("/help", handleHelpMenu);
  httpServer.on("/clients", handleClients);
  httpServer.on("/setrgb", handleColorRGB);
  httpServer.on("/sethex", handleColorHEX);
  httpServer.onNotFound(handleNotFound);
  httpServer.begin();
}

void updateServer() {
  httpServer.handleClient();
}

/* =========================== Server  Section ============================== */
/* ========================== Headlight Section ============================= */
WiFiUDP headlightServer;
IPAddress headlightMulticast(228,232,238,246);
const unsigned int headlightPort = 23808;

void startHeadlights() {
  headlightServer.beginMulticast(WiFi.softAPIP(), headlightMulticast, headlightPort);
}

void updateHeadlights() {
  if (headlightShow) {
    delay(200);
    char sbuf[16];
    size_t len = sprintf(sbuf, "0x%08X_\0", hexcolor) + 1;
    headlightServer.beginPacketMulticast(headlightMulticast, headlightPort, WiFi.softAPIP());
    headlightServer.write(sbuf, len);
    headlightServer.endPacket();
    delay(200);
    if (headlightShow++ == 3)
      headlightShow = 0;
  }
}

/* ========================== Headlight Section ============================= */
/* ========================== Heartbeat Section ============================= */
volatile unsigned long heartbeat;

void doHeartbeat() {
  if (millis() > heartbeat) {
    char sbuf[4] = "hey";
    headlightServer.beginPacketMulticast(headlightMulticast, headlightPort, WiFi.softAPIP());
    headlightServer.write(sbuf, 4);
    headlightServer.endPacket();
    heartbeat = millis() + 48000;
  }
}

/* ========================== Heartbeat Section ============================= */
/* ============================ Main  Section =============================== */
void setup() {
  startWiFi();
  startHeadlights();  // start nothing
  startServer();      // start http server
  heartbeat = millis() + 60000;

  pinMode(0, OUTPUT);
  pinMode(1, OUTPUT);
  pinMode(2, OUTPUT);
  pinMode(3, INPUT);
  digitalWrite(0, LOW);
  digitalWrite(1, LOW);
  digitalWrite(2, LOW);
}

void loop() {
  updateServer();
  updateHeadlights();
  //doHeartbeat();
  delay(100);
}

/* ============================ Main  Section =============================== */

