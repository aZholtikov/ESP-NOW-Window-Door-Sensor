# Communication protocol

The protocol can be read with 2 USB-TTL converters. RX of the first one is connected to the RX of the WiFi module to read the MCU data. Connect the RX of the second to the TX of the WiFi module to read the module's data. Don't forget to connect all GND (of both converters and module).

Communication protocol used in the firmware (only necessary "cuts" from the original protocol):

```text
1. Normal mode. Sensor triggering.

      Module power is on
      Module sends            55 AA 00 01 00 00 00                                        (Initial message)
      MCU returns             55 AA 00 01 00 ............                                 (MCU system information)
      Module sends            55 AA 00 02 00 01 04 06                                     (Network connection established)
      MCU returns             55 AA 00 02 00 00 01                                        (MCU confirmation message)
      MCU sends               55 AA 00 08 00 0C 00 01 01 01 01 01 03 04 00 01 02 23       (Battery status. 02 23 - high, 01 22 - medium, 00 21 - low)
      Module returns          55 AA 00 08 00 01 00 08                                     (Confirmation message)
      MCU sends               55 AA 00 08 00 0C 00 02 02 02 02 02 01 01 01 00 22          (Sensor position. 01 23 - open, 00 22 - closed)
      Module returns          55 AA 00 08 00 01 00 08                                     (Confirmation message)
      Module power off

2. Sending the battery status. Pressing the button. Not used in the firmware.

      Module power is on
      Module sends            55 AA 00 01 00 00 00                                        (Initial message)
      MCU returns             55 AA 00 01 00 00 ............                              (MCU system information)
      Module sends            55 AA 00 02 00 01 04 06                                     (Network connection established)
      MCU returns             55 AA 00 02 00 00 01                                        (MCU confirmation message)
      MCU sends               55 AA 00 08 00 0C 00 01 01 01 01 01 03 04 00 01 02 23       (Battery status. 02 23 - high, 01 22 - medium, 00 21 - low)
      Module returns          55 AA 00 08 00 01 00 08                                     (Confirmation message)
      Module power off

3. Update mode. Pressing the button for > 5 seconds - the LED light on.

      Module power on
      Module sends            55 AA 00 01 00 00 00                                        (Initial message)
      MCU returns             55 AA 00 01 00 00 ............                              (MCU system information)
      Module sends            55 AA 00 02 00 01 04 06                                     (Network connection established)
      MCU returns             55 AA 00 02 00 00 01                                        (MCU confirmation message)
      MCU sends               55 AA 00 03 00 00 02                                        (Message for switching to setting mode)
      Module returns          55 AA 00 03 00 00 02                                        (Confirmation message)

      Update mode has started. Will be available during 120 seconds until the module is powered off.
      After updating and rebooting the module will return to normal mode - the LED will go off.
      Highly recommended connect an external power supply during update.
```
