/*
 * ============================================================================
 * Project  : WEMOS-NTC-Dotmatrix002
 * File     : WEMOS-NTC-Dotmatrix002.ino
 * Version  : 2.1.0
 * Date     : 2026-10-04
 * Author   : OpenCode (Generated)
 * Board    : WEMOS D1 mini (ESP8266) / NodeMCU / ESP-12E
 * ============================================================================
 * Description:
 *   โปรเจกต์แสดงนาฬิกาดิจิตอลบน Dot Matrix LED 8x8 จำนวน 4 แผง (MAX7219)
 *   โดยดึงเวลาจาก NTP Server (th.pool.ntp.org) ผ่าน Wi-Fi
 *   แสดงผลรูปแบบ HH:MM บน Dot Matrix พร้อมเติมเลข 0 ด้านหน้าอัตโนมัติ
 *   มี Web Server + หน้าเว็บนาฬิกาตัวใหญ่ + mDNS + NTFY Notification
 * ============================================================================
 * Required libraries:
 *   - TimeLib.h            (Time by Paul Stoffregen)
 *   - ESP8266WiFi.h        (ESP8266 Core)
 *   - ESP8266WebServer.h   (ESP8266 Core)
 *   - ESP8266mDNS.h        (ESP8266 Core)
 *   - ESP8266HTTPClient.h  (ESP8266 Core)
 *   - WiFiUdp.h            (ESP8266 Core)
 *   - EEPROM.h             (Arduino Built-in)
 *   - SPI.h                (Arduino Built-in)
 *   - Adafruit_GFX.h       (Adafruit GFX Library)
 *   - Max72xxPanel.h       (arduino-Max72xxPanel)
 * ============================================================================
 */

// ===== Library Includes =====
#include <TimeLib.h>            // จัดการเวลา / วันที่
#include <ESP8266WiFi.h>        // Wi-Fi สำหรับ ESP8266
#include <ESP8266WebServer.h>  // Web Server สำหรับ ESP8266
#include <ESP8266mDNS.h>       // mDNS สำหรับเข้าหน้าเว็บผ่านชื่อแทน IP
#include <ESP8266HTTPClient.h> // HTTP Client สำหรับส่ง NTFY Notification
#include <WiFiUdp.h>            // UDP สำหรับส่ง/รับ NTP packet
#include <EEPROM.h>             // บันทึกค่า NTFY config ลง Flash
#include <SPI.h>                // SPI สำหรับสื่อสารกับ MAX7219
#include <Adafruit_GFX.h>       // กราฟฟิกสำหรับ Dot Matrix
#include <Max72xxPanel.h>       // ไดรเวอร์ Dot Matrix MAX7219

// ===== Hardware Configurations =====
// CS pin สำหรับ MAX7219 (ESP8266 GPIO12 / D6 บน WEMOS D1 mini)
int pinCS = D6;

// จำนวนแผง Dot Matrix แนวนอน 4 แผง, แนวตั้ง 1 แผง
int numberOfHorizontalDisplays = 4;
int numberOfVerticalDisplays   = 1;

// สร้างอ็อบเจกต์ matrix โดยระบุ pin CS และจำนวนจอ
Max72xxPanel matrix = Max72xxPanel(pinCS, numberOfHorizontalDisplays, numberOfVerticalDisplays);

// ===== Display Parameters =====
int wait   = 150;  // ระยะห่างการเลื่อนข้อความ (ms) — ไม่ใช้ในโหมดนาฬิกา
int spacer = 1;    // ระยะห่างระหว่างตัวอักษร (pixel)
int width  = 5 + spacer;  // ความกว้างตัวอักษร 5 px + ช่องว่าง 1 px

// ===== Wi-Fi Credentials =====
// เปลี่ยนเป็นของเราเตอร์ที่ใช้งานจริง
const char ssid[] = "Jib_2.4G";  // SSID
const char pass[] = "12345678";  // Password

// ===== NTP Configuration =====
// NTP server หลักของไทย
static const char ntpServerName[] = "th.pool.ntp.org";

// Time zone: ไทย UTC+7
const int timeZone = 7;

// UDP port สำหรับรอบรับ NTP response
WiFiUDP Udp;
unsigned int localPort = 8888;

