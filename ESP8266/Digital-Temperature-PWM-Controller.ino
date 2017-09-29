/*
   Product : Digital Temperature PWM Controller
    Author : Ruoyang Xu
   Website : http://hsury.com/
*/

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <ESP8266HTTPUpdateServer.h>
#include <NTPClient.h>
#include <EEPROM.h>
#include <ArduinoJson.h>

const char* ver = "20170929";
const char* update_path = "/ota";
const char* update_username = "ILWT";
const char* update_password = "ILWT";
const char* ssid = "Xiaomi_33C7";
const char* password = "duoguanriben8";

boolean isOnline;
float temp, offset;
String tempStr;
int stat, pwm, border[2];
unsigned long timeStamp[2];

ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "ntp1.aliyun.com", 28800, 10000);

void readConf(void) {
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

String getMAC(void) {
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

void otaSetup(void) {
  ArduinoOTA.setPassword(update_password);
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
  ArduinoOTA.begin();
  httpUpdater.setup(&server, update_path, update_username, update_password);
}

boolean connectWifi(void) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.printf("\nConnecting to %s\n", ssid);
  for (int i = 0; i < 20; i++) {
    delay(500);
    if (WiFi.status() != WL_CONNECTED) {
      Serial.print(">");
    } else {
      break;
    }
  }
  Serial.print(" ");
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("OK");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    otaSetup();
    Serial.println("OTA service started");
    MDNS.addService("http", "tcp", 80);
    Serial.println("MDNS responder started");
    server.on("/", handleRoot);
    server.begin();
    Serial.println("HTTP server started");
    timeClient.begin();
    Serial.println("NTP client started");
    return true;
  } else {
    Serial.println("FAIL");
    WiFi.disconnect(true);
    return false;
  }
}

void dataProcess(void) {
  if (timeStamp[0] == 0 or millis() - timeStamp[0] > 250) {
    temp = analogRead(A0) / 1024.0 * 100 + offset;
    tempStr = int(temp * 10 + 0.5) / 10.0;
    tempStr = tempStr.substring(0, tempStr.length() - 1);
    pwm = (50 - temp) / 20 * 100;
    if (pwm < 0) pwm = 0;
    if (pwm > 100) pwm = 100;
    timeStamp[0] = millis();
  }
}

void handleRoot(void) {
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
  root["temp"] = tempStr;
  root["pwm"] = pwm;
  JsonArray& borderJson = root.createNestedArray("border");
  borderJson.add(border[0]);
  borderJson.add(border[1]);
  return root;
}

void timeUpdate(void) {
  if (timeStamp[1] == 0 or millis() - timeStamp[1] > 30000) {
    timeClient.update();
    timeStamp[1] = millis();
  }
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
    ArduinoOTA.handle();
    server.handleClient();
    timeUpdate();
  }
  digitalWrite(LED_BUILTIN, HIGH);
}

