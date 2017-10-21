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
const char* password = "KAGAMIZ.COM";
const char* ssid = "HsuRY";
const char* update_username = "ILWT";
const char* update_password = "ILWT";
const char* update_path = "/ota";
const char* ver = "20171022";
const String cmdList[5] = {"border", "fitting", "restart", "restore", "print"};

boolean isOnline;
byte menu;
char incomingPacket[255];
char replyPacekt[] = "Got it, ILWT!\n";
float border[2], fitting[2], temp;
int adc, pwm;
String tempStr;
unsigned int localUdpPort = 8266;
unsigned long timeStamp[2];

ESP8266HTTPUpdateServer httpUpdater;
ESP8266WebServer server(80);
WiFiUDP mainUDP, ntpUDP;
NTPClient timeClient(ntpUDP, "ntp1.aliyun.com", 28800, 10000);
SevenSegmentTM1637 display(pinCLK, pinDIO);

void stateMachine(void) {
  switch (menu) {
    case 0:
      byte tempStrLen = tempStr.length();
      byte  rawData[4];
      rawData[0] = display.encode(tempStrLen > 4 ? tempStr[tempStrLen - 5] : ' ');
      rawData[1] = display.encode(tempStrLen > 3 ? tempStr[tempStrLen - 4] : ' ');
      rawData[2] = display.encode(tempStr[tempStrLen - 3]) | B10000000;
      rawData[3] = display.encode(tempStr[tempStrLen - 1]);
      display.printRaw(rawData);
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
  EEPROM.begin(9);
  if (EEPROM.read(0) != 0x39) {
    EEPROM.write(0, 0x39);
    EEPROM.write(1, round(BORDER_L * 10) / 10);
    EEPROM.write(2, round(BORDER_L * 10) % 10);
    EEPROM.write(3, round(BORDER_H * 10) / 10);
    EEPROM.write(4, round(BORDER_H * 10) % 10);
    EEPROM.write(5, round(FITTING_K * 100) / 100);
    EEPROM.write(6, round(FITTING_K * 100) % 100);
    EEPROM.write(7, round(FITTING_B * 100) / 100);
    EEPROM.write(8, round(FITTING_B * 100) % 100);
  }
  border[0] = EEPROM.read(1) + EEPROM.read(2) * 0.1;
  border[1] = EEPROM.read(3) + EEPROM.read(4) * 0.1;
  fitting[0] = EEPROM.read(5) + EEPROM.read(6) * 0.01;
  fitting[1] = EEPROM.read(7) + EEPROM.read(8) * 0.01;
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
  for (int i = 0; i < 60; i++) {
    delay(500);
    char statusbar[] = "    ";
    statusbar[i % 4] = '-';
    display.print(statusbar);
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
    display.print("PASS");
    delay(500);
    Serial.println("Displaying IP address");
    display.printf("   %d.%d.%d.%d   ", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
    return true;
  } else {
    Serial.println("FAIL");
    WiFi.disconnect(true);
    display.print("FAIL");
    delay(500);
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
    /*
      Serial.printf("UDP Received %d bytes from %s, port %d, packet contents: %s", packetSize, udp.remoteIP().toString().c_str(), udp.remotePort(), incomingPacket);
      Serial.println();
      udp.beginPacket(udp.remoteIP(), udp.remotePort());
      udp.write(replyPacekt);
      udp.endPacket();
      Serial.println("UDP reply packet sent");
    */
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
            if (newBorder[0] > 0 and newBorder[0] < newBorder[1] and newBorder[1] < 100 ) {
              EEPROM.begin(9);
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
            EEPROM.begin(9);
            EEPROM.write(5, round(newFitting[0] * 100) / 100);
            EEPROM.write(6, round(newFitting[0] * 100) % 100);
            EEPROM.write(7, round(newFitting[1] * 100) / 100);
            EEPROM.write(8, round(newFitting[1] * 100) % 100);
            fitting[0] = EEPROM.read(5) + EEPROM.read(6) * 0.01;
            fitting[1] = EEPROM.read(7) + EEPROM.read(8) * 0.01;
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
          EEPROM.begin(9);
          EEPROM.write(0, 0x39);
          EEPROM.write(1, round(BORDER_L * 10) / 10);
          EEPROM.write(2, round(BORDER_L * 10) % 10);
          EEPROM.write(3, round(BORDER_H * 10) / 10);
          EEPROM.write(4, round(BORDER_H * 10) % 10);
          EEPROM.write(5, round(FITTING_K * 100) / 100);
          EEPROM.write(6, round(FITTING_K * 100) % 100);
          EEPROM.write(7, round(FITTING_B * 100) / 100);
          EEPROM.write(8, round(FITTING_B * 100) % 100);
          border[0] = EEPROM.read(1) + EEPROM.read(2) * 0.1;
          border[1] = EEPROM.read(3) + EEPROM.read(4) * 0.1;
          fitting[0] = EEPROM.read(5) + EEPROM.read(6) * 0.01;
          fitting[1] = EEPROM.read(7) + EEPROM.read(8) * 0.01;
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
  Serial.begin(115200);
  display.setPrintDelay(200);
  display.begin();
  display.print("boot");
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