// ===== Web Server =====
ESP8266WebServer webServer(80);  // Web Server ที่พอร์ต 80

// ===== NTFY Notification Configuration =====
// โครงสร้างเก็บค่าการตั้งค่า NTFY ใน EEPROM
// ขนาดรวม: 4 (magic) + 128 (server) + 64 (topic) + 1 (intervalHours) + 1 (enabled) = 198 bytes
struct NtfyConfig {
  char magic[4];          // "NTFY" ใช้ตรวจสอบว่าข้อมูลถูกต้อง
  char server[128];       // URL NTFY server เช่น ntfy.sh
  char topic[64];         // Topic name
  uint8_t intervalHours;  // ระยะห่างการแจ้งเตือน 1-24 ชั่วโมง
  uint8_t enabled;        // 0=ปิด, 1=เปิด
};

NtfyConfig ntfyConfig;
const int EEPROM_SIZE = 256;
const int EEPROM_ADDR_NTFY = 0;

// ตัวแปรเก็บชั่วโมงล่าสุดที่ส่งแจ้งเตือน
int lastNotifyHour = -1;

// ===== Function Prototypes =====
time_t getNtpTime();
void digitalClockDisplay();
void printDigits(int digits);
void sendNTPpacket(IPAddress &address);
void handleRoot();
void handleTime();
void handleNtfyConfig();
void handleNtfySave();
String getClockPage();
String getConfigPage();
String getTimeJson();
void loadNtfyConfig();
void saveNtfyConfig();
void sendNtfyNotification();
bool isNtfyDue();
void renderClock(bool showColon);

// ===== ตัวแปรสำหรับนาฬิกาและการกระพริบ =====
time_t prevDisplay = 0;              // เวลาที่แสดงล่าสุด
unsigned long lastBlinkMillis = 0;   // เวลาล่าสุดที่กระพริบ colon (millis)
bool colonVisible = true;            // สถานะการแสดงผล colon
int lastSerialMinute = -1;           // นาทีล่าสุดที่พิมพ์ออก Serial
int lastNotifyMinute = -1;           // นาทีล่าสุดที่ตรวจสอบ NTFY (กันส่งซ้ำ)

/*
 * ============================================================================
 * setup() — เริ่มต้นระบบครั้งเดียวเมื่อบอร์ด Reset / Power on
 * ============================================================================
 * ขั้นตอน:
 *   1. เปิด Serial Monitor
 *   2. เชื่อมต่อ Wi-Fi
 *   3. เริ่ม UDP
 *   4. ตั้งค่า NTP sync provider
 *   5. เริ่ม Web Server, mDNS, โหลดค่า NTFY จาก EEPROM
 *   6. เริ่ม Dot Matrix และกำหนดความสว่าง / การหมุนจอ
 * ============================================================================
 */
