#include <Ticker.h>
#include <TinyGPS++.h>
#include <ESP8266WiFi.h>
#include <ESP8266SSDP.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266HTTPClient.h>
#include "wifi.h"

/* Description
 * This sketch connects to a 4G hotspot with built in GPS. The hotspot can stream
 * NEMA GPS strings output over TCP from port 11010. Just connect and listen.
 */

/* ============================ WiFi  Section =============================== */

void startWiFi() {
  while(!WiFi.mode(WIFI_STA));
  while(!WiFi.setPhyMode(WIFI_PHY_MODE_11N));
  WiFi.setOutputPower(16.5);            // transmission power range 0 to +20.5 dBm
  WiFi.setAutoReconnect(HIGH);          // autoreconnect if connection lost
  WiFi.setSleepMode(WIFI_LIGHT_SLEEP);  // sets MCU & Radio to sleep mode when idle
  WiFi.hostname(WIFI_HOST_NAME);
  WiFi.begin(WIFI_SSID, WIFI_KEY);
  delay(100);
}

void stopWiFi() {
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  delay(100);
}

/* ============================ WiFi  Section =============================== */
/* =========================== Server  Section ============================== */
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

int myhour, myminute, mysecond;
IPAddress publicIP;

void handleRoot() {
  String message = WIFI_HELLO_STATEMENT;
  message += String("\n\nCurrent Time: ");
  message += String(myhour) + String(':') + String(myminute) + String(':') + String(mysecond);
  httpServer.send(200, "text/plain", message);
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += httpServer.uri();
  message += "\nMethod: ";
  message += (httpServer.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += httpServer.args();
  message += "\n";
  for (uint8_t i=0; i<httpServer.args(); i++){
    message += " " + httpServer.argName(i) + ": " + httpServer.arg(i) + "\n";
  }
  httpServer.send(404, "text/plain", message);
  Serial.print(message);
}

void startServer() {
  httpUpdater.setup(&httpServer);

  httpServer.on("/", handleRoot);
  httpServer.on("/favicon.ico", [](){/* do nothing */} );
  httpServer.on("/index.html", HTTP_GET, [](){httpServer.send(200, "text/plain", "Hello World!");});
  httpServer.on("/description.xml", HTTP_GET, [](){SSDP.schema(httpServer.client());});
  httpServer.onNotFound(handleNotFound);
  httpServer.begin();

  MDNS.begin(WIFI_HOST_NAME);
  MDNS.addService("http", "tcp", 80);
/*
  SSDP.setSchemaURL("description.xml");
  SSDP.setHTTPPort(80);
  SSDP.setName("Philips hue clone");
  SSDP.setURL("index.html");
  SSDP.setSerialNumber("001788102201");
  SSDP.setModelName("Philips hue bridge 2012");
  SSDP.setModelNumber("929000226503");
  SSDP.setModelURL("http://www.meethue.com");
  SSDP.setManufacturer("Royal Philips Electronics");
  SSDP.setManufacturerURL("http://www.philips.com");
  SSDP.begin();
*/
}

void updateServer() {
  httpServer.handleClient();
}

/* =========================== Server  Section ============================== */
/* ============================  GPS  Section  ============================== */
TinyGPSPlus gps;
IPAddress gpsServer(192,168,1,1);   // address of hotspot with GPS
WiFiClient gpsClient;
HTTPClient checkip;

void updateGPS() {
  if (gpsClient.connected()) {
    while (gpsClient.available()) {
      char c = gpsClient.read();
      gps.encode(c);
    }
  } else {
    gpsClient.stop();
    gpsClient.connect(gpsServer, 11010);    // TCP port for NEMA string stream
    delay(100);
  }
}

Ticker tickerTime;
void updateTime() {
  if (gps.time.isValid())
  {
    myhour = (gps.time.hour()-5);    
    myminute = gps.time.minute();
    mysecond = gps.time.second();
    if (myhour < 0) myhour += 24;
  }
  
}

unsigned long lastIP = 0;
void updateIP() {
  if ((millis()-10000) > lastIP) {
    checkip.begin("http://checkip.dyndns.com/");
    checkip.GET();
    delay(1000);
    
    String result = checkip.getString();
    checkip.end();
    
    int stringIPa = result.indexOf("Address: ");
    int stringIPb = result.indexOf("</body>");
    
    if  ((stringIPa > 0) && (stringIPb > 0)) {
      result = result.substring(stringIPa+9, stringIPb);
      if (publicIP.fromString(result)) {
        Serial.println(publicIP);
        lastIP = millis();
      }
    }
  }
}

/* ============================  GPS  Section  ============================== */
/* ===========================  Main  Section  ============================== */
void setup() {
  startWiFi();
  startServer();

  Serial.begin(74880);
  Serial.println("started");
}

void loop() {
  updateGPS();
  updateTime();
  updateServer();
  updateIP();
}

/* ===========================  Main  Section  ============================== */
