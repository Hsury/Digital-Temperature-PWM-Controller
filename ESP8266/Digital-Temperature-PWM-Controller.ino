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

const char* ver = "20171005";
const char* update_path = "/ota";
const char* update_username = "ILWT";
const char* update_password = "ILWT";
const char* ssid = "Xiaomi_33C7";
const char* password = "duoguanriben8";
const String cmdList[4] = {"restart", "serialPrint", "setOffset", "setBorder"};

boolean isOnline;
unsigned int localUdpPort = 8266;
char incomingPacket[255];
char  replyPacekt[] = "Got it, ILWT!\n";
float temp, offset;
String tempStr;
int pwm, border[2];
unsigned long timeStamp[2];

WiFiUDP mainUDP, ntpUDP;
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;
NTPClient timeClient(ntpUDP, "ntp1.aliyun.com", 28800, 10000);

void bannerPrint(void) {
  Serial.println();
  Serial.println("Digital Temperature PWM Controller");
  Serial.printf("MAC address: %s", getMAC().c_str());
  Serial.println();
  Serial.printf("Software version: %s", ver);
  Serial.println();
}

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
  ArduinoOTA.setPort(6628);
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
  Serial.printf("Connecting to %s ", ssid);
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
    mainUDP.begin(localUdpPort);
    Serial.printf("UDP port: %u", localUdpPort);
    Serial.println();
    Serial.println("UDP listener started");
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
    if (temp < 0) temp = 0;
    if (temp > 100) temp = 100;
    tempStr = String(int(temp * 10 + 0.5) / 10.0);
    tempStr = tempStr.substring(0, tempStr.length() - 1);
    pwm = (border[1] - temp) / (border[1] - border[0]) * 100;
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

void udpHandle(WiFiUDP& udp) {
  int packetSize = udp.parsePacket();
  if (packetSize)
  {
    int len = udp.read(incomingPacket, 255);
    if (len > 0)
    {
      incomingPacket[len] = 0;
    }
    Serial.printf("UDP Received %d bytes from %s, port %d, packet contents: %s", packetSize, udp.remoteIP().toString().c_str(), udp.remotePort(), incomingPacket);
    Serial.println();
    udp.beginPacket(udp.remoteIP(), udp.remotePort());
    udp.write(replyPacekt);
    udp.endPacket();
    Serial.println("UDP reply packet sent");
    cmdCheck(incomingPacket);
  }
}

void cmdCheck(char* packet) {
  String raw = String(packet);
  String parameter;
  for (int i = 0; i < sizeof(cmdList) / sizeof(cmdList[0]); i++) {
    if (raw.startsWith(cmdList[i] + ":") and raw.endsWith(";")) {
      parameter = raw.substring(cmdList[i].length() + 1, raw.length() - 1);
      Serial.printf("Command \"%s\" got, parameter: \"%s\"", cmdList[i].c_str(), parameter.c_str());
      Serial.println();
      switch (i) {
        case 0:
          Serial.println("Restarting...");
          ESP.restart();
          break;
        case 1:
          Serial.printf("Temperature: %s, PWM: %d, Offset: ", tempStr.c_str(), pwm);
          Serial.print(offset);
          Serial.printf(", Border: {%d, %d}", border[0], border[1]);
          Serial.println();
          break;
        case 2:
          offset = parameter.toFloat();
          Serial.print("offset <= ");
          Serial.println(offset);
          break;
        case 3:
          if (parameter.indexOf(',') != -1) {
            unsigned int newBorder[2] = {parameter.substring(0, parameter.indexOf(',')).toInt(), parameter.substring(parameter.indexOf(',') + 1, parameter.length()).toInt()};
            if (newBorder[0] >= 0 and newBorder[0] < newBorder[1] and newBorder[1] <= 100) {
              border[0] = newBorder[0];
              border[1] = newBorder[1];
              Serial.printf("border <= {%d, %d}", border[0], border[1]);
              Serial.println();
            } else Serial.println("Value error");
          } else Serial.println("Format error");
          break;
      }
      break;
    }
  }
}

boolean ssd4Drv(uint8_t dataPin, uint8_t clockPin, uint8_t latchPin, uint8_t invl, unsigned long& timeStamp, char* str) {
  String text = String(str);
  int ptrBegin = (text.length() > 4) ? -3 : 0;
  int ptrEnd = (text.length() > 4) ? text.length() - 1 : 0;
  int i = ptrBegin;
  for (int j = 0; j++; j < 4) {
    uint8_t led;
    if (i + j >= 0 and i + j <= text.length() - 1) {
      switch (text[i + j]) {
        case '.':
          led = B00000001;
          break;

        case 'I':
          led = B01100000;
          break;

        case 'P':
          led = B11001110;
          break;

        default:
          led = 0;
          break;
      }
      if (!led & B00000001 and i + j + 1 >= 0 and i + j + 1 <= text.length() - 1 and text[i + j + 1] == '.') {
        led |= B00000001;
        i++;
      }
    } else {
      led = 0;
    }
    digitalWrite(latchPin, LOW);
    shiftOut(dataPin, clockPin, LSBFIRST, led);
    shiftOut(dataPin, clockPin, LSBFIRST, B10000000 << j);
    digitalWrite(latchPin, HIGH);
  }
  if (timeStamp == 0 or millis() - timeStamp > invl) {
    i++;
    if (i >= ptrEnd + 1) {
      i = ptrBegin;
    }
    timeStamp = millis();
  }
}

void setup(void) {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  bannerPrint();
  readConf();
  isOnline = connectWifi();
}

void loop(void) {
  digitalWrite(LED_BUILTIN, LOW);
  dataProcess();
  if (isOnline) {
    udpHandle(mainUDP);
    ArduinoOTA.handle();
    server.handleClient();
    timeUpdate();
  }
  digitalWrite(LED_BUILTIN, HIGH);
}