void setup() {
  Serial.begin(9600);
  // รอ Serial Monitor (จำเป็นสำหรับบอร์ดที่ใช้ USB-CDC เช่น Leonardo, แต่ ESP8266 ไม่จำเป็น)
  while (!Serial)
    ;
  delay(250);

  Serial.println("TimeNTP Example");
  Serial.print("Connecting to ");
  Serial.println(ssid);

  // เริ่มต้นเชื่อมต่อ Wi-Fi
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // เชื่อมต่อสำเร็จ
  Serial.println();
  Serial.print("IP number assigned by DHCP is ");
  Serial.println(WiFi.localIP());

  Serial.println("Starting UDP");
  Udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(Udp.localPort());

  Serial.println("waiting for sync");
  // กำหนดให้ฟังก์ชัน getNtpTime() เป็น Time provider
  setSyncProvider(getNtpTime);
  // ตั้งช่วงเวลาซิงค์กับ NTP ทุก 300 วินาที (5 นาที)
  setSyncInterval(300);

  // โหลดค่า NTFY จาก EEPROM
  loadNtfyConfig();

  // ===== เริ่ม Web Server =====
  webServer.on("/", HTTP_GET, handleRoot);            // หน้าแรกแสดงนาฬิกาใหญ่
  webServer.on("/time", HTTP_GET, handleTime);         // API ส่งเวลา JSON
  webServer.on("/config", HTTP_GET, handleNtfyConfig); // หน้าตั้งค่า NTFY
  webServer.on("/save", HTTP_POST, handleNtfySave);    // บันทึกค่า NTFY
  webServer.begin();
  Serial.println("HTTP web server started");
  Serial.print("Open browser at http://");
  Serial.println(WiFi.localIP());

  // ===== เริ่ม mDNS =====
  // สามารถเปิดหน้าเว็บด้วย http://wemos-clock.local แทนการจำ IP
  if (MDNS.begin("wemos-clock")) {
    Serial.println("MDNS responder started");
    Serial.println("You can also open http://wemos-clock.local");
    MDNS.addService("http", "tcp", 80);  // ประกาศบริการ HTTP บน mDNS
  } else {
    Serial.println("Error setting up MDNS responder!");
  }

  // ===== ตั้งค่า Dot Matrix =====
  // ความสว่าง 0-15 (0 = มืดสุด, 15 = สว่างสุด)
  matrix.setIntensity(0);

  // หมุนจอแต่ละแผงให้ตัวอักษรตั้งขึ้น
  // ค่า 1 = หมุน 90 องศา (ขึ้นอยู่กับทิศทางติดตั้งจริง)
  matrix.setRotation(0, 1);
  matrix.setRotation(1, 1);
  matrix.setRotation(2, 1);
  matrix.setRotation(3, 1);
}

/*
 * ============================================================================
 * loop() — ทำงานซ้ำไปมา
 * ============================================================================
 * ขั้นตอน:
 *   1. รับ request จาก Web Server และอัปเดต mDNS
 *   2. ตรวจสอบว่าเวลาได้รับการซิงค์จาก NTP แล้ว (timeStatus() != timeNotSet)
 *   3. ทุก 0.5 วินาที: สลับสถานะ colon แล้ววาดจอใหม่ (กระพริบเครื่องหมาย :)
 *   4. เมื่อนาทีเปลี่ยน: พิมพ์เวลาออก Serial Monitor
 *   5. ตรวจสอบและส่ง NTFY Notification
 *   6. ไม่ใช้ delay() ทำให้ Web Server ตอบสนองได้ทันที
 * ============================================================================
 */
void loop() {
  // รับ request จาก Web Server และอัปเดต mDNS อย่างต่อเนื่อง
  webServer.handleClient();
  MDNS.update();

  if (timeStatus() != timeNotSet) {
    // ===== กระพริบเครื่องหมาย : ทุก 0.5 วินาที =====
    unsigned long nowMs = millis();
    if (nowMs - lastBlinkMillis >= 500) {
      lastBlinkMillis = nowMs;
      colonVisible = !colonVisible;   // สลับสถานะเปิด/ปิด colon
      prevDisplay = now();
      renderClock(colonVisible);      // วาดนาฬิกาใหม่
    }

    // ===== พิมพ์เวลาออก Serial Monitor เมื่อนาทีเปลี่ยน =====
    if (minute() != lastSerialMinute) {
      lastSerialMinute = minute();
      digitalClockDisplay();
    }

    // ===== ตรวจสอบและส่ง NTFY Notification =====
    // ตรวจสอบเฉพาะต้นนาที (second()==0) และไม่ซ้ำนาทีเดิม
    if (ntfyConfig.enabled && second() == 0 && minute() != lastNotifyMinute) {
      lastNotifyMinute = minute();
      if (isNtfyDue()) {
        sendNtfyNotification();
        lastNotifyHour = hour();
      }
    }
  }
}

/*
 * ============================================================================
 * renderClock(bool showColon) — วาดนาฬิกา HH:MM ลง Dot Matrix
 * showColon = true  → แสดงเครื่องหมาย ':'
 * showColon = false → ซ่อนเครื่องหมาย ':' (สำหรับกระพริบ)
 * ============================================================================
 */
