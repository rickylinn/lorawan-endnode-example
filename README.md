# lorawan-endnode-example
Simple LoRaWAN end-node project

The device:
- Connects to a LoRaWAN network (e.g. ChirpStack or The Things Network)
- Sends temperature and humidity data periodically (every 10 seconds)
- Displays system status on the LCD
- Supports manual transmission with a long press or multiple button presses

---

## 🛠️ Hardware Used

| Component          | Details                              |
|-------------------|---------------------------------------|
| Microcontroller    | Arduino UNO                          |
| LoRa Shield        | Cytron RFM LoRa Shield (RFM95 based) |
| Temperature Sensor | DHT11                                |
| LCD Display        | 16x2 I2C LCD (Address: `0x27`)       |
| Button             | 1x Push button (pull-up enabled)     |

---

## 📦 Features

- OTAA Join using LMIC
- LoRaWAN uplinks every `10` seconds
- Manual uplink: 
  - Hold button for 3 seconds
  - Or press button 3 times within 2 seconds
- Uses dummy values if DHT11 fails to read
- LCD shows:
  - Join status
  - TX activity
  - Errors
  - Basic static info (e.g. “HELLO LORAWAN”)
 
## 🧰 Libraries Required

Install the following libraries from the Arduino Library Manager:

- [LMIC](https://github.com/matthijskooijman/arduino-lmic)
- [DHT sensor library](https://github.com/adafruit/DHT-sensor-library)
- [Adafruit Unified Sensor](https://github.com/adafruit/Adafruit_Sensor)
- [LiquidCrystal_I2C](https://github.com/johnrickman/LiquidCrystal_I2C)

📝 Notes
LMIC is very sensitive to timing. The sketch avoids calling DHT read functions during the join phase to prevent radio timing conflicts.

DHT reads are cached to avoid blocking os_runloop_once().

If using another sensor (e.g., BME280), replace DHT logic accordingly.
