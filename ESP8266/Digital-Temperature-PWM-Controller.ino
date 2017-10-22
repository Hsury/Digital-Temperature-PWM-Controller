/*
   Product : Digital Temperature PWM Controller
    Author : Ruoyang Xu
   Website : http://hsury.com/
*/

#define BORDER_L 30
#define BORDER_H 50
#define FITTING_K 100 / 1024.0
#define FITTING_B 40
#define SAMPLING_PERIOD 250
#define COOLDOWN_TIME 100

#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <SevenSegmentTM1637.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>

const byte pinCLK = 4;
const byte pinDIO = 5;
const byte pinPage = 14;
const byte pinDown = 12;
const byte pinUp = 13;
const char* password = "duoguanriben8";
const char* ssid = "Robocon";
const char* update_username = "ILWT";
const char* update_password = "ILWT";
const char* update_path = "/ota";
const char* ver = "alpha";
const String cmdList[5] = {"border", "fitting", "restart", "restore", "print"};

boolean isOnline;
byte menu;
char incomingPacket[255];
char replyPacekt[] = "Got it, ILWT!\n";
float border[2], fitting[2], temp;
int adc, pwm;
String tempStr;
unsigned int localUdpPort = 8266;
unsigned long timeStamp[3];

ESP8266HTTPUpdateServer httpUpdater;
ESP8266WebServer server(80);
WiFiUDP mainUDP, ntpUDP;
NTPClient timeClient(ntpUDP, "ntp1.aliyun.com", 28800, 10000);
SevenSegmentTM1637 display(pinCLK, pinDIO);

int checkButton(void) {
  if (timeStamp[2] == 0 or millis() - timeStamp[2] > COOLDOWN_TIME) {
    if (digitalRead(pinPage) == LOW) {
      delay(10);
      if (digitalRead(pinPage) == LOW) {
        Serial.println("Button [Page] was pushed");
        return 1;
      }
    } else if (digitalRead(pinDown) == LOW) {
      delay(10);
      if (digitalRead(pinDown) == LOW) {
        Serial.println("Button [Down] was pushed");
        return 2;
      }
    } else if (digitalRead(pinUp) == LOW) {
      delay(10);
      if (digitalRead(pinUp) == LOW) {
        Serial.println("Button [Up] was pushed");
        return 3;
      }
    }
    timeStamp[2] = millis();
  }
  return 0;
}

void fade(int style, int msWait = 100) {
  int i;
  if (style == 0 or style == 2) {
    for (i = 0; i <= 80; i += 10) {
      display.setBacklight(i);
      delay(50);
    }
  }
  if (style == 2) delay(msWait);
  if (style == 1 or style == 2) {
    for (i = 80; i >= 0; i -= 10) {
      display.setBacklight(i);
      delay(50);
    }
  }
}