void renderClock(bool showColon) {
  // ล้างหน้าจอ Dot Matrix
  matrix.fillScreen(LOW);

  // สร้างสตริงชั่วโมงและนาทีแบบ 2 หลักเสมอ (เติม 0 ด้านหน้า)
  char hourStr[3];
  char minuteStr[3];
  snprintf(hourStr,   sizeof(hourStr),   "%02d", hour());
  snprintf(minuteStr, sizeof(minuteStr), "%02d", minute());

  // --- วาดชั่วโมง ---
  // x=3 : ตัวเลขหลักสิบของชั่วโมง
  matrix.drawChar(3, 0, hourStr[0], HIGH, LOW, 1);
  // x=9 : ตัวเลขหลักหน่วยของชั่วโมง
  matrix.drawChar(9, 0, hourStr[1], HIGH, LOW, 1);

  // --- วาดเครื่องหมาย : (colon) ---
  // ASCII 58 = ':' — วาดเฉพาะเมื่อ showColon เป็น true
  if (showColon) {
    matrix.drawChar(14, 0, 58, HIGH, LOW, 1);
  }

  // --- วาดนาที ---
  // x=19 : ตัวเลขหลักสิบของนาที
  matrix.drawChar(19, 0, minuteStr[0], HIGH, LOW, 1);
  // x=25 : ตัวเลขหลักหน่วยของนาที
  matrix.drawChar(25, 0, minuteStr[1], HIGH, LOW, 1);

  // ส่ง bitmap ไปยัง MAX7219
  matrix.write();
}

/*
 * ============================================================================
 * digitalClockDisplay() — พิมพ์เวลา/วันที่ ออก Serial Monitor
 * รูปแบบ: HH:MM:SS DD.MM.YYYY
 * ============================================================================
 */
void digitalClockDisplay() {
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print(" ");
  Serial.print(day());
  Serial.print(".");
  Serial.print(month());
  Serial.print(".");
  Serial.print(year());
  Serial.println();
}

/*
 * ============================================================================
 * printDigits(int digits) — เครื่องมือช่วยพิมพ์ ":" และเติม 0 ด้านหน้า
 * ============================================================================
 */
void printDigits(int digits) {
  Serial.print(":");
  if (digits < 10)
    Serial.print('0');
  Serial.print(digits);
}

/*
 * ============================================================================
 * NTP Code
 * ============================================================================
 */

const int NTP_PACKET_SIZE = 48;      // NTP packet มีขนาด 48 bytes
byte packetBuffer[NTP_PACKET_SIZE];  // buffer เก็บ packet ขาเข้า/ขาออก

/*
 * getNtpTime() — ขอเวลาจาก NTP server
 * ขั้นตอน:
 *   1. ทิ้ง packet UDP เก่าที่ค้างอยู่
 *   2. แปลงชื่อโดเมน NTP เป็น IP Address
 *   3. ส่ง NTP request packet ไปยัง port 123
 *   4. รอ response สูงสุด 1.5 วินาที
 *   5. แปลง bytes ตำแหน่ง 40-43 (seconds since 1900) เป็น Unix time
 *   6. ปรับ Time zone
 *   7. ถ้าไม่มี response ให้คืนค่า 0 (timeNotSet)
 * ============================================================================
 */
time_t getNtpTime() {
  IPAddress ntpServerIP;  // IP ของ NTP server ที่ได้จาก DNS

  // ทิ้ง packet UDP เก่าที่ค้างอยู่ทั้งหมด
  while (Udp.parsePacket() > 0)
    ;

  Serial.println("Transmit NTP Request");

  // แปลงชื่อโดเมน th.pool.ntp.org เป็น IP address
  WiFi.hostByName(ntpServerName, ntpServerIP);
  Serial.print(ntpServerName);
  Serial.print(": ");
  Serial.println(ntpServerIP);

  // ส่ง NTP request
  sendNTPpacket(ntpServerIP);

  // รอ response นาน 1.5 วินาที
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);

      // แปลง 4 bytes ที่ตำแหน่ง 40-43 เป็นจำนวนวินาทีนับจาก 1 ม.ค. 1900
      unsigned long secsSince1900;
      secsSince1900  = (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];

      // ลบออฟเซ็ต 1900→1970 และบวก time zone
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }

  // ไม่ได้รับ response
  Serial.println("No NTP Response :-(");
  return 0;
}

