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
#include <EEPROM.h>
#include <ArduinoOTA.h>

const char* ver = "20170928";
const char* ssid = "Xiaomi_33C7";
const char* password = "duoguanriben8";
const float offset = 0.0;
boolean isOnline;
unsigned long timeStamp;
float temp;
int stat, pwm, border[2];

ESP8266WebServer server(80);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "ntp1.aliyun.com", 28800, 60000);

void readConf() {
  EEPROM.begin(3);
  if (EEPROM.read(0) != 0x39) {
    EEPROM.write(0, 0x39);
    EEPROM.write(1, 30);
    EEPROM.write(2, 50);
  }
  border[0] = EEPROM.read(1);
  border[1] = EEPROM.read(2);
  EEPROM.end();
}

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

boolean connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.printf("\nConnecting to %s\n", ssid);
  for (int i = 0; i < 10; i++) {
    delay(1000);
    if (WiFi.status() != WL_CONNECTED) {
      Serial.print(">");
    } else {
      break;
    }
  }
  Serial.print(" ");
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Pass");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    server.on("/", handleRoot);
    server.begin();
    Serial.println("HTTP server started");
    timeClient.begin();
    Serial.println("NTP client started");
    otaCfg();
    ArduinoOTA.begin();
    Serial.println("OTA service started");
    return true;
  } else {
    Serial.println("Fail");
    WiFi.disconnect(true);
    return false;
  }
}

void dataProcess() {
  if (millis() - timeStamp > 250) {
    temp = analogRead(A0) / 1024.0 * 100 + offset;
    pwm = (50 - temp) / 20 * 100;
    if (pwm < 0 or pwm > 100) pwm = -1;
    timeStamp = millis();
  }
}

void handleRoot() {
  String msg;
  StaticJsonBuffer<256> jsonBuffer;
  JsonObject& json = prepareResponse(jsonBuffer);
  json.prettyPrintTo(msg);
  server.send(200, "application/json", msg);
}

JsonObject& prepareResponse(JsonBuffer& jsonBuffer) {
  JsonObject& root = jsonBuffer.createObject();
  root["mac"] = getMAC();
  root["version"] = ver;
  root["time"] = timeClient.getFormattedTime();
  root["stat"] = stat;
  root["temp"] = temp;
  root["pwm"] = pwm;
  JsonArray& borderJson = root.createNestedArray("border");
  borderJson.add(border[0]);
  borderJson.add(border[1]);
  return root;
}

void otaCfg(void) {
  ArduinoOTA.setPassword((const char *)"ILWT");
  ArduinoOTA.onStart([]() {
    Serial.println("OTA start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nOTA end");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("OTA progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
}

void setup(void) {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  readConf();
  isOnline = connectWifi();
}

void loop(void) {
  digitalWrite(LED_BUILTIN, LOW);
  dataProcess();
  if (isOnline) {
    server.handleClient();
    timeClient.update();
    ArduinoOTA.handle();
  }
  digitalWrite(LED_BUILTIN, HIGH);
}

