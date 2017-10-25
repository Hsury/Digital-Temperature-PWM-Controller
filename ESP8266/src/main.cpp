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

#include <Arduino.h>
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
const byte pinDown = 12;
const byte pinDIO = 5;
const byte pinLED = 2;
const byte pinPage = 14;
const byte pinUp = 13;
const char* password = "duoguanriben8";
const char* ssid = "Robocon";
const char* update_username = "hsury";
const char* update_password = "miku";
const char* update_path = "/ota";
const char* ver = "171025";
const String cmdList[5] = {"border", "fitting", "restart", "restore", "print"};

union float2byte {
  float in;
  byte out[4];
};

boolean isOnline;
byte menu;
char incomingPacket[255];
char replyPacekt[] = "Got it.";
float border[2];
float fitting[2];
float pwm;
float temp;
int adc;
String borderLStr;
String borderHStr;
String pwmStr;
String tempStr;
unsigned int localUdpPort = 8266;
unsigned long timeStamp[3];

ESP8266HTTPUpdateServer httpUpdater;
ESP8266WebServer server(80);
WiFiUDP mainUDP, ntpUDP;
NTPClient timeClient(ntpUDP, "ntp1.aliyun.com", 28800, 10000);
SevenSegmentTM1637 display(pinCLK, pinDIO);

boolean connectWifi(void);
int buttonCheck(void);
JsonObject& prepareResponse(JsonBuffer& jsonBuffer);
String getMAC(void);
void bannerPrint(void);
void cmdCheck(String packet);
void comHandle(void);
void dataProcess(void);
void fade(int style, int msWait = 100);
void handleRoot(void);
void otaSetup(void);
void readConf(void);
void saveConf(void);
void stateMachine(void);
void timeUpdate(void);
void udpHandle(WiFiUDP& udp);
void writeConf(void);

