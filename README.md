# arduino-projects
Repository for my personal arduino projects, sketches, and libraries

## Folders


### **Projects/** : Arduino projects made in C/C++ with PlatformIO

| Project Name | Desctription |
| -- | --- |
| MKR1010_Indoor_Plant_Monitor | A soil and air sensor for monitoring up to 7 plants. Made for Arduino MKR 1010 WiFi boards, adaptable for Nano 33 IoT boards. Wireless connectivity using WiFi/MQTT. |
| MKR1010_Indoor_Plant_Monitor_V2 | A revorked version of the MKR1010_Indoor_Plant_Monitor project |
| Nano_IoT_Indoor_Air_Monitor | An climate and air quality sensor for Arduino MKR 1010 WiFi and Nano 33 IoT boards using a DFR SEN0335 sensor. WiFi/MQTT. |
| Nano_IoT_Simple_Climate | A minimal climate sensor for Nano 33 IoT boards using an SHT31 sensor. WiFi/MQTT. |
| Common Libraries | Common module implementations shared between PIO projects |
| - Mqtt_Utility | A class for handling MQTT broker connections on Arduino MKR 1010 WiFi and Nano 33 IoT boards. Handles connection, status checking, reconnection, and publishing. |

ToDo for **Projects/** :
- Common libraries for sensors.
- Update Mqtt Utility to match newer project specific implementations
- General code cleanup

