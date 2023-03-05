#include "ArduinoJson.h"
#include "ArduinoOTA.h"
#include "ESPAsyncWebServer.h" // https://github.com/aZholtikov/Async-Web-Server
#include "LittleFS.h"
#include "EEPROM.h"
#include "ZHNetwork.h"
#include "ZHConfig.h"

ADC_MODE(ADC_VCC);

void onConfirmReceiving(const uint8_t *target, const uint16_t id, const bool status);

void loadConfig(void);
void saveConfig(void);
void setupWebServer(void);

void sendSensorConfigMessage(void);
void sendBatteryConfigMessage(void);
void sendAttributesMessage(void);

struct deviceConfig
{
  const String firmware{"1.3"};
  String espnowNetName{"DEFAULT"};
  String deviceName = "ESP-NOW window " + String(ESP.getChipId(), HEX);
  uint8_t deviceClass{HABSDC_WINDOW};
} config;

char receivedBytes[128]{0};
uint8_t counter{0};
uint8_t messageLenght{0};
bool dataReceiving{false};
bool dataReceived{false};
bool semaphore{false};

const char initialMessage[] = {0x55, 0xAA, 0x00, 0x01, 0x00, 0x00, 0x00};
const char connectedMessage[] = {0x55, 0xAA, 0x00, 0x02, 0x00, 0x01, 0x04, 0x06};
const char settingMessage[] = {0x55, 0xAA, 0x00, 0x03, 0x00, 0x00, 0x02};
const char confirmationMessage[] = {0x55, 0xAA, 0x00, 0x08, 0x00, 0x01, 0x00, 0x08};

ZHNetwork myNet;
AsyncWebServer webServer(80);

void setup()
{
  Serial.begin(9600);

  LittleFS.begin();

  loadConfig();

  myNet.begin(config.espnowNetName.c_str());
  // myNet.setCryptKey("VERY_LONG_CRYPT_KEY"); // If encryption is used, the key must be set same of all another ESP-NOW devices in network.

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
    if (receivedBytes[3] == 0x01) // MCU system information.
    {
      Serial.write(connectedMessage, sizeof(connectedMessage));
      Serial.flush();
      dataReceived = false;
    }
    if (receivedBytes[3] == 0x02) // MCU confirmation message.
      dataReceived = false;
    if (receivedBytes[3] == 0x03) // Message for switching to setting mode.
    {
      Serial.write(settingMessage, sizeof(settingMessage));
      Serial.flush();
      Serial.end();
      dataReceived = false;
      WiFi.mode(WIFI_AP);
      WiFi.softAP(("ESP-NOW window " + String(ESP.getChipId(), HEX)).c_str(), "12345678", 1, 0);
      setupWebServer();
      ArduinoOTA.begin();
    }
    if (receivedBytes[3] == 0x08) // Sensor status message.
    {
      if (receivedBytes[7] == 0x01) // Battery status.
      {
        dataReceived = false;
        Serial.write(confirmationMessage, sizeof(confirmationMessage));
        Serial.flush();
      }
      if (receivedBytes[7] == 0x02) // Sensor position.
      {
        esp_now_payload_data_t outgoingData{ENDT_SENSOR, ENPT_STATE};
        DynamicJsonDocument json(sizeof(esp_now_payload_data_t::message));
        if (receivedBytes[17] == 0x01)
          json["state"] = "OPEN";
        if (receivedBytes[17] == 0x00)
          json["state"] = "CLOSED";
        json["battery"] = round((double(system_get_vdd33()) / 1000) * 100) / 100;
        dataReceived = false;
        serializeJsonPretty(json, outgoingData.message);
        char temp[sizeof(esp_now_payload_data_t)]{0};
        memcpy(&temp, &outgoingData, sizeof(esp_now_payload_data_t));
        myNet.sendBroadcastMessage(temp);
        semaphore = true;
      }
    }
  }
  myNet.maintenance();
  ArduinoOTA.handle();
}

void onConfirmReceiving(const uint8_t *target, const uint16_t id, const bool status)
{
  if (semaphore)
  {
    Serial.write(confirmationMessage, sizeof(confirmationMessage));
    Serial.flush();
  }
}

void loadConfig()
{
  ETS_GPIO_INTR_DISABLE();
  EEPROM.begin(4096);
  if (EEPROM.read(4095) == 254)
  {
    EEPROM.get(0, config);
    EEPROM.end();
  }
  else
  {
    EEPROM.end();
    saveConfig();
  }
  delay(50);
  ETS_GPIO_INTR_ENABLE();
}