void stateMachine(void) {
  String borderLStr, borderHStr;
  byte strLen;
  byte rawData[4];
  switch (menu) {
    case 0:
    case 1:
      strLen = tempStr.length();
      rawData[0] = display.encode(strLen > 4 ? tempStr[strLen - 5] : ' ');
      rawData[1] = display.encode(strLen > 3 ? tempStr[strLen - 4] : ' ');
      rawData[2] = display.encode(tempStr[strLen - 3]) | B10000000;
      rawData[3] = display.encode(tempStr[strLen - 1]);
      display.printRaw(rawData);
      if (menu == 0) {
        Serial.println("Changed to page 0");
        fade(0);
        menu = 1;
      }
      if (checkButton() == 1) {
        fade(1);
        menu = 2;
      }
      break;
    case 2:
    case 3:
      borderLStr = String(border[0]);
      strLen = borderLStr.length();
      rawData[0] = display.encode('L');
      rawData[1] = display.encode(strLen > 4 ? borderLStr[strLen - 5] : ' ');
      rawData[2] = display.encode(borderLStr[strLen - 4]) | B10000000;
      rawData[3] = display.encode(borderLStr[strLen - 2]);
      display.printRaw(rawData);
      if (menu == 2) {
        Serial.println("Changed to page 1");
        fade(0);
        menu = 3;
      }
      switch (checkButton()) {
        case 1:
          fade(1);
          menu = 4;
          break;
        case 2:
          if (border[0] > 0.1) {
            border[0] -= 0.1;
            EEPROM.begin(11);
            EEPROM.write(1, round(border[0] * 10) / 10);
            EEPROM.write(2, round(border[0] * 10) % 10);
            EEPROM.end();
            Serial.print("border <= {");
            Serial.print(border[0]);
            Serial.print(", ");
            Serial.print(border[1]);
            Serial.println("}");
          }
          break;
        case 3:
          if (border[0] < 99.7) {
            border[0] += 0.1;
            EEPROM.begin(11);
            EEPROM.write(1, round(border[0] * 10) / 10);
            EEPROM.write(2, round(border[0] * 10) % 10);
            if (border[1] < border[0] + 0.2) {
              border[1] = border[0] + 0.2;
              EEPROM.write(3, round(border[1] * 10) / 10);
              EEPROM.write(4, round(border[1] * 10) % 10);
            }
            EEPROM.end();
            Serial.print("border <= {");
            Serial.print(border[0]);
            Serial.print(", ");
            Serial.print(border[1]);
            Serial.println("}");
          }
          break;
      }
      break;
    case 4:
    case 5:
      borderHStr = String(border[1]);
      strLen = borderHStr.length();
      rawData[0] = display.encode('H');
      rawData[1] = display.encode(strLen > 4 ? borderHStr[strLen - 5] : ' ');
      rawData[2] = display.encode(borderHStr[strLen - 4]) | B10000000;
      rawData[3] = display.encode(borderHStr[strLen - 2]);
      display.printRaw(rawData);
      if (menu == 4) {
        Serial.println("Changed to page 2");
        fade(0);
        menu = 5;
      }
      switch (checkButton()) {
        case 1:
          fade(1);
          menu = 0;
          break;
        case 2:
          if (border[1] > 0.3) {
            border[1] -= 0.1;
            EEPROM.begin(11);
            if (border[0] > border[1] - 0.2) {
              border[0] = border[1] - 0.2;
              EEPROM.write(1, round(border[0] * 10) / 10);
              EEPROM.write(1, round(border[0] * 10) % 10);
            }
            EEPROM.write(3, round(border[1] * 10) / 10);
            EEPROM.write(4, round(border[1] * 10) % 10);
            EEPROM.end();
            Serial.print("border <= {");
            Serial.print(border[0]);
            Serial.print(", ");
            Serial.print(border[1]);
            Serial.println("}");
          }
          break;
        case 3:
          if (border[1] < 99.9) {
            border[1] += 0.1;
            EEPROM.begin(11);
            EEPROM.write(3, round(border[1] * 10) / 10);
            EEPROM.write(4, round(border[1] * 10) % 10);
            EEPROM.end();
            Serial.print("border <= {");
            Serial.print(border[0]);
            Serial.print(", ");
            Serial.print(border[1]);
            Serial.println("}");
          }
          break;
      }
      break;
  }
}

void bannerPrint(void) {
  Serial.println();
  Serial.println("Digital Temperature PWM Controller");
  Serial.printf("MAC address: %s", getMAC().c_str());
  Serial.println();
  Serial.printf("Software version: %s", ver);
  Serial.println();
}

