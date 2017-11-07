#include <WiFiUdp.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <NeoPixelBus.h>
#include "headlight.h"
#include "wifi.h"

/* ========================== NeoPixel  Section ============================= */
RgbColor amber(192, 50, 0);
RgbColor amberDim(40, 16, 0);
RgbColor white(100, 80, 80);
RgbColor red(120, 0, 0);
RgbColor orange(140, 48, 0);
RgbColor yellow(80, 80, 0);
RgbColor green(0, 100, 0);
RgbColor blue(0, 0, 128);
RgbColor purple(120, 0, 100);
RgbColor black(0);

volatile uint32_t hexcolor;
volatile unsigned long turning;

const int cornerLength = 18;
volatile int cornerStripShow;
NeoPixelBus<NeoGrbFeature, NeoEsp8266Dma800KbpsMethod> cornerStrip(cornerLength);
RgbColor cornerColor = amber;

const int haloLength = 37;
volatile int haloStripShow;
NeoPixelBus<NeoGrbFeature, NeoEsp8266Uart800KbpsMethod> haloStrip(haloLength);
RgbColor haloColor = amberDim;

/* Synchronize Corner Strip with turn signal */
void turnISR() {
  int i;
  turning = millis() + 1250;
  int turnEdge = digitalRead(turnPin);
  /* if turn signal just turned off */
  if (turnEdge) {
    for (i = 0; i < cornerLength; i++) {
      cornerStrip.SetPixelColor(i, black);
    }
    for (i = 0; i < haloLength; i++) {
      haloStrip.SetPixelColor(i, amberDim);
    }
  }
  /* if turn signal just turned on */
  else {
    for (i = 0; i < cornerLength; i++) {
      cornerStrip.SetPixelColor(i, amber);
    }
    for (i = 0; i < haloLength; i++) {
      haloStrip.SetPixelColor(i, amber);
    }
  }
  cornerStripShow = 1;
  haloStripShow = 1;
}

/* Counter-Sync Halo Strip with low beam */
void lowISR() {
  int i;
  int lowEdge = digitalRead(lowPin);
  /* if low beam just turned off */
  if (lowEdge) {
    for (i = 0; i < haloLength; i++) {
      haloStrip.SetPixelColor(i, haloColor);
    }
  }
  /* if low beam just turned on */
  else {
    for (i = 0; i < haloLength; i++) {
      haloStrip.SetPixelColor(i, black);
    }
  }
  haloStripShow = 1;
}

/* Turn on Strips at startup */
void turnLightsOn() {
  lowISR();
  int i;
  for (i = 0; i < cornerLength; i++) {
    cornerStrip.SetPixelColor(i, cornerColor);
  }
  cornerStripShow = 1;
}

void updateColors() {
  cornerColor = RgbColor(HtmlColor(hexcolor));
  haloColor = RgbColor(HtmlColor(hexcolor));
  turnLightsOn();
}

/* ========================== NeoPixel  Section ============================= */
/* =========================== Server  Section ============================== */
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

void handleRoot() {
  String message = WIFI_HELLO_STATEMENT;
  message += "\n0x";
  message += String(hexcolor, HEX);
  httpServer.send(200, "text/plain", message);
}

void handleHelpMenu() {
  char help_menu[] = "Help Menu:\n"
                     "1) /help - displays this help page"
                     "2) /setrgb?r=X&g=Y&b=Z - sets RGB color by Red = X, Green = Y, and Blue = Z\n"
                     "3) /sethex?hex=0x000000 - sets RGB color by HEX value\n"
                     "4) /gay - sets corner lights to rainbow color\n";
  httpServer.send(200, "text/plain", help_menu);
  //Serial.print(help_menu);
}

void handleColorRGB() {
  uint32_t r = httpServer.arg("r").toInt();
  uint32_t g = httpServer.arg("g").toInt();
  uint32_t b = httpServer.arg("b").toInt();
  hexcolor = (r << 16) | (g << 8) | b;

  httpServer.send(200, "text/plain", "ok");

  updateColors();
}

void handleColorHEX() {
  char colorBuffer[8];
  httpServer.arg(0).toCharArray(colorBuffer, 8);
  hexcolor = strtol(colorBuffer, NULL, 16);

  httpServer.send(200, "text/plain", "ok");

  updateColors();
}

