# ESP-NOW window/door sensor for ESP8266

ESP-NOW based window/door sensor for ESP8266. Alternate firmware for Tuya/SmartLife WiFi window/door sensors.

## Features

1. When triggered transmits system information, battery level charge and sensor status (OPEN/CLOSED).
2. Average response time of 1 second (depends on the MCU of the sensor).
3. In setup/update mode creates an access point named "ESP-NOW window XXXXXXXXXXXX" with password "12345678" (IP 192.168.4.1).
4. Automatically adds sensor and battery configuration to Home Assistan via MQTT discovery as a binary_sensor (for sensor) and sensor (for battery).
5. Possibility firmware update over OTA (if is allows the size of the flash memory).
6. Web interface for settings.
  
## Notes

1. ESP-NOW mesh network based on the library [ZHNetwork](https://github.com/aZholtikov/ZHNetwork).
2. For enter to setup/update mode press the button for > 5 seconds. The LED will illuminate. Access point will be available during 120 seconds before the module is powered off.

## Tested on

See [here](https://github.com/aZholtikov/ESP-NOW-Window-Door-Sensor/tree/main/hardware).

## Attention

1. A gateway is required. For details see [ESP-NOW Gateway](https://github.com/aZholtikov/ESP-NOW-Gateway).
2. ESP-NOW network name must be set same of all another ESP-NOW devices in network.
3. If encryption is used, the key must be set same of all another ESP-NOW devices in network.
4. Upload the "data" folder (with web interface) into the filesystem before flashing.
5. For using this firmware on Tuya/SmartLife WiFi window/door sensors, the WiFi module must be replaced with an ESP8266 compatible module (if necessary).
6. Highly recommended connect an external power supply during setup/upgrade.
7. Because this sensor is battery operated, it has an additional controller (MCU) that controls the power of the WiFi module (Module) and transmits data to it for transmission to the network. The communication is done via UART at 9600 speed. Make sure that the protocol is correct before flashing. Details [here](https://github.com/aZholtikov/ESP-NOW-Window-Door-Sensor/tree/main/doc).

Any feedback via [e-mail](mailto:github@zh.com.ru) would be appreciated. Or... [Buy me a coffee](https://paypal.me/aZholtikov).