/*
 * ============================================================================
 * sendNTPpacket(IPAddress &address) — สร้างและส่ง NTP request packet
 * โครงสร้าง packet ตามมาตรฐาน NTP (RFC 5905)
 * ============================================================================
 */
void sendNTPpacket(IPAddress &address) {
  // เคลียร์ buffer ให้เป็น 0 ทั้งหมด
  memset(packetBuffer, 0, NTP_PACKET_SIZE);

  // กำหนดค่า NTP header
  packetBuffer[0] = 0b11100011;  // LI=0, Version=3, Mode=3 (Client)
  packetBuffer[1] = 0;           // Stratum
  packetBuffer[2] = 6;           // Polling Interval
  packetBuffer[3] = 0xEC;        // Peer Clock Precision

  // Root Delay & Root Dispersion = 0 (8 bytes)
  // Reference Identifier ใช้ค่า "NTP" (0x4E545050) หรือ bytes 49, 0x4E, 49, 52
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;

  // ส่ง packet ไปยัง NTP server port 123
  Udp.beginPacket(address, 123);
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

/*
 * ============================================================================
 * Web Server Handlers
 * ============================================================================
 */

/*
 * handleRoot() — ส่งหน้าเว็บนาฬิกาตัวใหญ่กลับไปยัง browser
 */
void handleRoot() {
  webServer.send(200, "text/html", getClockPage());
}

/*
 * handleTime() — API /time ส่งเวลาปัจจุบันเป็น JSON
 * ตัวอย่าง: {"hour":8,"minute":5,"second":32,"day":4,"month":10,"year":2026}
 */
void handleTime() {
  webServer.send(200, "application/json", getTimeJson());
}

/*
 * handleNtfyConfig() — ส่งหน้าเว็บตั้งค่า NTFY Notification
 */
void handleNtfyConfig() {
  webServer.send(200, "text/html", getConfigPage());
}

/*
 * handleNtfySave() — รับค่าจากฟอร์มและบันทึกลง EEPROM
 */
void handleNtfySave() {
  String server   = webServer.arg("server");
  String topic    = webServer.arg("topic");
  String interval = webServer.arg("interval");
  String enabled  = webServer.arg("enabled");

  if (server.length() > 0) {
    memset(ntfyConfig.server, 0, sizeof(ntfyConfig.server));
    server.toCharArray(ntfyConfig.server, sizeof(ntfyConfig.server));
  }
  if (topic.length() > 0) {
    memset(ntfyConfig.topic, 0, sizeof(ntfyConfig.topic));
    topic.toCharArray(ntfyConfig.topic, sizeof(ntfyConfig.topic));
  }
  int hrs = interval.toInt();
  if (hrs < 1) hrs = 1;
  if (hrs > 24) hrs = 24;
  ntfyConfig.intervalHours = (uint8_t)hrs;
  ntfyConfig.enabled = (enabled == "on") ? 1 : 0;

  saveNtfyConfig();

  String msg = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  msg += "<style>body{background:#000;color:#0f0;font-family:sans-serif;text-align:center;padding-top:20vh;}";
  msg += "a{color:#0f0;font-size:5vw;}</style></head><body>";
  msg += "<h1>บันทึกเรียบร้อย</h1>";
  msg += "<p>Server: " + String(ntfyConfig.server) + "<br>Topic: " + String(ntfyConfig.topic) + "<br>";
  msg += "Interval: " + String(ntfyConfig.intervalHours) + " ชั่วโมง<br>Enabled: " + String(ntfyConfig.enabled ? "ON" : "OFF") + "</p>";
  msg += "<a href='/config'>กลับไปหน้าตั้งค่า</a> | <a href='/'>ดูนาฬิกา</a>";
  msg += "</body></html>";
  webServer.send(200, "text/html", msg);
}

/*
 * getClockPage() — สร้าง HTML/CSS/JS หน้านาฬิกาขนาดใหญ่
 * - แสดงเวลา HH:MM:SS ตัวใหญ่
 - แสดงวันที่ DD/MM/YYYY
 - ดึงเวลาจาก /time ทุก 1 วินาที
 * - รองรับ Responsive / Mobile
 * ============================================================================
 */
String getClockPage() {
  String page = F("<!DOCTYPE html>"
                  "<html lang='th'>"
                  "<head>"
                  "<meta charset='UTF-8'>"
                  "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                  "<title>WEMOS NTP Clock</title>"
                  "<style>"
                  "body{margin:0;padding:0;background:#000;color:#0f0;font-family:'Courier New',monospace;"
                  "display:flex;flex-direction:column;justify-content:center;align-items:center;height:100vh;overflow:hidden;}"
                  "#clock{font-size:18vw;font-weight:bold;text-shadow:0 0 20px #0f0,0 0 40px #0f0;white-space:nowrap;}"
                  "#date{font-size:6vw;color:#0c0;margin-top:2vh;}"
                  "#status{font-size:3vw;color:#333;margin-top:2vh;}"
                  "@media(max-width:600px){#clock{font-size:22vw;}#date{font-size:7vw;}#status{font-size:4vw;}}"
                  "</style>"
                  "</head>"
                  "<body>"
                  "<div id='clock'>--:--:--</div>"
                  "<div id='date'>--/--/----</div>"
                  "<div id='status'>WEMOS NTP Clock</div>"
                  "<script>"
                  "function pad(n){return n<10?'0'+n:n;}"
                  "function updateClock(){"
                  "fetch('/time').then(r=>r.json()).then(d=>{"
                  "document.getElementById('clock').innerText="
                  "pad(d.hour)+':'+pad(d.minute)+':'+pad(d.second);"
                  "document.getElementById('date').innerText="
                  "pad(d.day)+'/'+pad(d.month)+'/'+d.year;"
                  "}).catch(e=>{document.getElementById('status').innerText='Offline';});"
                  "}"
                  "updateClock();"
                  "setInterval(updateClock,1000);"
                  "</script>"
                  "</body>"
                  "</html>");
  return page;
}

/*
 * getTimeJson() — สร้าง JSON string จากเวลาปัจจุบัน
 */
String getTimeJson() {
  String json = "{\"hour\":";
  json += hour();
  json += ",\"minute\":";
  json += minute();
  json += ",\"second\":";
  json += second();
  json += ",\"day\":";
  json += day();
  json += ",\"month\":";
  json += month();
  json += ",\"year\":";
  json += year();
  json += "}";
  return json;
}

/*
 * ============================================================================
 * NTFY Notification Functions
 * ============================================================================
 */

/*
 * loadNtfyConfig() — โหลดค่าการตั้งค่า NTFY จาก EEPROM
 * ถ้า magic ไม่ตรง "NTFY" จะใช้ค่าเริ่มต้น (ปิด, interval=1)
 */
void loadNtfyConfig() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(EEPROM_ADDR_NTFY, ntfyConfig);

  if (strncmp(ntfyConfig.magic, "NTFY", 4) != 0) {
    // ไม่มีข้อมูลที่ถูกต้อง ใช้ค่าเริ่มต้น
    memset(&ntfyConfig, 0, sizeof(ntfyConfig));
    strcpy(ntfyConfig.magic, "NTFY");
    strcpy(ntfyConfig.server, "ntfy.sh");
    strcpy(ntfyConfig.topic, "wemos-clock");
    ntfyConfig.intervalHours = 1;
    ntfyConfig.enabled = 0;
    saveNtfyConfig();
  }

  EEPROM.end();
}