void readConf(void) {
  EEPROM.begin(11);
  if (EEPROM.read(0) != 0x39) {
    EEPROM.write(0, 0x39);
    EEPROM.write(1, round(BORDER_L * 10) / 10);
    EEPROM.write(2, round(BORDER_L * 10) % 10);
    EEPROM.write(3, round(BORDER_H * 10) / 10);
    EEPROM.write(4, round(BORDER_H * 10) % 10);
    EEPROM.write(5, (FITTING_K < 0) ? 1 : 0);
    EEPROM.write(6, round(abs(FITTING_K) * 100) / 100);
    EEPROM.write(7, round(abs(FITTING_K) * 100) % 100);
    EEPROM.write(8, (FITTING_B < 0) ? 1 : 0);
    EEPROM.write(9, round(abs(FITTING_B) * 100) / 100);
    EEPROM.write(10, round(abs(FITTING_B) * 100) % 100);
  }
  border[0] = EEPROM.read(1) + EEPROM.read(2) * 0.1;
  border[1] = EEPROM.read(3) + EEPROM.read(4) * 0.1;
  fitting[0] = (EEPROM.read(5) == 1 ? -1 : 1) * (EEPROM.read(6) + EEPROM.read(7) * 0.01);
  fitting[1] = (EEPROM.read(8) == 1 ? -1 : 1) * (EEPROM.read(9) + EEPROM.read(10) * 0.01);
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
  for (int i = 0; i < 15; i++) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.print(">");
    } else {
      break;
    }
    display.print("conn");
    fade(2);
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
    Serial.println("Displaying IP address");
    display.clear();
    display.on();
    display.printf("    %d.%d.%d.%d    ", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
    display.off();
    return true;
  } else {
    Serial.println("FAIL");
    WiFi.disconnect(true);
    return false;
  }
}

