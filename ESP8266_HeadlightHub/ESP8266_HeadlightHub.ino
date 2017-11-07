#include <WiFiUdp.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <Ticker.h>
#include "wifi.h"

/* ============================ WiFi  Section =============================== */
void startWiFi() {
  //WiFi.mode(WIFI_STA);
  WiFi.mode(WIFI_AP);
  WiFi.hostname(WIFI_HOST_NAME);
  WiFi.setPhyMode(WIFI_PHY_MODE_11G);
  WiFi.setOutputPower(20.5);              // transmission power range 0 to +20.5 dBm
  WiFi.setSleepMode(WIFI_LIGHT_SLEEP);  // sets MCU & Radio to sleep mode when idle

  //WiFi.begin(WIFI_SSID, WIFI_KEY);
  WiFi.softAP(WIFI_SSID, WIFI_KEY, 1, 1); //create hidden network
}

/* ============================ WiFi  Section =============================== */
/* =========================== Server  Section ============================== */
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

volatile uint32_t hexcolor;
volatile int headlightShow, httpRespond;

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

void handleColorRGB() {
  uint32_t r = httpServer.arg("r").toInt();
  uint32_t g = httpServer.arg("g").toInt();
  uint32_t b = httpServer.arg("b").toInt();

  hexcolor = (r << 16) | (g << 8) | (b);
  headlightShow = 1;
  httpRespond = 1;
  //Serial.println(hexcolor,HEX);
}

void handleColorHEX() {
  char colorBuffer[12];
  httpServer.arg(0).toCharArray(colorBuffer, 12);

  hexcolor = strtol(colorBuffer, NULL, 16);
  headlightShow = 1;
  httpRespond = 1;
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
  httpUpdater.setup(&httpServer);
  httpServer.on("/", handleRoot);
  httpServer.on("/favicon.ico", [](){ httpServer.send(404, "text/plain", ""); });
  httpServer.on("/help", handleHelpMenu);
  httpServer.on("/setrgb", handleColorRGB);
  httpServer.on("/sethex", handleColorHEX);
  httpServer.onNotFound(handleNotFound);
  httpServer.begin();
}

void updateServer() {
  if (httpRespond) {
    // tell http client job is being done
    httpServer.send(200, "text/plain", "OK");
    httpRespond = 0;
  }
  yield();
  delay(10);
  httpServer.handleClient();
  yield();
  delay(10);
}

/* =========================== Server  Section ============================== */
/* ========================== Headlight Section ============================= */
WiFiUDP headlightServer;
IPAddress headlightMulticast(224,0,0,0);
const unsigned int headlightPort = 23808;

void startHeadlights() {
  //headlightServer.beginMulticast(WiFi.softAPIP(), headlightMulticast, headlightPort);
}

void updateHeadlights() {
  if (headlightShow) {
    char sbuf[16];
    size_t len = sprintf(sbuf, "0x%08X_", hexcolor) + 1;
    yield();
    headlightServer.beginPacketMulticast(headlightMulticast, headlightPort, WiFi.softAPIP());
    yield();
    headlightServer.write(sbuf, len);
    yield();
    headlightServer.endPacket();
    yield();
    delay(200);
    yield();
    delay(200);
    if (headlightShow++ == 3)
      headlightShow = 0;
  } else {
    yield();
    delay(10);
  }
}

/* ========================== Headlight Section ============================= */
/* ========================== Heartbeat Section ============================= */
volatile unsigned long heartbeat;

void doHeartbeat() {
  if (millis() > heartbeat) {
    char sbuf[4] = "hey";
    yield();
    headlightServer.beginPacketMulticast(headlightMulticast, headlightPort, WiFi.softAPIP());
    yield();
    headlightServer.write(sbuf, 4);
    yield();
    headlightServer.endPacket();
    yield();
    heartbeat = millis() + 48000;
    yield();
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
  pinMode(3, OUTPUT);
  digitalWrite(0, LOW);
  digitalWrite(1, HIGH);
  digitalWrite(2, LOW);
  digitalWrite(3, LOW);
}

void loop() {
  updateServer();
  updateHeadlights();
  doHeartbeat();
  delay(10);
}

/* ============================ Main  Section =============================== */