/*
 * saveNtfyConfig() — บันทึกค่า NTFY ลง EEPROM
 */
void saveNtfyConfig() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.put(EEPROM_ADDR_NTFY, ntfyConfig);
  EEPROM.commit();
  EEPROM.end();

  Serial.println("NTFY config saved");
  Serial.print("Server: ");
  Serial.println(ntfyConfig.server);
  Serial.print("Topic: ");
  Serial.println(ntfyConfig.topic);
  Serial.print("Interval: ");
  Serial.print(ntfyConfig.intervalHours);
  Serial.println(" hour(s)");
  Serial.print("Enabled: ");
  Serial.println(ntfyConfig.enabled ? "YES" : "NO");
}

/*
 * isNtfyDue() — ตรวจสอบว่าครบรอบการแจ้งเตือนหรือยัง
 * คืนค่า true เมื่อ hour() เปลี่ยนไปตาม intervalHours ที่ตั้งไว้
 */
bool isNtfyDue() {
  if (ntfyConfig.intervalHours == 0) return false;
  if (lastNotifyHour < 0) return true;  // ยังไม่เคยส่ง

  int diff = hour() - lastNotifyHour;
  if (diff < 0) diff += 24;  // ข้ามเที่ยงคืน

  return diff >= (int)ntfyConfig.intervalHours;
}

