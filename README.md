# WEMOS-NTC-Dotmatrix002

> **Clock + Dot Matrix LED with NTP sync**  
> โปรเจกต์แสดงนาฬิกาดิจิตอลบน Dot Matrix 8×8 จำนวน 4 แผง โดยดึงเวลาจาก NTP Server ผ่าน Wi-Fi ด้วยบอร์ด **WEMOS D1 mini (ESP8266)** และไดรเวอร์ **MAX7219**

---

## สารบัญ

1. [ภาพรวมโปรเจกต์](#ภาพรวมโปรเจกต์)
2. [สเป็กบอร์ดและอุปกรณ์](#สเป็กบอร์ดและอุปกรณ์)
3. [ผัง Pinout](#ผัง-pinout)
4. [หลักการทำงาน](#หลักการทำงาน)
5. [ผังวงจร / การต่อสาย](#ผังวงจร--การต่อสาย)
6. [ขั้นตอนการต่อวงจร](#ขั้นตอนการต่อวงจร)
7. [ขั้นตอนการทำงานของโค้ด](#ขั้นตอนการทำงานของโค้ด)
8. [การตั้งค่าและใช้งาน](#การตั้งค่าและใช้งาน)
9. [ข้อควรระวัง](#ข้อควรระวัง)
10. [การแก้ไขปัญหาเบื้องต้น](#การแก้ไขปัญหาเบื้องต้น)
11. [เวอร์ชัน](#เวอร์ชัน)

---

## ภาพรวมโปรเจกต์

โปรเจกต์นี้อ่านเวลาจาก **NTP (Network Time Protocol)** ผ่านอินเทอร์เน็ต Wi-Fi และแสดงผลในรูปแบบ `HH:MM` บนจอ Dot Matrix LED ขนาด 8×8 ที่ต่อกัน 4 แผงแนวนอน โดยมีจุดเด่นคือ

- แสดงเวลาแบบ **24 ชั่วโมง**
- **เติมเลข 0 ด้านหน้าอัตโนมัติ** เมื่อชั่วโมงหรือนาทีเป็นหลักหน่วย เช่น `08:05`
- **เครื่องหมาย `:` กระพริบทุก 0.5 วินาที**
- ซิงค์เวลากับ NTP Server ของไทย (`th.pool.ntp.org`) ทุก 5 นาที
- ใช้ไลบรารี่ `Max72xxPanel` เพื่อควบคุม MAX7219 ผ่าน SPI
- มี **Web Server** แสดงนาฬิกาตัวใหญ่ + **mDNS** + **NTFY Notification**

---

## สเป็กบอร์ดและอุปกรณ์

| รายการ | รุ่น / ค่า | หมายเหตุ |
|---|---|---|
| บอร์ดหลัก | WEMOS D1 mini / NodeMCU / ESP-12E | ESP8266 @ 80/160 MHz |
| จอแสดงผล | MAX7219 Dot Matrix 8×8 × 4 แผง | ต่อกันแนวนอน |
| ไลบรารี่ | `TimeLib`, `ESP8266WiFi`, `ESP8266WebServer`, `ESP8266mDNS`, `ESP8266HTTPClient`, `EEPROM`, `WiFiUdp`, `SPI`, `Adafruit_GFX`, `Max72xxPanel` | |
| แหล่งจ่ายไฟ | 5V / 1A ขึ้นไป | ผ่าน USB หรือขา 5V |
| Wi-Fi | 2.4 GHz | ต้องเป็น SSID 2.4G เท่านั้น |
| Web Server | HTTP พอร์ต 80 | แสดงนาฬิกาใหญ่ + API /time |
| mDNS | `wemos-clock.local` | เข้าหน้าเว็บโดยไม่ต้องจำ IP |
| NTFY Notification | `/config` ตั้งค่าได้ | แจ้งเตือนทุก 1-24 ชั่วโมง |

---

## ผัง Pinout

### WEMOS D1 mini (ESP8266) → MAX7219 Dot Matrix

> สัญญาณ SPI ของ ESP8266 บน WEMOS D1 mini ใช้ขาดังนี้

| ขา MAX7219 | ขา ESP8266 | ชื่อ WEMOS | คำอธิบาย |
|---|---|---|---|
| VCC | 3V3 หรือ 5V | 3V3 / 5V | ไฟเลี้ยง MAX7219 |
| GND | GND | G | กราวนด์ |
| DIN | GPIO13 | D7 | SPI MOSI (ข้อมูล) |
| CS / LOAD | GPIO12 | D6 | SPI SS (Chip Select) |
| CLK | GPIO14 | D5 | SPI Clock |

### สรุป Pin ที่ใช้ในโค้ด

```cpp
int pinCS = D6;   // CS → GPIO12
```

- `D5` = CLK
- `D7` = DIN (MOSI)
- `D6` = CS (กำหนดเองในโค้ด)

---

## หลักการทำงาน

1. **เชื่อมต่อ Wi-Fi**: บอร์ด ESP8266 เชื่อมต่อกับ Access Point ที่กำหนดใน `ssid[]` และ `pass[]`
2. **ขอเวลาจาก NTP Server**: ส่ง UDP packet ไปยัง `th.pool.ntp.org` ผ่านพอร์ต 123
3. **แปลง timestamp**: NTP ส่งค่า **seconds since 1900** กลับมา โค้ดแปลงเป็น **Unix time (seconds since 1970)** และบวก Time zone `UTC+7`
4. **บันทึกเวลาใน TimeLib**: ใช้ `setSyncProvider(getNtpTime)` ให้ `TimeLib` จัดการเวลา
5. **อ่านชั่วโมง/นาที**: ใน `loop()` เรียก `hour()` และ `minute()`
6. **จัดรูปแบบ 2 หลัก**: ใช้ `snprintf(..., "%02d", ...)` เพื่อให้ได้เสมอ 2 หลัก เช่น `08`, `05`
7. **วาดลง Dot Matrix**: ใช้ `matrix.drawChar()` วาดตัวเลข 5×7 พร้อมเครื่องหมาย `:` ตรงกลาง
8. **กระพริบ `:`**: ทุก 0.5 วินาที สลับ `colonVisible` แล้วเรียก `renderClock()` วาดใหม่ (ไม่ใช้ `delay()`)
9. **รีเฟรชจอ**: `matrix.write()` ส่ง bitmap ไปยัง MAX7219
10. **Web Server**: ESP8266 เปิด HTTP server พอร์ต 80 แสดงหน้าเว็บนาฬิกาตัวใหญ่
11. **API /time**: หน้าเว็บดึงเวลาจาก `/time` ทุก 1 วินาที ผ่าน JavaScript fetch
12. **NTFY Notification**: บันทึก config ใน EEPROM ตรวจสอบทุกนาที ส่งแจ้งเตือนทุก 1-24 ชั่วโมงตามที่ตั้งไว้
13. **ซิงค์ซ้ำอัตโนมัติ**: ทุก 5 นาที `TimeLib` จะเรียก `getNtpTime()` ซิงค์เวลาใหม่

---

## ผังวงจร / การต่อสาย

```text
+----------------+        +---------------------------+
|   WEMOS D1     |        |   MAX7219 Dot Matrix 4    |
|   mini/ESP8266 |        |   in 1 (Horizontal)       |
+----------------+        +---------------------------+
| 3V3  ----------+------> | VCC                       |
| GND  ----------+------> | GND                       |
| D7   (MOSI)    +------> | DIN                       |
| D6   (CS)      +------> | CS                        |
| D5   (CLK)     +------> | CLK                       |
+----------------+        +---------------------------+
          |
          |  5V USB Power
          v
+----------------+
|   USB 5V/1A    |
+----------------+
```

### ลำดับการต่อสายแบบ Daisy-chain (หากมีหลายแผง)

ถ้ามี Dot Matrix หลายแผง ให้ต่อแผงแรกเข้ากับ ESP8266 แล้วต่อแผงถัดไปแบบ chain

```text
ESP8266 → แผง 1 → แผง 2 → แผง 3 → แผง 4
             DIN    DOUT   DIN    DOUT ...
             CLK    CLK    CLK    CLK
             CS     CS     CS     CS     (CS ร่วมกัน)
```

---

## ขั้นตอนการต่อวงจร

1. ปิดแหล่งจ่ายไฟก่อนเชื่อมต่อสายเสมอ
2. ต่อ `VCC` ของ MAX7219 ไปยัง `5V` หรือ `3V3` ของ WEMOS
3. ต่อ `GND` ของ MAX7219 ไปยัง `GND` ของ WEMOS
4. ต่อ `DIN` ของ MAX7219 ไปยังขา `D7` ของ WEMOS
5. ต่อ `CS` ของ MAX7219 ไปยังขา `D6` ของ WEMOS
6. ต่อ `CLK` ของ MAX7219 ไปยังขา `D5` ของ WEMOS
7. ตรวจสอบสายไฟซ้ำอีกครั้ง โดยเฉพาะขา VCC และ GND
8. เปิดไฟเลี้ยง 5V ผ่าน USB หรือ adapter

---

## ขั้นตอนการทำงานของโค้ด

### `setup()`

1. เปิด Serial Monitor ความเร็ว 9600 baud
2. เชื่อมต่อ Wi-Fi ด้วย SSID/Password ที่ระบุ
3. เริ่ม UDP บนพอร์ต 8888
4. ตั้ง `getNtpTime()` เป็น Time provider และซิงค์ทุก 300 วินาที
5. เริ่ม Web Server พอร์ต 80
   - `/` → ส่งหน้าเว็บนาฬิกาตัวใหญ่
   - `/time` → API ส่ง JSON เวลาปัจจุบัน
   - `/config` → หน้าตั้งค่า NTFY
   - `/save` → รับข้อมูลฟอร์มและบันทึก EEPROM
6. เริ่ม mDNS ด้วยชื่อ `wemos-clock.local`
   - สามารถเปิดเว็บด้วย `http://wemos-clock.local` แทนการจำ IP
7. โหลดค่า NTFY จาก EEPROM (ถ้าไม่มีจะใช้ค่าเริ่มต้น `ntfy.sh/wemos-clock`)
8. กำหนดค่า Dot Matrix
   - `setIntensity(0)` ความสว่างต่ำสุด (0–15)
   - `setRotation(..., 1)` หมุนทุกแผงให้ตัวอักษรตั้งตรง

### `loop()`

1. เรียก `webServer.handleClient()` และ `MDNS.update()` ทุก loop
2. ตรวจสอบว่าได้รับเวลาจาก NTP แล้ว (`timeStatus() != timeNotSet`)
3. ทุก 0.5 วินาที (`millis()`) สลับ `colonVisible` แล้วเรียก `renderClock()` เพื่อ **กระพริบ `:`**
4. `renderClock(bool showColon)` ล้างจอ, จัดรูปแบบ `HH`/`MM` 2 หลักเสมอ, วาดตัวเลข และวาด `:` เฉพาะเมื่อ `showColon == true`
5. พิมพ์เวลาออก Serial Monitor เมื่อนาทีเปลี่ยน
6. ถ้า `second() == 0` และ NTFY เปิดใช้งาน ให้ตรวจสอบ `isNtfyDue()` และส่งแจ้งเตือน
7. ไม่ใช้ `delay()` ใน loop ทำให้ Web Server ตอบสนองได้ทันที

### `getNtpTime()`

1. ทิ้ง UDP packet เก่าที่ค้าง
2. DNS แปลง `th.pool.ntp.org` → IP
3. ส่ง NTP request ไปยังพอร์ต 123
4. รอ response สูงสุด 1500 ms
5. อ่าน 4 bytes ตำแหน่ง 40–43 แล้วแปลงเป็น Unix time
6. ปรับ Time zone UTC+7
7. คืนค่าเวลา หรือคืน 0 ถ้า timeout

### `sendNTPpacket()`

1. เคลียร์ buffer 48 bytes
2. กำหนด NTP header
   - `packetBuffer[0] = 0b11100011` → LI=0, Version=3, Mode=Client
   - Stratum, Polling, Precision
3. ใส่ Reference Identifier
4. ส่ง UDP packet ไปยัง NTP server

### Web Server

#### `handleRoot()`
- เรียก `getClockPage()` ส่ง HTML/CSS/JS กลับไป
- หน้าเว็บแสดง `HH:MM:SS` ตัวใหญ่เต็มจอ + วันที่

#### `handleTime()`
- เรียก `getTimeJson()` ส่ง JSON เวลาปัจจุบัน
- ตัวอย่าง: `{"hour":8,"minute":5,"second":32,"day":4,"month":10,"year":2026}`

#### `getClockPage()`
- สร้าง HTML embedded ใน String
- ใช้ `fetch('/time')` ดึงเวลาทุก 1 วินาที
- ตัวเลขขนาดใหญ่ 18vw ปรับตามหน้าจอ mobile

#### NTFY Functions
- `loadNtfyConfig()` / `saveNtfyConfig()` — อ่าน/เขียน config ลง EEPROM
- `isNtfyDue()` — ตรวจสอบว่าครบรอบการแจ้งเตือนหรือยัง
- `sendNtfyNotification()` — ส่ง HTTP PUT ไปยัง NTFY server
- `getConfigPage()` — สร้างหน้าฟอร์มตั้งค่า NTFY

---

## การตั้งค่าและใช้งาน

### 1. ติดตั้งไลบรารี่ที่จำเป็น

ผ่าน **Library Manager** ใน Arduino IDE:

- `Time` by Paul Stoffregen
- `ESP8266WiFi` (มาพร้อม ESP8266 core)
- `ESP8266WebServer` (มาพร้อม ESP8266 core)
- `ESP8266mDNS` (มาพร้อม ESP8266 core)
- `ESP8266HTTPClient` (มาพร้อม ESP8266 core)
- `Adafruit GFX Library`
- `arduino-Max72xxPanel` (ติดตั้งจาก ZIP `arduino-Max72xxPanel-main.zip` ในโฟลเดอร์)

### 2. แก้ไข Wi-Fi

เปิดไฟล์ `.ino` แล้วแก้ไข 2 บรรทัดนี้

```cpp
const char ssid[] = "Jib_2.4G";
const char pass[] = "12345678";
```

### 3. เลือกบอร์ด

ใน Arduino IDE:

```text
Tools → Board → LOLIN(WEMOS) D1 mini (Clone)
Tools → Flash Size → 4MB (FS:2MB OTA:~1019KB)
Tools → Upload Speed → 921600
```

### 4. อัปโหลดโค้ด

กด **Verify** แล้ว **Upload** หลังจากอัปโหลดเสร็จ เปิด Serial Monitor ดู IP address และข้อความ NTP sync

### 5. เข้าหน้าเว็บ

- ผ่าน IP: `http://<IP จาก Serial Monitor>`
- ผ่าน mDNS: `http://wemos-clock.local` (บน Windows/macOS/Linux ที่รองรับ mDNS)

### 6. ตั้งค่า NTFY Notification

1. เปิดหน้า `http://wemos-clock.local/config`
2. กรอก NTFY server (เช่น `ntfy.sh`)
3. กรอก topic (เช่น `wemos-clock`)
4. เลือกระยะห่าง 1-24 ชั่วโมง
5. ติ๊ก "เปิดใช้งาน NTFY"
6. กด **บันทึก**

หรือติดตั้ง NTFY app บนมือถือแล้ว subscribe topic ของคุณ

---

## ข้อควรระวัง

| หัวข้อ | คำแนะนำ |
|---|---|
| **ไฟเลี้ยง** | ใช้แหล่งจ่าย 5V/1A ขึ้นไป เพราะ MAX7219 + LED ทั้งหมดกินกระแสสูง |
| **ขั้ว VCC/GND** | ตรวจสอบให้ถูกต้องก่อนเปิดไฟ กลับขั้วอาจทำให้ MAX7219 เสีย |
| **แรงดัน Logic** | DIN/CS/CLK เป็น 3.3V logic บอร์ด ESP8266 ใช้ได้เลย ไม่ต้องใช้ level shifter |
| **Heat of LED matrix** | ถ้าตั้งความสว่างสูง (`setIntensity(15)`) จอและ MAX7219 ร้อน ควรระวัง |
| **Wi-Fi 2.4G เท่านั้น** | ESP8266 ไม่รองรับ 5 GHz |
| **NTP Timeout** | ถ้า router ไม่มีอินเทอร์เน็ต หรือ DNS ใช้งานไม่ได้ เวลาจะไม่ถูกตั้ง |
| **Pin D6** | อย่าลืมต่อ CS ของ MAX7219 ไปที่ D6 ตามที่กำหนดใน `pinCS` |
| **Serial Monitor** | เปิด Serial Monitor ที่ baud rate 9600 |

---

## การแก้ไขปัญหาเบื้องต้น

| อาการ | สาเหตุที่เป็นไปได้ | วิธีแก้ |
|---|---|---|
| จอไม่ติด | ไฟไม่เข้า / สาย GND หลุด | ตรวจสาย VCC, GND |
| จอติดแต่แสดงผลผิด | การหมุนจอไม่ตรง | ปรับค่า `setRotation()` จาก 1 เป็น 0/2/3 |
| จอแสดง `00:00` ค้าง | ยังไม่ได้ IP / NTP timeout | ตรวจ Wi-Fi, เปิด Serial Monitor |
| ชั่วโมงไม่ตรง | Time zone ผิด | ตรวจ `const int timeZone = 7;` |
| ชั่วโมงหรือนาทีไม่มีเลข 0 นำ | ใช้โค้ดเวอร์ชันเก่า | ใช้ `snprintf("%02d", ...)` |
| ตัวเลขเบี้ยว / ขาด pixel | สาย SPI ยาว / สัญญาณรบกวน | ใช้สายสั้น หรือลดความยาวสาย |
| เปิด `wemos-clock.local` ไม่ได้ | mDNS ไม่ทำงาน / router ไม่รองรับ | ใช้ IP แทน |
| NTFY ไม่ส่ง | topic/server ผิด / ปิดใช้งาน | ตรวจ `/config` และ Serial Monitor |

---

## เวอร์ชัน

| เวอร์ชัน | วันที่ | รายละเอียด |
|---|---|---|
| **2.1.0** | 2026-10-04 | ทำให้เครื่องหมาย `:` กระพริบทุก 0.5 วินาที, ลบ `delay()` ออกจาก loop |
| **2.0.0** | 2026-10-04 | เพิ่ม Web Server นาฬิกาตัวใหญ่, mDNS, NTFY Notification ตั้งค่าระยะห่าง 1-24 ชม. |
| **1.0.0** | 2026-10-04 | เพิ่ม comment ทั้งหมด, README.md, เติมเลข 0 ด้านหน้าชั่วโมง/นาที |

---

## ไฟล์ในโปรเจกต์

```
WEMOS-NTC-Dotmatrix002/
├── WEMOS-NTC-Dotmatrix002.ino    # ไฟล์ Arduino หลัก
├── arduino-Max72xxPanel-main.zip # ไลบรารี่ Max72xxPanel
└── README.md                       # เอกสารนี้
```

---

## License / ข้อกำหนด

โค้ดต้นฉบับมาจากตัวอย่าง `TimeNTP_ESP8266WiFi.ino` ของไลบรารี่ TimeLib ซึ่งเป็น Open Source การใช้งานต่อยอดควรปฏิบัติตามลิขสิทธิ์ของแต่ละไลบรารี่ที่ใช้
