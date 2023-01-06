#include "ArduinoJson.h"
#include "ArduinoOTA.h"
#include "ESPAsyncWebServer.h"
#include "ZHNetwork.h"
#include "ZHConfig.h"

void onConfirmReceiving(const uint8_t *target, const bool status);

void loadConfig(void);
void saveConfig(void);
void setupWebServer(void);

void sendSensorConfigMessage(void);
void sendBatteryConfigMessage(void);
void sendAttributesMessage(void);

const String firmware{"1.0"};

String espnowNetName{"DEFAULT"};

String deviceSensorName{"ESP-NOW window sensor"};
uint8_t deviceSensorClass{HABSDC_WINDOW};
String deviceBatteryName{"ESP-NOW window sensor battery"};

char receivedBytes[128]{0};
byte counter{0};
byte messageLenght{0};
bool dataReceiving{false};
bool dataReceived{false};
bool semaphore{false};

esp_now_payload_data_t outgoingData{ENDT_SENSOR, ENPT_STATE};
StaticJsonDocument<sizeof(esp_now_payload_data_t::message)> json;
char buffer[sizeof(esp_now_payload_data_t::message)]{0};
char temp[sizeof(esp_now_payload_data_t)]{0};
const char initialMessage[] = {0x55, 0xAA, 0x00, 0x01, 0x00, 0x00, 0x00};
const char connectedMessage[] = {0x55, 0xAA, 0x00, 0x02, 0x00, 0x01, 0x04, 0x06};
const char settingMessage[] = {0x55, 0xAA, 0x00, 0x03, 0x00, 0x00, 0x02};
const char confirmationMessage[] = {0x55, 0xAA, 0x00, 0x08, 0x00, 0x01, 0x00, 0x08};

ZHNetwork myNet;
AsyncWebServer webServer(80);

void setup()
{
  Serial.begin(9600);

  SPIFFS.begin();

  loadConfig();

  myNet.begin(espnowNetName.c_str());

  myNet.setOnConfirmReceivingCallback(onConfirmReceiving);

  sendSensorConfigMessage();
  sendBatteryConfigMessage();
  sendAttributesMessage();

  Serial.write(initialMessage, sizeof(initialMessage));
  Serial.flush();
}

void loop()
{
  if (Serial.available() > 0 && !dataReceived)
  {
    char receivedByte[1];
    Serial.readBytes(receivedByte, 1);
    if (receivedByte[0] == 0x55)
    {
      dataReceiving = true;
      receivedBytes[counter++] = receivedByte[0];
      return;
    }
    if (dataReceiving)
    {
      if (counter == 5)
        messageLenght = 6 + int(receivedByte[0]);
      if (counter == messageLenght)
      {
        receivedBytes[counter] = receivedByte[0];
        counter = 0;
        dataReceiving = false;
        dataReceived = true;
        return;
      }
      receivedBytes[counter++] = receivedByte[0];
    }
  }
  if (dataReceived)
  {
    if (receivedBytes[3] == 0x01)
    {
      Serial.write(connectedMessage, sizeof(connectedMessage));
      Serial.flush();
      dataReceived = false;
    }
    if (receivedBytes[3] == 0x02)
      dataReceived = false;
    if (receivedBytes[3] == 0x03)
    {
      Serial.write(settingMessage, sizeof(settingMessage));
      Serial.flush();
      Serial.end();
      dataReceived = false;
      WiFi.softAP(("ESP-NOW Window " + myNet.getNodeMac()).c_str(), "12345678", 1, 0);
      setupWebServer();
      ArduinoOTA.begin();
    }
    if (receivedBytes[3] == 0x08)
    {
      if (receivedBytes[7] == 0x01)
      {
        if (receivedBytes[17] == 0x02)
          json["battery"] = "HIGH";
        if (receivedBytes[17] == 0x01)
          json["battery"] = "MID";
        if (receivedBytes[17] == 0x00)
          json["battery"] = "LOW";
        dataReceived = false;
        Serial.write(confirmationMessage, sizeof(confirmationMessage));
        Serial.flush();
      }
      if (receivedBytes[7] == 0x02)
      {
        if (receivedBytes[17] == 0x01)
          json["state"] = "OPEN";
        if (receivedBytes[17] == 0x00)
          json["state"] = "CLOSED";
        dataReceived = false;
        serializeJsonPretty(json, buffer);
        memcpy(outgoingData.message, buffer, sizeof(esp_now_payload_data_t::message));
        memcpy(temp, &outgoingData, sizeof(esp_now_payload_data_t));
        myNet.sendBroadcastMessage(temp);
        semaphore = true;
      }
    }
  }
  myNet.maintenance();
  ArduinoOTA.handle();
}

void onConfirmReceiving(const uint8_t *target, const bool status)
{
  if (semaphore)
  {
    Serial.write(confirmationMessage, sizeof(confirmationMessage));
    Serial.flush();
  }
}

