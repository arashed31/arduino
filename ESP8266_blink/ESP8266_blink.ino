#include <ESP8266WiFi.h>
//#include <ESP8266SSDP.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266HTTPClient.h>
#include <WiFiUdp.h>
#include <Ticker.h>
#include "wifi.h"

/* ============================ WiFi  Section =============================== */
#define WIFI_HOST_NAME "esp8266-blink"

#define DBG Serial

void startWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setPhyMode(WIFI_PHY_MODE_11N);
  WiFi.setOutputPower(16.5);            // transmission power range 0 to +20.5 dBm
  WiFi.setAutoReconnect(HIGH);          // autoreconnect if connection lost
  WiFi.setSleepMode(WIFI_LIGHT_SLEEP);  // sets MCU & Radio to sleep mode when idle
  WiFi.hostname(WIFI_HOST_NAME);
  WiFi.begin(WIFI_SSID, WIFI_KEY);
  delay(1000);
}

void stopWiFi() {
  WiFi.disconnect();
  delay(1000);
  WiFi.mode(WIFI_OFF);
  delay(1000);
}

/* ============================ WiFi  Section =============================== */
/* =========================== Server  Section ============================== */
Ticker ipTask;
HTTPClient ipClient;
IPAddress publicIP;
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

#define WIFI_HELLO_STATEMENT "Hello from ESP8266 Blink!"

int myhour, myminute, mysecond;

void handleRoot() {
  String message = String(WIFI_HELLO_STATEMENT);
  message += String("\n\nCurrent IP: ");
  message += publicIP.toString();
  message += String("\nCurrent Time: ");
  message += String(myhour) + String(':') + String(myminute) + String(':') + String(mysecond);
  httpServer.send(200, "text/plain", message);
  DBG.println(message);
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
  DBG.println(message);
}

int ipUpdate = 0;
void triggerIP() {
  ipUpdate = 1;
}

void updateIP() {
  if (ipUpdate) {
    ipUpdate = 0;
    ipClient.begin("http://checkip.dyndns.com/");
    ipClient.GET();
    delay(500);
    yield();
    delay(500);
    
    String result = ipClient.getString();
    ipClient.end();
    
    int stringIPa = result.indexOf("Address: ");
    int stringIPb = result.indexOf("</body>");
    
    if  ((stringIPa > 0) && (stringIPb > 0)) {
      result = result.substring(stringIPa+9, stringIPb);
      IPAddress tempaddr;
      if (tempaddr.fromString(result)) {
        publicIP = tempaddr;
        DBG.println(publicIP);
      }
    }
  }
}

void updateServer() {
  httpServer.handleClient();
}

void startServer() {
  httpUpdater.setup(&httpServer);

  httpServer.on("/", handleRoot);
  httpServer.on("/favicon.ico", [](){/* do nothing */} );
  httpServer.on("/index.html", HTTP_GET, [](){httpServer.send(200, "text/plain", "Hello World!");});
  httpServer.on("/description.xml", HTTP_GET, [](){/*SSDP.schema(httpServer.client());*/});
  httpServer.onNotFound(handleNotFound);
  httpServer.begin();
  yield();
  MDNS.begin(WIFI_HOST_NAME);
  MDNS.addService("http", "tcp", 80);
  yield();
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
  yield();
  */

  //ipTask.attach(60, triggerIP);
}

/* =========================== Server  Section ============================== */
/* ============================= NTP Section ================================ */
WiFiUDP ntpClient;
Ticker ntpTask;
IPAddress timeServerIP;
const int NTP_PACKET_SIZE = 48;
const char* ntpServerName = "time.nist.gov";
byte packetBuffer[NTP_PACKET_SIZE];

unsigned long ICACHE_FLASH_ATTR sendNTPpacket()
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  ntpClient.beginPacket(timeServerIP, 123); //NTP requests are to port 123
  ntpClient.write(packetBuffer, NTP_PACKET_SIZE);
  ntpClient.endPacket();
}

int ntpUpdate = 0;
void triggerNTP() {
  ntpUpdate = 1;
}

void updateNTP() {
  if (ntpUpdate) {
    ntpUpdate = 0;
    WiFi.hostByName(ntpServerName, timeServerIP);
    sendNTPpacket();
    //delay(500);

    if (ntpClient.parsePacket()) {
      ntpClient.read(packetBuffer, NTP_PACKET_SIZE);
      unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
      unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
      unsigned long secsSince1900 = highWord << 16 | lowWord;
      const unsigned long seventyYears = 2208988800UL;
      unsigned long epoch = secsSince1900 - seventyYears; // unix time
  
      myhour = (epoch  % 86400L) / 3600;
      myminute = (epoch  % 3600) / 60;
      mysecond = epoch % 60;
      
      DBG.print("The UTC time is ");
      if ( myhour < 10 ) DBG.print('0');
      DBG.print(myhour);
      DBG.print(':');
      if ( myminute < 10 ) DBG.print('0');
      DBG.print(myminute);
      DBG.print(':'); 
      if ( mysecond < 10 ) DBG.print('0');
      DBG.println(epoch % 60);
    }
  }
}

void startNTP() {
  ntpClient.begin(2390);
  delay(500);
  yield();
  delay(500);
  ntpTask.attach(360, triggerNTP);
}

/* ============================= NTP Section ================================ */
/* =========================== Blinky  Section ============================== */
Ticker blinkyTask;

int LED_LEVEL = 1;
int LED_COUNT = 0;

void blinky() {
  digitalWrite(LED_BUILTIN, LED_LEVEL);
  DBG.println(++LED_COUNT);
  if (LED_LEVEL)
    LED_LEVEL = 0;
  else
    LED_LEVEL = 1;
}

void startBlinky() {
  pinMode(LED_BUILTIN, LED_LEVEL);
  blinkyTask.attach(1, blinky);
}

/* =========================== Blinky  Section ============================== */
/* ============================ Main  Section =============================== */
void setup() {
  startWiFi();
  startServer();
  startNTP();
  startBlinky();

  DBG.begin(74880);
  DBG.println("started");
  triggerIP();
  updateIP();
  triggerNTP();
}

void loop() {
  updateServer();
  delay(500);
  updateNTP();
  delay(500);
  //updateIP();
}

/* ============================ Main  Section =============================== */
