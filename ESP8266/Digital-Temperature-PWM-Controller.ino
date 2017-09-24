/*
   Product : Digital Temperature PWM Controller
    Author : Ruoyang Xu
   Website : http://hsury.com/
*/

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>

const char* ver = "20170924";
const char* ssid = "HsuRY";
const char* password = "KAGAMIZ.COM";

ESP8266WebServer server(80);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "ntp1.aliyun.com", 28800, 60000);

int border[2] = {30, 50};

String getMAC() {
  byte mac[6];
  WiFi.macAddress(mac);
  String macstr;
  for (int i = 0; i < 6; i++) {
    if (mac[i] <= 0x0F) macstr += '0';
    macstr += String(mac[i], HEX);
    if (i < 5) macstr += '-';
  }
  macstr.toUpperCase();
  return macstr;
}

JsonObject& prepareResponse(JsonBuffer& jsonBuffer) {
  JsonObject& root = jsonBuffer.createObject();
  root["mac"] = getMAC();
  root["version"] = ver;
  root["time"] = timeClient.getFormattedTime();
  root["stat"] = 1;
  root["sensor"] = 27;
  root["bias"] = 0;
  root["pwm"] = 0;
  JsonArray& range = root.createNestedArray("range");
  range.add(border[0]);
  range.add(border[1]);
  return root;
}

void handleRoot() {
  String msg;
  StaticJsonBuffer<500> jsonBuffer;
  JsonObject& json = prepareResponse(jsonBuffer);
  json.prettyPrintTo(msg);
  server.send(200, "application/json", msg);
}

void handleNotFound() {

  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void setup(void) {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("esp8266")) {
    Serial.println("MDNS responder started");
  }

  server.on("/", handleRoot);

  server.on("/inline", []() {
    server.send(200, "text/plain", "this works as well");
  });

  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");

  timeClient.begin();
}

void loop(void) {
  digitalWrite(LED_BUILTIN, LOW);
  server.handleClient();
  timeClient.update();
  digitalWrite(LED_BUILTIN, HIGH);
}

