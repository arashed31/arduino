#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266mDNS.h>
#include <NeoPixelBus.h>
#include <WiFiUdp.h>
#include "headlight.h"
#include "wifi.h"

/* ========================== NeoPixel  Section ============================= */
RgbColor amber(160, 40, 0);
RgbColor amberDim(40, 16, 0);
RgbColor white(100, 80, 80);
RgbColor red(120, 0, 0);
RgbColor orange(120, 40, 0);
RgbColor yellow(80, 70, 0);
RgbColor green(0, 96, 0);
RgbColor blue(0, 0, 160);
RgbColor purple(100, 0, 80);
RgbColor black(0);

volatile uint32_t hexcolor;
volatile int headlightReboot;
volatile unsigned long turning;

const int cornerLength = 16;
volatile int cornerStripShow;
NeoPixelBus<NeoGrbFeature, NeoEsp8266Dma800KbpsMethod> cornerStrip(cornerLength);
RgbColor cornerColor = amberDim;

#ifdef HALO
const int haloLength = 37;
volatile int haloStripShow;
NeoPixelBus<NeoGrbFeature, NeoEsp8266Uart800KbpsMethod> haloStrip(haloLength);
RgbColor haloColor = black;
#endif

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
#ifdef HALO
    for (i = 0; i < haloLength; i++) {
      haloStrip.SetPixelColor(i, amberDim);
    }
#endif
  }
  /* if turn signal just turned on */
  else {
    for (i = 0; i < cornerLength; i++) {
      cornerStrip.SetPixelColor(i, amber);
    }
#ifdef HALO
    for (i = 0; i < haloLength; i++) {
      haloStrip.SetPixelColor(i, amber);
    }
#endif
  }
  cornerStripShow = 1;
#ifdef HALO
  haloStripShow = 1;
#endif
}

/* Counter-Sync Halo Strip with low beam */
void lowISR() {
#ifdef HALO
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
#endif
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
#ifdef HALO
  haloColor = RgbColor(HtmlColor(hexcolor));
#endif
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
  //handleRoot();

  updateColors();
}

void handleColorHEX() {
  char colorBuffer[8];
  httpServer.arg(0).toCharArray(colorBuffer, 8);
  hexcolor = strtol(colorBuffer, NULL, 16);

  httpServer.send(200, "text/plain", "ok");
  //handleRoot();

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
  cornerStrip.SetPixelColor(14, purple);
  cornerStrip.SetPixelColor(15, purple);
  cornerStripShow = 1;

  httpServer.send(200, "text/plain", "ok");
  //handleRoot();
}

void handleNotFound() {
  String message = "<html><head><title>404</title>";
  message += "<meta http-equiv=\"refresh\" content=\"3; URL=/\"></head><body><p>";
  message += "File Not Found<br /><br />";
  message += "URI: " + httpServer.uri();
  message += " <br /> Method: " + httpServer.method();
  message += " <br /> Arguments: " + httpServer.args();
  for (uint8_t i = 0; i < httpServer.args(); i++) {
    message += "<br /> &nbsp;&nbsp;" + httpServer.argName(i) + ": " + httpServer.arg(i);
  }
  message += "<b /></p></body></html>";
  httpServer.send(404, "text/html", message);
  //Serial.println(message);
}

void handleReboot() {
  headlightReboot = 1;
  httpServer.send(200, "text/plain", "ok");
}

void startServer() {
  hexcolor = 0;
  httpUpdater.setup(&httpServer, "/update", UPDATE_USR, UPDATE_KEY);
  httpServer.on("/", handleRoot);
  httpServer.on("/gay", handleGayCorner);
  httpServer.on("/help", handleHelpMenu);
  httpServer.on("/reboot", handleReboot);
  httpServer.on("/setrgb", handleColorRGB);
  httpServer.on("/sethex", handleColorHEX);
  httpServer.onNotFound(handleNotFound);
  httpServer.begin();
  delay(50);
  MDNS.begin(WIFI_HOST_NAME);
  delay(50);
  MDNS.addService("http", "tcp", 80);
  delay(50);
}

void updateServer() {
  httpServer.handleClient();
}

/* =========================== Server  Section ============================== */
/* ========================== Headlight Section ============================= */
WiFiUDP headlightClient;
IPAddress headlightMulticast(228,232,238,246);
const unsigned int headlightPort = 23808;

void startHeadlights() {
  pinMode(lowPin, INPUT);
  pinMode(turnPin, INPUT);
  
  turning = 0;
  headlightReboot = 0;

  cornerStripShow = 0;
  cornerStrip.Begin();
  attachInterrupt(digitalPinToInterrupt(turnPin), turnISR, CHANGE);

#ifdef HALO
  haloStripShow = 0;
  haloStrip.Begin();
  attachInterrupt(digitalPinToInterrupt(lowPin), lowISR, CHANGE);
#endif

  headlightClient.beginMulticast(WiFi.localIP(), headlightMulticast, headlightPort);
}

void updateHeadlights() {
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
/* ===========================  Main  Section  ============================== */
void setup() {
  while(!WiFi.mode(WIFI_STA));
  while(!WiFi.setPhyMode(WIFI_PHY_MODE_11B));
  WiFi.setOutputPower(20.5);
  delay(100);
  WiFi.hostname(WIFI_HOST_NAME);
  WiFi.begin(WIFI_SSID, WIFI_KEY);
  delay(100);
  startServer();        // start HTTPWebServer
  startHeadlights();    // turn on lights & start UDP listener
}

void loop() {
  if (turning) {
    if (millis() > turning) {
      turning = 0;
      turnLightsOn();
      yield();
    }
  }

  if (cornerStripShow) {
    cornerStrip.Show();
    cornerStripShow = 0;
    yield();
  }

#ifdef HALO
  if (haloStripShow) {
    haloStrip.Show();
    haloStripShow = 0;
    yield();
  }
#endif

  updateServer();
  updateHeadlights();

  if (headlightReboot) {
    delay(250);
    delay(250);
    ESP.restart();
  }

  delay(15);
}
/* ===========================  Main  Section  ============================== */