void saveConfig()
{
  ETS_GPIO_INTR_DISABLE();
  EEPROM.begin(4096);
  EEPROM.write(4095, 254);
  EEPROM.put(0, config);
  EEPROM.end();
  delay(50);
  ETS_GPIO_INTR_ENABLE();
}

void setupWebServer()
{
  webServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/index.htm"); });

  webServer.on("/function.js", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/function.js"); });

  webServer.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request)
               { request->send(LittleFS, "/style.css"); });

  webServer.on("/setting", HTTP_GET, [](AsyncWebServerRequest *request)
               {
        config.deviceName = request->getParam("deviceName")->value();
        config.deviceClass = request->getParam("deviceClass")->value().toInt();
        config.espnowNetName = request->getParam("espnowNetName")->value();
        request->send(200);
        saveConfig(); });

  webServer.on("/config", HTTP_GET, [](AsyncWebServerRequest *request)
               {
        String configJson;
        DynamicJsonDocument json(256); // To calculate the buffer size uses https://arduinojson.org/v6/assistant.
        json["firmware"] = config.firmware;
        json["espnowNetName"] = config.espnowNetName;
        json["deviceName"] = config.deviceName;
        json["deviceClass"] = config.deviceClass;
        serializeJsonPretty(json, configJson);
        request->send(200, "application/json", configJson); });

  webServer.on("/restart", HTTP_GET, [](AsyncWebServerRequest *request)
               {request->send(200);
        ESP.restart(); });

  webServer.onNotFound([](AsyncWebServerRequest *request)
                       { request->send(404, "text/plain", "File Not Found"); });

  webServer.begin();
}

void sendSensorConfigMessage()
{
  esp_now_payload_data_t outgoingData{ENDT_SENSOR, ENPT_CONFIG};
  DynamicJsonDocument json(sizeof(esp_now_payload_data_t::message));
  json[MCMT_DEVICE_NAME] = config.deviceName;
  json[MCMT_DEVICE_UNIT] = 1;
  json[MCMT_COMPONENT_TYPE] = HACT_BINARY_SENSOR;
  json[MCMT_DEVICE_CLASS] = config.deviceClass;
  json[MCMT_VALUE_TEMPLATE] = "state";
  json[MCMT_PAYLOAD_ON] = "OPEN";
  json[MCMT_PAYLOAD_OFF] = "CLOSED";
  serializeJsonPretty(json, outgoingData.message);
  char temp[sizeof(esp_now_payload_data_t)]{0};
  memcpy(&temp, &outgoingData, sizeof(esp_now_payload_data_t));
  myNet.sendBroadcastMessage(temp);
}

void sendBatteryConfigMessage()
{
  esp_now_payload_data_t outgoingData{ENDT_SENSOR, ENPT_CONFIG};
  DynamicJsonDocument json(sizeof(esp_now_payload_data_t::message));
  json[MCMT_DEVICE_NAME] = config.deviceName + " battery";
  json[MCMT_DEVICE_UNIT] = 2;
  json[MCMT_COMPONENT_TYPE] = HACT_SENSOR;
  json[MCMT_DEVICE_CLASS] = HASDC_VOLTAGE;
  json[MCMT_VALUE_TEMPLATE] = "battery";
  json[MCMT_UNIT_OF_MEASUREMENT] = "V";
  serializeJsonPretty(json, outgoingData.message);
  char temp[sizeof(esp_now_payload_data_t)]{0};
  memcpy(&temp, &outgoingData, sizeof(esp_now_payload_data_t));
  myNet.sendBroadcastMessage(temp);
}

void sendAttributesMessage()
{
  esp_now_payload_data_t outgoingData{ENDT_SENSOR, ENPT_ATTRIBUTES};
  DynamicJsonDocument json(sizeof(esp_now_payload_data_t::message));
  json["Type"] = "ESP-NOW window sensor";
  json["MCU"] = "ESP8266";
  json["MAC"] = myNet.getNodeMac();
  json["Firmware"] = config.firmware;
  json["Library"] = myNet.getFirmwareVersion();
  serializeJsonPretty(json, outgoingData.message);
  char temp[sizeof(esp_now_payload_data_t)]{0};
  memcpy(&temp, &outgoingData, sizeof(esp_now_payload_data_t));
  myNet.sendBroadcastMessage(temp);
}