/*
 * sendNtfyNotification() — ส่งข้อความแจ้งเตือนไปยัง NTFY server
 * ใช้ HTTP PUT พร้อม header Title และ Priority
 */
void sendNtfyNotification() {
  if (WiFi.status() != WL_CONNECTED) return;

  String url = "http://";
  url += String(ntfyConfig.server);
  url += "/";
  url += String(ntfyConfig.topic);

  WiFiClient client;
  HTTPClient http;

  if (http.begin(client, url)) {
    http.addHeader("Content-Type", "text/plain");
    http.addHeader("Title", "WEMOS Clock Alert");
    http.addHeader("Priority", "default");

    char payload[128];
    snprintf(payload, sizeof(payload),
             "เวลา %02d:%02d:%02d วันที่ %02d/%02d/%d",
             hour(), minute(), second(), day(), month(), year());

    Serial.println("Sending NTFY notification...");
    int httpCode = http.PUT(payload);
    Serial.print("NTFY HTTP code: ");
    Serial.println(httpCode);

    if (httpCode > 0) {
      String response = http.getString();
      Serial.println(response);
    } else {
      Serial.print("NTFY send failed: ");
      Serial.println(http.errorToString(httpCode));
    }

    http.end();
  } else {
    Serial.println("Unable to connect to NTFY server");
  }
}

/*
 * getConfigPage() — สร้างหน้า HTML ตั้งค่า NTFY Notification
 * - กำหนด NTFY server, topic, interval 1-24 ชั่วโมง, เปิด/ปิด
 * ============================================================================
 */
String getConfigPage() {
  String checked = ntfyConfig.enabled ? "checked" : "";

  String page = F("<!DOCTYPE html>"
                  "<html lang='th'>"
                  "<head>"
                  "<meta charset='UTF-8'>"
                  "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                  "<title>NTFY Config</title>"
                  "<style>"
                  "body{margin:0;padding:4vw;background:#000;color:#0f0;font-family:sans-serif;}"
                  "h1{font-size:6vw;text-align:center;}"
                  "form{max-width:600px;margin:0 auto;font-size:4vw;}"
                  "label{display:block;margin-top:3vh;}"
                  "input[type=text],input[type=number],select{width:100%;padding:2vw;font-size:4vw;background:#111;color:#0f0;border:1px solid #0f0;box-sizing:border-box;}"
                  "input[type=checkbox]{width:6vw;height:6vw;}"
                  "button{margin-top:5vh;width:100%;padding:3vw;font-size:5vw;background:#0f0;color:#000;border:none;font-weight:bold;}"
                  "a{color:#0f0;display:block;text-align:center;margin-top:4vh;font-size:4vw;}"
                  "</style>"
                  "</head>"
                  "<body>"
                  "<h1>ตั้งค่า NTFY Notification</h1>"
                  "<form action='/save' method='POST'>");

  page += "<label>NTFY Server</label>"
          "<input type='text' name='server' value='" + String(ntfyConfig.server) + "' placeholder='ntfy.sh' required>";

  page += "<label>Topic</label>"
          "<input type='text' name='topic' value='" + String(ntfyConfig.topic) + "' placeholder='wemos-clock' required>";

  page += "<label>แจ้งเตือนทุก (1-24 ชั่วโมง)</label>"
          "<input type='number' name='interval' min='1' max='24' value='" + String(ntfyConfig.intervalHours) + "' required>";

  page += "<label><input type='checkbox' name='enabled' " + checked + "> เปิดใช้งาน NTFY</label>";

  page += F("<button type='submit'>บันทึก</button>"
            "</form>"
            "<a href='/'>กลับไปหน้านาฬิกา</a>"
            "</body>"
            "</html>");
  return page;
}

// ============================================================================
// End of WEMOS-NTC-Dotmatrix002.ino
// ============================================================================