void dataProcess(void) {
  if (timeStamp[0] == 0 or millis() - timeStamp[0] > SAMPLING_PERIOD) {
    adc = analogRead(A0);
    temp = fitting[0] * adc + fitting[1];
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
  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = prepareResponse(jsonBuffer);
  json.prettyPrintTo(msg);
  server.send(200, "application/json", msg);
}

JsonObject& prepareResponse(JsonBuffer& jsonBuffer) {
  JsonObject& root = jsonBuffer.createObject();
  root["adc"] = adc;
  JsonArray& borderJson = root.createNestedArray("border");
  borderJson.add(border[0]);
  borderJson.add(border[1]);
  JsonArray& fittingJson = root.createNestedArray("fitting");
  fittingJson.add(fitting[0]);
  fittingJson.add(fitting[1]);
  root["mac"] = getMAC();
  root["pwm"] = pwm;
  root["temp"] = tempStr;
  root["time"] = timeClient.getFormattedTime();
  root["version"] = ver;
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
          if (parameter.indexOf(',') != -1) {
            float newBorder[2] = {parameter.substring(0, parameter.indexOf(',')).toFloat(), parameter.substring(parameter.indexOf(',') + 1, parameter.length()).toFloat()};
            if (newBorder[0] >= 0.1 and (newBorder[1] - newBorder[0]) > 0.1 and newBorder[1] <= 99.9 ) {
              EEPROM.begin(11);
              EEPROM.write(1, round(newBorder[0] * 10) / 10);
              EEPROM.write(2, round(newBorder[0] * 10) % 10);
              EEPROM.write(3, round(newBorder[1] * 10) / 10);
              EEPROM.write(4, round(newBorder[1] * 10) % 10);
              border[0] = EEPROM.read(1) + EEPROM.read(2) * 0.1;
              border[1] = EEPROM.read(3) + EEPROM.read(4) * 0.1;
              EEPROM.end();
              Serial.print("border <= {");
              Serial.print(border[0]);
              Serial.print(", ");
              Serial.print(border[1]);
              Serial.println("}");
            } else Serial.println("Value error");
          } else Serial.println("Format error");
          break;
        case 1:
          if (parameter.indexOf(',') != -1) {
            float newFitting[2] = {parameter.substring(0, parameter.indexOf(',')).toFloat(), parameter.substring(parameter.indexOf(',') + 1, parameter.length()).toFloat()};
            EEPROM.begin(11);
            EEPROM.write(5, (newFitting[0] < 0) ? 1 : 0);
            EEPROM.write(6, round(abs(newFitting[0]) * 100) / 100);
            EEPROM.write(7, round(abs(newFitting[0]) * 100) % 100);
            EEPROM.write(8, (newFitting[1] < 0) ? 1 : 0);
            EEPROM.write(9, round(abs(newFitting[1]) * 100) / 100);
            EEPROM.write(10, round(abs(newFitting[1]) * 100) % 100);
            fitting[0] = (EEPROM.read(5) == 1 ? -1 : 1) * (EEPROM.read(6) + EEPROM.read(7) * 0.01);
            fitting[1] = (EEPROM.read(8) == 1 ? -1 : 1) * (EEPROM.read(9) + EEPROM.read(10) * 0.01);
            EEPROM.end();
            Serial.print("fitting <= {");
            Serial.print(fitting[0]);
            Serial.print(", ");
            Serial.print(fitting[1]);
            Serial.println("}");
          } else Serial.println("Format error");
          break;
        case 2:
          Serial.println("Restarting...");
          ESP.restart();
          break;
        case 3:
          EEPROM.begin(11);
          EEPROM.write(0, 0x39);
          EEPROM.write(1, round(BORDER_L * 10) / 10);
          EEPROM.write(2, round(BORDER_L * 10) % 10);
          EEPROM.write(3, round(BORDER_H * 10) / 10);
          EEPROM.write(4, round(BORDER_H * 10) % 10);
          EEPROM.write(5, (FITTING_K < 0) ? 1 : 0);
          EEPROM.write(6, round(abs(FITTING_K) * 100) / 100);
          EEPROM.write(7, round(abs(FITTING_K) * 100) % 100);
          EEPROM.write(8, (FITTING_B < 0) ? 1 : 0);
          EEPROM.write(9, round(abs(FITTING_B) * 100) / 100);
          EEPROM.write(10, round(abs(FITTING_B) * 100) % 100);
          border[0] = EEPROM.read(1) + EEPROM.read(2) * 0.1;
          border[1] = EEPROM.read(3) + EEPROM.read(4) * 0.1;
          fitting[0] = (EEPROM.read(5) == 1 ? -1 : 1) * (EEPROM.read(6) + EEPROM.read(7) * 0.01);
          fitting[1] = (EEPROM.read(8) == 1 ? -1 : 1) * (EEPROM.read(9) + EEPROM.read(10) * 0.01);
          EEPROM.end();
          Serial.print("border <= {");
          Serial.print(border[0]);
          Serial.print(", ");
          Serial.print(border[1]);
          Serial.println("}");
          Serial.print("fitting <= {");
          Serial.print(fitting[0]);
          Serial.print(", ");
          Serial.print(fitting[1]);
          Serial.println("}");
          break;
        case 4:
          Serial.println("---------- BEGIN ----------");
          Serial.printf("ADC = %d", adc);
          Serial.println();
          Serial.print("Border = {");
          Serial.print(border[0]);
          Serial.print(", ");
          Serial.print(border[1]);
          Serial.println("}");
          Serial.print("Fitting = {");
          Serial.print(fitting[0]);
          Serial.print(", ");
          Serial.print(fitting[1]);
          Serial.println("}");
          Serial.printf("PWM = %d", pwm);
          Serial.println();
          Serial.printf("Temperature = %s", tempStr.c_str());
          Serial.println();
          Serial.printf("Time = %s", timeClient.getFormattedTime().c_str());
          Serial.println();
          Serial.println("----------- END -----------");
          break;
      }
      break;
    }
  }
}

void setup(void) {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(pinPage, INPUT_PULLUP);
  pinMode(pinDown, INPUT_PULLUP);
  pinMode(pinUp, INPUT_PULLUP);
  Serial.begin(115200);
  display.setPrintDelay(200);
  display.setBacklight(0);
  display.begin();
  display.print("boot");
  fade(2);
  bannerPrint();
  readConf();
  isOnline = connectWifi();
  Serial.println("All done");
}

void loop(void) {
  digitalWrite(LED_BUILTIN, LOW);
  dataProcess();
  stateMachine();
  if (isOnline) {
    udpHandle(mainUDP);
    ArduinoOTA.handle();
    server.handleClient();
    timeUpdate();
  }
  digitalWrite(LED_BUILTIN, HIGH);
}