void setup(void) {
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
  dataProcess();
  stateMachine();
  comHandle();
  if (isOnline) {
    udpHandle(mainUDP);
    ArduinoOTA.handle();
    server.handleClient();
    timeUpdate();
  }
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

int buttonCheck(void) {
  if ((timeStamp[0] == 0) or (millis() - timeStamp[0] > COOLDOWN_TIME)) {
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
    timeStamp[0] = millis();
  }
  return 0;
}

JsonObject& prepareResponse(JsonBuffer& jsonBuffer) {
  JsonObject& root = jsonBuffer.createObject();
  root["adc"] = adc;
  JsonArray& borderJson = root.createNestedArray("border");
  borderJson.add(borderLStr);
  borderJson.add(borderHStr);
  JsonArray& fittingJson = root.createNestedArray("fitting");
  fittingJson.add(fitting[0]);
  fittingJson.add(fitting[1]);
  root["mac"] = getMAC();
  root["pwm"] = pwmStr;
  root["temp"] = tempStr;
  root["time"] = timeClient.getFormattedTime();
  root["version"] = ver;
  return root;
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

void bannerPrint(void) {
  Serial.println();
  Serial.println("Digital Temperature PWM Controller");
  Serial.printf("MAC address: %s", getMAC().c_str());
  Serial.println();
  Serial.printf("Software version: %s", ver);
  Serial.println();
}

void comHandle(void) {
  if (Serial.available() > 0) {
    cmdCheck(Serial.readStringUntil('\r'));
    Serial.flush();
  }
}

void cmdCheck(String packet) {
  String parameter;
  for (int i = 0; i < sizeof(cmdList) / sizeof(cmdList[0]); i++) {
    if (packet.startsWith(cmdList[i] + ":") and packet.endsWith(";")) {
      parameter = packet.substring(cmdList[i].length() + 1, packet.length() - 1);
      Serial.printf("Command \"%s\" got, parameter: \"%s\"", cmdList[i].c_str(), parameter.c_str());
      Serial.println();
      switch (i) {
        case 0:
          if (parameter.indexOf(',') != -1) {
            float newBorder[2] = {parameter.substring(0, parameter.indexOf(',')).toFloat(), parameter.substring(parameter.indexOf(',') + 1, parameter.length()).toFloat()};
            if ((newBorder[0] >= 0.1) and (newBorder[1] - newBorder[0] >= 0.2) and (newBorder[1] <= 99.9)) {
              border[0] = newBorder[0];
              border[1] = newBorder[1];
              writeConf();
              Serial.printf("border <= {%s, %s}", borderLStr.c_str(), borderHStr.c_str());
              Serial.println();
            } else Serial.println("Value error");
          } else Serial.println("Format error");
          break;
        case 1:
          if (parameter.indexOf(',') != -1) {
            float newFitting[2] = {parameter.substring(0, parameter.indexOf(',')).toFloat(), parameter.substring(parameter.indexOf(',') + 1, parameter.length()).toFloat()};
            fitting[0] = newFitting[0];
            fitting[1] = newFitting[1];
            writeConf();
            Serial.print("fitting <= {");
            Serial.print(fitting[0]);
            Serial.print(", ");
            Serial.print(fitting[1]);
            Serial.println("}");
          } else Serial.println("Format error");
          break;
        case 2:
          Serial.println("Restarting...");
          delay(250);
          ESP.restart();
          break;
        case 3:
          border[0] = BORDER_L;
          border[1] = BORDER_H;
          fitting[0] = FITTING_K;
          fitting[1] = FITTING_B;
          writeConf();
          Serial.printf("border <= {%s, %s}", borderLStr.c_str(), borderHStr.c_str());
          Serial.println();
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
          Serial.printf("Border = {%s, %s}", borderLStr.c_str(), borderHStr.c_str());
          Serial.println();
          Serial.print("Fitting = {");
          Serial.print(fitting[0]);
          Serial.print(", ");
          Serial.print(fitting[1]);
          Serial.println("}");
          Serial.printf("PWM = %s", pwmStr.c_str());
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

void dataProcess(void) {
  if ((timeStamp[1] == 0) or (millis() - timeStamp[1] > SAMPLING_PERIOD)) {
    adc = analogRead(A0);
    temp = fitting[0] * adc + fitting[1];
    if (temp < 0) temp = 0;
    if (temp > 100) temp = 100;
    tempStr = String(round(temp * 10) / 10.0);
    tempStr = tempStr.substring(0, tempStr.length() - 1);
    pwm = 1.0 * (border[1] - temp) / (border[1] - border[0]);
    if (pwm < 0) pwm = 0;
    if (pwm > 1) pwm = 1;
    pwmStr = String(round(pwm * 100 * 10) / 10.0);
    pwmStr = pwmStr.substring(0, pwmStr.length() - 1);
    timeStamp[1] = millis();
  }
}

void fade(int style, int msWait) {
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

void handleRoot(void) {
  String msg;
  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = prepareResponse(jsonBuffer);
  json.prettyPrintTo(msg);
  server.send(200, "application/json", msg);
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

void readConf(void) {
  if (EEPROM.read(0) == 0x39) {
    EEPROM.begin(17);
    float2byte tmpFloat;
    for (int i = 0; i < 4; i++) {
      tmpFloat.out[i] = EEPROM.read(i + 1);
    }
    border[0] = tmpFloat.in;
    for (int i = 0; i < 4; i++) {
      tmpFloat.out[i] = EEPROM.read(i + 5);
    }
    border[1] = tmpFloat.in;
    for (int i = 0; i < 4; i++) {
      tmpFloat.out[i] = EEPROM.read(i + 9);
    }
    fitting[0] = tmpFloat.in;
    for (int i = 0; i < 4; i++) {
      tmpFloat.out[i] = EEPROM.read(i + 13);
    }
    fitting[1] = tmpFloat.in;
    EEPROM.end();
    borderLStr = String(border[0]);
    borderLStr = borderLStr.substring(0, borderLStr.length() - 1);
    borderHStr = String(border[1]);
    borderHStr = borderHStr.substring(0, borderHStr.length() - 1);
  } else {
    border[0] = BORDER_L;
    border[1] = BORDER_H;
    fitting[0] = FITTING_K;
    fitting[1] = FITTING_B;
    writeConf();
  }
}

void stateMachine(void) {
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
      analogWrite(pinLED, round((1 - pwm) * 1023));
      if (menu == 0) {
        Serial.println("Changed to page 0");
        fade(0);
        menu = 1;
      }
      if (buttonCheck() == 1) {
        fade(1);
        analogWrite(pinLED, 1023);
        menu = 2;
      }
      break;
    case 2:
    case 3:
      strLen = borderLStr.length();
      rawData[0] = display.encode('L');
      rawData[1] = display.encode(strLen > 3 ? borderLStr[strLen - 4] : ' ');
      rawData[2] = display.encode(borderLStr[strLen - 3]) | B10000000;
      rawData[3] = display.encode(borderLStr[strLen - 1]);
      display.printRaw(rawData);
      if (menu == 2) {
        Serial.println("Changed to page 1");
        fade(0);
        menu = 3;
      }
      switch (buttonCheck()) {
        case 1:
          fade(1);
          menu = 4;
          break;
        case 2:
          if (border[0] >= 0.2) {
            border[0] -= 0.1;
            writeConf();
            Serial.printf("border <= {%s, %s}", borderLStr.c_str(), borderHStr.c_str());
            Serial.println();
          }
          break;
        case 3:
          if (border[0] <= 99.6) {
            border[0] += 0.1;
            if (border[1] < border[0] + 0.2) border[1] = border[0] + 0.2;
            writeConf();
            Serial.printf("border <= {%s, %s}", borderLStr.c_str(), borderHStr.c_str());
            Serial.println();
          }
          break;
      }
      break;
    case 4:
    case 5:
      strLen = borderHStr.length();
      rawData[0] = display.encode('H');
      rawData[1] = display.encode(strLen > 3 ? borderHStr[strLen - 4] : ' ');
      rawData[2] = display.encode(borderHStr[strLen - 3]) | B10000000;
      rawData[3] = display.encode(borderHStr[strLen - 1]);
      display.printRaw(rawData);
      if (menu == 4) {
        Serial.println("Changed to page 2");
        fade(0);
        menu = 5;
      }
      switch (buttonCheck()) {
        case 1:
          fade(1);
          menu = 0;
          break;
        case 2:
          if (border[1] >= 0.4) {
            border[1] -= 0.1;
            if (border[0] > border[1] - 0.2) border[0] = border[1] - 0.2;
            writeConf();
            Serial.printf("border <= {%s, %s}", borderLStr.c_str(), borderHStr.c_str());
            Serial.println();
          }
          break;
        case 3:
          if (border[1] <= 99.8) {
            border[1] += 0.1;
            writeConf();
            Serial.printf("border <= {%s, %s}", borderLStr.c_str(), borderHStr.c_str());
            Serial.println();
          }
          break;
      }
      break;
  }
}

void timeUpdate(void) {
  if ((timeStamp[2] == 0) or (millis() - timeStamp[2] > 30000)) {
    timeClient.update();
    timeStamp[2] = millis();
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
    cmdCheck(String(incomingPacket));
  }
}

void writeConf(void) {
  EEPROM.begin(17);
  EEPROM.write(0, 0x39);
  border[0] = round(border[0] * 10) / 10.0;
  border[1] = round(border[1] * 10) / 10.0;
  float2byte tmpFloat;
  tmpFloat.in =  border[0];
  for (int i = 0; i < 4; i++) {
    EEPROM.write(i + 1, tmpFloat.out[i]);
  }
  tmpFloat.in =  border[1];
  for (int i = 0; i < 4; i++) {
    EEPROM.write(i + 5, tmpFloat.out[i]);
  }
  tmpFloat.in =  fitting[0];
  for (int i = 0; i < 4; i++) {
    EEPROM.write(i + 9, tmpFloat.out[i]);
  }
  tmpFloat.in =  fitting[1];
  for (int i = 0; i < 4; i++) {
    EEPROM.write(i + 13, tmpFloat.out[i]);
  }
  EEPROM.end();
  borderLStr = String(border[0]);
  borderLStr = borderLStr.substring(0, borderLStr.length() - 1);
  borderHStr = String(border[1]);
  borderHStr = borderHStr.substring(0, borderHStr.length() - 1);
}
