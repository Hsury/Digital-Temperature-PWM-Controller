# Digital-Temperature-PWM-Controller
This device can tell you current temperature and control a heater automatically by using a 10bit ADC to read the voltage of a diode.

### Rule
MSCup 2017, Problem E: [PDF document](http://orymkdf8f.bkt.clouddn.com/E%E9%A2%98%E6%95%B0%E5%AD%97%E6%B8%A9%E5%BA%A6PWM%E6%8E%A7%E5%88%B6%E4%BB%AA.pdf)

### Usage
There are two ways to check its working status and modify its configuration:

##### On the device
It has a 4-bit seven segment display, an on-board LED(simulating the heater) and MENU/UP/DOWN buttons. These allow you to see what is happening and adjust the temperature limit instantly.

##### On the computer
This device has the ability to publish and subscribe data through Wi-Fi. Thanks to this feature, all information and even the temperature curve graph can be browsed on your computer. You can also interact with it in the console.

### Hardware
- ESP8266EX
- TM1637

### Wiring
| GPIO | Pin |
| :------------: | :------------: |
| 2 | Heater |
| 4 | TM1637 CLK |
| 5 | TM1637 DIO |
| 13 | Button Down |
| 14 | Button Up |
| 15 | Button Menu |

### Software
- PlatformIO
- Python 3.6.2

### Dependency

##### PlatformIO
- ArduinoJson
- NTPClient
- SevenSegmentTM1637

##### Python
- json
- numpy
- PyQt5
- Qwt