void loadConfig()
{
  if (!SPIFFS.exists("/config.json"))
    saveConfig();
  File file = SPIFFS.open("/config.json", "r");
  String jsonFile = file.readString();
  StaticJsonDocument<512> json;
  deserializeJson(json, jsonFile);
  espnowNetName = json["espnowNetName"].as<String>();
  deviceSensorName = json["deviceSensorName"].as<String>();
  deviceBatteryName = json["deviceBatteryName"].as<String>();
  deviceSensorClass = json["deviceSensorClass"];
  file.close();
}

void saveConfig()
{
  StaticJsonDocument<512> json;
  json["firmware"] = firmware;
  json["espnowNetName"] = espnowNetName;
  json["deviceSensorName"] = deviceSensorName;
  json["deviceBatteryName"] = deviceBatteryName;
  json["deviceSensorClass"] = deviceSensorClass;
  json["system"] = "empty";
  File file = SPIFFS.open("/config.json", "w");
  serializeJsonPretty(json, file);
  file.close();
}

void setupWebServer()
{
  webServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(SPIFFS, "/index.htm"); });

  webServer.on("/setting", HTTP_GET, [](AsyncWebServerRequest *request)
               {
        deviceSensorName = request->getParam("deviceSensorName")->value();
        deviceBatteryName = request->getParam("deviceBatteryName")->value();
        deviceSensorClass = request->getParam("deviceSensorClass")->value().toInt();
        espnowNetName = request->getParam("espnowNetName")->value();
        request->send(200);
        saveConfig(); });

  webServer.on("/restart", HTTP_GET, [](AsyncWebServerRequest *request)
               {
        request->send(200);
        ESP.restart(); });

  webServer.onNotFound([](AsyncWebServerRequest *request)
                       { 
        if (SPIFFS.exists(request->url()))
        request->send(SPIFFS, request->url());
        else
        {
        request->send(404, "text/plain", "File Not Found");
        } });

  webServer.begin();
}

void sendSensorConfigMessage()
{
  esp_now_payload_data_t outgoingData{ENDT_SENSOR, ENPT_CONFIG};
  StaticJsonDocument<sizeof(esp_now_payload_data_t::message)> json;
  json["name"] = deviceSensorName;
  json["unit"] = 1;
  json["type"] = HACT_BINARY_SENSOR;
  json["class"] = deviceSensorClass;
  json["payload_on"] = "OPEN";
  json["payload_off"] = "CLOSED";
  char buffer[sizeof(esp_now_payload_data_t::message)]{0};
  serializeJsonPretty(json, buffer);
  memcpy(outgoingData.message, buffer, sizeof(esp_now_payload_data_t::message));
  char temp[sizeof(esp_now_payload_data_t)]{0};
  memcpy(&temp, &outgoingData, sizeof(esp_now_payload_data_t));
  myNet.sendBroadcastMessage(temp);
}

void sendBatteryConfigMessage()
{
  esp_now_payload_data_t outgoingData{ENDT_SENSOR, ENPT_CONFIG};
  StaticJsonDocument<sizeof(esp_now_payload_data_t::message)> json;
  json["name"] = deviceBatteryName;
  json["unit"] = 2;
  json["type"] = HACT_BINARY_SENSOR;
  json["class"] = HABSDC_BATTERY;
  json["payload_on"] = "MID";
  json["payload_off"] = "HIGH";
  char buffer[sizeof(esp_now_payload_data_t::message)]{0};
  serializeJsonPretty(json, buffer);
  memcpy(outgoingData.message, buffer, sizeof(esp_now_payload_data_t::message));
  char temp[sizeof(esp_now_payload_data_t)]{0};
  memcpy(&temp, &outgoingData, sizeof(esp_now_payload_data_t));
  myNet.sendBroadcastMessage(temp);
}

void sendAttributesMessage()
{
  esp_now_payload_data_t outgoingData{ENDT_SENSOR, ENPT_ATTRIBUTES};
  StaticJsonDocument<sizeof(esp_now_payload_data_t::message)> json;
  json["Type"] = "ESP-NOW Window Sensor";
  json["MCU"] = "ESP8266";
  json["MAC"] = myNet.getNodeMac();
  json["Firmware"] = firmware;
  json["Library"] = myNet.getFirmwareVersion();
  char buffer[sizeof(esp_now_payload_data_t::message)]{0};
  serializeJsonPretty(json, buffer);
  memcpy(outgoingData.message, buffer, sizeof(esp_now_payload_data_t::message));
  char temp[sizeof(esp_now_payload_data_t)]{0};
  memcpy(temp, &outgoingData, sizeof(esp_now_payload_data_t));
  myNet.sendBroadcastMessage(temp);
}