void handleGayCorner() {
  cornerStrip.SetPixelColor(0, red);
  cornerStrip.SetPixelColor(1, red);
  cornerStrip.SetPixelColor(2, red);
  cornerStrip.SetPixelColor(3, orange);
  cornerStrip.SetPixelColor(4, orange);
  cornerStrip.SetPixelColor(5, orange);
  cornerStrip.SetPixelColor(6, yellow);
  cornerStrip.SetPixelColor(7, yellow);
  cornerStrip.SetPixelColor(8, yellow);
  cornerStrip.SetPixelColor(9, green);
  cornerStrip.SetPixelColor(10, green);
  cornerStrip.SetPixelColor(11, green);
  cornerStrip.SetPixelColor(12, blue);
  cornerStrip.SetPixelColor(13, blue);
  cornerStrip.SetPixelColor(14, blue);
  cornerStrip.SetPixelColor(15, purple);
  cornerStrip.SetPixelColor(16, purple);
  cornerStrip.SetPixelColor(17, purple);
  cornerStripShow = 1;

  httpServer.send(200, "text/plain", "ok");
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += httpServer.uri();
  message += "\nMethod: ";
  message += (httpServer.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += httpServer.args();
  message += "\n";
  for (uint8_t i = 0; i < httpServer.args(); i++) {
    message += " " + httpServer.argName(i) + ": " + httpServer.arg(i) + "\n";
  }
  httpServer.send(404, "text/plain", message);
  //Serial.print(message);
}

void startServer() {
  httpUpdater.setup(&httpServer);
  httpServer.on("/", handleRoot);
  httpServer.on("/help", handleHelpMenu);
  httpServer.on("/setrgb", handleColorRGB);
  httpServer.on("/sethex", handleColorHEX);
  httpServer.on("/gay", handleGayCorner);
  httpServer.onNotFound(handleNotFound);
  httpServer.begin();
  delay(100);
  yield();
}

void updateServer() {
  httpServer.handleClient();
  delay(10);
  yield();
}

/* =========================== Server  Section ============================== */
/* ========================== Headlight Section ============================= */
WiFiUDP headlightClient;
IPAddress headlightMulticast(224,0,0,0);
const unsigned int headlightPort = 23808;

void startHeadlights() {
  turning = 1;
  hexcolor = 0;
  haloStripShow = 0;
  cornerStripShow = 0;
  
  haloStrip.Begin();
  cornerStrip.Begin();
  turnLightsOn();

  pinMode(lowPin, INPUT);
  attachInterrupt(digitalPinToInterrupt(lowPin), lowISR, CHANGE);
  pinMode(turnPin, INPUT);
  attachInterrupt(digitalPinToInterrupt(turnPin), turnISR, CHANGE);

  yield();
  headlightClient.beginMulticast(WiFi.localIP(), headlightMulticast, headlightPort);
  yield();
  delay(100);
  yield();
}

void updateHeadlights() {
  delay(100);
  yield();
  delay(100);
  yield();
  if (headlightClient.parsePacket()) {
    char colorBuffer[14];
    size_t len = headlightClient.read(colorBuffer, 14);
    colorBuffer[len] = '\0';
    if (String(colorBuffer).indexOf("_") > 0) {
      hexcolor = strtol(colorBuffer, NULL, 16);
      updateColors();
    }
  }
}

/* ========================== Headlight Section ============================= */
/* ===========================  WiFi  Section  ============================== */

void startWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.hostname(WIFI_HOST_NAME);
  WiFi.setPhyMode(WIFI_PHY_MODE_11G);
  WiFi.setOutputPower(20);               // transmission power range 0 to +20.5 dBm
  WiFi.setSleepMode(WIFI_LIGHT_SLEEP);  // sets MCU & Radio to sleep mode when idle
  WiFi.begin(WIFI_SSID, WIFI_KEY);
  delay(500);
  yield();
}

/* ===========================  WiFi  Section  ============================== */
/* ===========================  Main  Section  ============================== */
void setup() {
  delay(100);
  yield();
  delay(100);
  yield();
  startWiFi();
  startServer();        // start HTTPWebServer 
  startHeadlights();    // turn on lights & start UDP listener
}

void loop() {
  if (turning) {
    if (millis() > turning) {
      turning = 0;
      turnLightsOn();
    }
  }
  if (haloStripShow) {
    haloStrip.Show();
    haloStripShow = 0;
  }
  if (cornerStripShow) {
    cornerStrip.Show();
    cornerStripShow = 0;
  }
  updateServer();
  updateHeadlights();
}
/* ===========================  Main  Section  ============================== */

