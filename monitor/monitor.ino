#include <WiFi.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <WiFiManager.h> 
#include "time.h"


// ====== IMPORT FILE ẢNH TỪ TAB KHÁC ======
#include "image.h"

// ====== Time config ======
const char* ntpServer = "time.google.com"; // Hoặc "asia.pool.ntp.org"
const long  gmtOffset_sec      = 7 * 3600;   // GMT+7
const int    daylightOffset_sec = 0;

// ====== Pins ======
#define TFT_CS     9
#define TFT_DC     8
#define TFT_RST    7
#define TFT_MOSI   6
#define TFT_SCLK   5

#define ENC_A_PIN    10
#define ENC_B_PIN    20
#define ENC_BTN_PIN  21
#define KEY0_PIN     0

#define LED_PIN 4
#define BUZZ_PIN 3

// ====== TFT ======
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST); 

// ====== Colors ======
#define CYBER_BG      ST77XX_BLACK
#define CYBER_GREEN   0x07E0  
#define CYBER_ACCENT  tft.color565(163, 255, 206)
#define CYBER_LIGHT   0xFD20  
#define CYBER_BLUE    0x07FF  
#define CYBER_PINK    tft.color565(255, 163, 212)
#define CYBER_DARK    0x4208

// =========================================================================
// [QUẢN LÝ ALBUM ẢNH]
// =========================================================================
#define TOTAL_PICS 17  
const uint16_t* album_data[TOTAL_PICS] = {
  epd_bitmap_0,
  epd_bitmap_1,
  epd_bitmap_2,
  epd_bitmap_3,
  epd_bitmap_4,
  epd_bitmap_5,
  epd_bitmap_6,
  epd_bitmap_7,
  epd_bitmap_8,
  epd_bitmap_9,
  epd_bitmap_10,
  epd_bitmap_11,
  epd_bitmap_12,
  epd_bitmap_13,
  epd_bitmap_14,
  epd_bitmap_15,
  epd_bitmap_16
};

// ====== Modes ======
enum UIMode {
  MODE_MENU = 0,
  MODE_CLOCK,
  MODE_ALARM,
  MODE_ALBUM,
  MODE_LETTER // Thêm Mode mới cho bức thư
};
UIMode currentMode = MODE_CLOCK;      
int menuIndex = 0;
const int MENU_ITEMS = 4;  // Tăng lên 4 mục trong Menu

// ====== Album vars ======
int currentPicIndex = 0;

// ====== Letter vars ======
int currentLetterPage = 0;
const int TOTAL_LETTER_PAGES = 5;

// ====== Clock vars ======
int    prevSecond  = -1;
String prevTimeStr = "";
int    prevLoveDays = -1;              

// ====== Encoder & Buttons ======
int  lastEncA   = HIGH;
int  lastEncB   = HIGH;
bool lastEncBtn = HIGH;
bool lastKey0   = HIGH;
unsigned long lastBtnMs = 0;

// ====== Alarm ======
bool     alarmEnabled = false;
uint8_t  alarmHour    = 7;
uint8_t  alarmMinute  = 0;
bool     alarmRinging = false;
int      alarmSelectedField = 0; 
int      lastAlarmDayTriggered = -1;

// ====== Alert / LED Blink ======
unsigned long lastLedToggleMs = 0;
bool ledState = false;

// ========= Helper: encoder & button =========
int readEncoderStep() {
  int encA = digitalRead(ENC_A_PIN);
  int encB = digitalRead(ENC_B_PIN);
  int step = 0;
  if (encA != lastEncA) {
    if (encA == LOW) {
      if (encB == HIGH) step = +1;
      else              step = -1;
    }
  }
  lastEncA = encA;
  lastEncB = encB;
  return step;
}

bool checkButtonPressed(uint8_t pin, bool &lastState) {
  bool cur = digitalRead(pin);
  bool pressed = false;
  unsigned long now = millis();
  if (cur == LOW && lastState == HIGH && (now - lastBtnMs) > 150) {
    pressed = true;
    lastBtnMs = now;
  }
  lastState = cur;
  return pressed;
}

// ========= WiFiManager Portal Callback =========
void configModeCallback(WiFiManager *myWiFiManager) {
  tft.fillScreen(CYBER_BG);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_RED);
  tft.setCursor(10, 20);
  tft.println("WiFi Disconnected!");
  
  tft.setTextColor(CYBER_ACCENT);
  tft.setCursor(10, 45);
  tft.println("Connect Phone To WiFi:");
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(15, 60);
  tft.println("> Cyber_Clock_WiFi");
  
  tft.setTextColor(CYBER_LIGHT);
  tft.setCursor(10, 85);
  tft.println("Setup page will pop up");
  tft.setCursor(10, 100);
  tft.println("to enter new password!");
}

// ========= Connect WiFi & Sync Time =========
void connectWiFiAndSyncTime() {
  tft.fillScreen(CYBER_BG);
  tft.setTextColor(CYBER_LIGHT);
  tft.setTextSize(1);
  tft.setCursor(10, 55);
  tft.print("Checking WiFi...");

  WiFiManager wm;
  wm.setConfigPortalTimeout(180); 
  wm.setAPCallback(configModeCallback);

  if (!wm.autoConnect("Cyber_Clock_WiFi")) {
    tft.fillScreen(CYBER_BG);
    tft.setCursor(10, 55);
    tft.setTextColor(ST77XX_RED);
    tft.print("Timeout! Booting Clock...");
    delay(2000);
    return;
  }

  tft.fillScreen(CYBER_BG);
  tft.setCursor(10, 55);
  tft.setTextColor(CYBER_GREEN);
  tft.print("WiFi Connected!");
  delay(1000);

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  tft.fillScreen(CYBER_BG);
  tft.setCursor(10, 55);
  tft.setTextColor(ST77XX_WHITE);
  tft.print("Syncing time...");
  delay(800);
}

String getTimeStr(char type) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "--";
  char buf[8];
  if (type == 'H') strftime(buf, sizeof(buf), "%H", &timeinfo);
  else if (type == 'M') strftime(buf, sizeof(buf), "%M", &timeinfo);
  else if (type == 'S') strftime(buf, sizeof(buf), "%S", &timeinfo);
  return String(buf);
}

int calculateLoveDays() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return -1;

  struct tm start_time;
  start_time.tm_year = 2022 - 1900; 
  start_time.tm_mon  = 7 - 1;       
  start_time.tm_mday = 17;
  start_time.tm_hour = 0;
  start_time.tm_min  = 0;
  start_time.tm_sec  = 0;
  start_time.tm_isdst = -1;

  time_t start_t = mktime(&start_time);
  time_t current_t = mktime(&timeinfo);

  if (current_t < start_t) return 0;

  double diff_seconds = difftime(current_t, start_t);
  int days = diff_seconds / (24 * 3600);
  return days;
}

void drawAlarmIcon() {
  int x = 148;
  if (!alarmEnabled) {
    for (int r = 0; r < 12; r++) {
      for (int c = (x - 10); c < (x + 2); c++) {
        int flash_index = r * 160 + c; 
        uint16_t pixel_color = pgm_read_word(&album_data[0][flash_index]);
        tft.drawPixel(c, r, pixel_color); 
      }
    }
    return; 
  }
  uint16_t c = CYBER_LIGHT; 
  tft.drawRoundRect(x - 9, 2, 10, 7, 2, c);
  tft.drawFastHLine(x - 8, 8, 8, c);
  tft.fillCircle(x - 4, 10, 1, c);
}

// ========= Clock UI =========
void initClockStaticUI() {
  tft.drawRGBBitmap(0, 0, album_data[0], 160, 128);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(4, 4);
  tft.print("Nghia's embe");
  drawAlarmIcon();
}

void drawClockTime(String hourStr, String minStr, String secStr) {
  String cur = hourStr + ":" + minStr + ":" + secStr;
  int days = calculateLoveDays();

  if (cur == prevTimeStr && days == prevLoveDays) return;

  int box_w = 144;
  int box_h = 55; 
  int box_x = (160 - box_w) / 2;
  int box_y = 40;  

  if (prevTimeStr != "") {
    uint16_t* local_buffer = (uint16_t*) malloc(box_w * box_h * sizeof(uint16_t));
    if (local_buffer != NULL) {
      for (int r = 0; r < box_h; r++) {
        for (int c = 0; c < box_w; c++) {
          int flash_index = (box_y + r) * 160 + (box_x + c);
          local_buffer[r * box_w + c] = pgm_read_word(&album_data[0][flash_index]);
        }
      }
      tft.drawRGBBitmap(box_x, box_y, local_buffer, box_w, box_h);
      free(local_buffer);
    } else {
      tft.fillRect(box_x, box_y, box_w, box_h, CYBER_BG);
    }
  }

  prevTimeStr = cur;
  prevLoveDays = days;

  tft.setTextSize(3);
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(cur, 0, 0, &x1, &y1, &w, &h);
  int x_time = (160 - w) / 2;
  int y_time = box_y + 4; 
  
  tft.setTextColor(CYBER_ACCENT);
  tft.setCursor(x_time, y_time);
  tft.print(cur);

  String loveStr = (days >= 0) ? String(days) + " Days" : "--- Days";
  tft.setTextSize(2);
  tft.getTextBounds(loveStr, 0, 0, &x1, &y1, &w, &h);
  int x_love = (160 - w) / 2;
  int y_love = y_time + 32; 

  tft.setTextColor(CYBER_PINK);
  tft.setCursor(x_love, y_love);
  tft.print(loveStr);
}

// ========= Menu UI =========
void drawMenu() {
  tft.fillScreen(CYBER_BG);
  tft.setTextSize(1);
  tft.setTextColor(CYBER_LIGHT);
  tft.setCursor(10, 6);
  tft.print("MODE SELECT");

  const char* items[MENU_ITEMS] = {
    "Monitor Clock",
    "Alarm Setup",
    "Photo Album",
    "Ck beo's letter" // Mục thứ 4 vừa thêm
  };

  for (int i = 0; i < MENU_ITEMS; i++) {
    int y = 22 + i * 16; // Thu hẹp khoảng cách một chút để fit 4 mục
    if (i == menuIndex) {
      tft.fillRect(6, y - 2, 148, 13, CYBER_ACCENT);
      tft.setTextColor(CYBER_BG);
    } else {
      tft.fillRect(6, y - 2, 148, 13, CYBER_BG);
      tft.setTextColor(ST77XX_WHITE);
    }
    tft.setCursor(12, y);
    tft.print(items[i]);
  }
}

// ========= Alarm UI =========
void drawAlarmScreen(bool full) {
  if (full) {
    tft.fillScreen(CYBER_BG);
    tft.setTextSize(1);
    tft.setCursor(8, 4);
    tft.setTextColor(CYBER_LIGHT);
    tft.print("ALARM");
  }

  char hBuf[3];
  char mBuf[3];
  sprintf(hBuf, "%02d", alarmHour);
  sprintf(mBuf, "%02d", alarmMinute);

  tft.setTextSize(3);
  String disp = String(hBuf) + ":" + String(mBuf);
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(disp, 0, 0, &x1, &y1, &w, &h);
  int x = (160 - w) / 2;
  int y = 30;

  tft.setCursor(x, y);
  tft.setTextColor(alarmSelectedField == 0 ? CYBER_LIGHT : ST77XX_WHITE, CYBER_BG);
  tft.print(hBuf);
  tft.setTextColor(ST77XX_WHITE, CYBER_BG);
  tft.print(":");
  tft.setTextColor(alarmSelectedField == 1 ? CYBER_LIGHT : ST77XX_WHITE, CYBER_BG);
  tft.print(mBuf);

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE, CYBER_BG);
  tft.fillRect(20, 80, 120, 24, CYBER_BG);
  tft.setCursor(30, 84);
  tft.print("Alarm:");
  tft.setCursor(80, 84);
  uint16_t enColor = alarmSelectedField == 2 ? CYBER_LIGHT : (alarmEnabled ? CYBER_GREEN : ST77XX_RED);
  tft.setTextColor(enColor, CYBER_BG);
  tft.print(alarmEnabled ? "ON " : "OFF");
  drawAlarmIcon();
}

void drawAlarmRingingScreen() {
  tft.fillScreen(ST77XX_RED);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE, ST77XX_RED);
  tft.setCursor(30, 40);
  tft.print("ALARM!");
}

// ========= Album UI =========
void drawAlbumScreen() {
  tft.drawRGBBitmap(0, 0, album_data[currentPicIndex], 160, 128);
  
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(130, 116);
  tft.printf("%d/%d", currentPicIndex + 1, TOTAL_PICS);
}

// ========= Letter UI (Bản nguyên văn đầy đủ 100% - 5 trang) =========
void drawLetterScreen() {
  tft.fillScreen(CYBER_BG);
  
  // Vẽ khung viền giả lập lá thư
  tft.drawRect(2, 2, 156, 124, CYBER_PINK);
  tft.drawRect(4, 4, 152, 120, CYBER_DARK);

  tft.setTextSize(1);
  tft.setTextColor(CYBER_ACCENT);
  tft.setCursor(10, 8);
  tft.print("From Ck Beo <3");

  tft.setTextColor(ST77XX_WHITE);
  
  if (currentLetterPage == 0) {
    // TRANG 1
    tft.setCursor(10, 24);  tft.println("Happy anniversary!");
    tft.setCursor(10, 36);  tft.println("Vay la chung ta da");
    tft.setCursor(10, 48);  tft.println("yeu nhau duoc 4 nam.");
    tft.setCursor(10, 64);  tft.println("Cam on em suot thoi");
    tft.setCursor(10, 76);  tft.println("gian qua da ben canh");
    tft.setCursor(10, 88);  tft.println("anh, day la khoang");
    tft.setCursor(10, 100); tft.println("thoi gian dep nhat...");
  } 
  else if (currentLetterPage == 1) {
    // TRANG 2
    tft.setCursor(10, 24);  tft.println("...cuoc doi anh cho");
    tft.setCursor(10, 36);  tft.println("den nay, o ben canh");
    tft.setCursor(10, 48);  tft.println("em anh thay binh yen.");
    tft.setCursor(10, 64);  tft.println("Du cho anh co mac");
    tft.setCursor(10, 76);  tft.println("phai nhung loi lam");
    tft.setCursor(10, 88);  tft.println("nghiem trong nhung");
    tft.setCursor(10, 100); tft.println("van nhan duoc su...");
  }
  else if (currentLetterPage == 2) {
    // TRANG 3
    tft.setCursor(10, 24);  tft.println("...tha thu tu em.");
    tft.setCursor(10, 36);  tft.println("Anh muon minh song");
    tft.setCursor(10, 48);  tft.println("ma khong ngung hoi");
    tft.setCursor(10, 60);  tft.println("loi, anh hua se ban");
    tft.setCursor(10, 72);  tft.println("linh hon de em khong");
    tft.setCursor(10, 84);  tft.println("phai roi nuoc mat");
    tft.setCursor(10, 96);  tft.println("lan nao nua.");
  }
  else if (currentLetterPage == 3) {
    // TRANG 4
    tft.setCursor(10, 24);  tft.println("Nhung loi anh viet co");
    tft.setCursor(10, 36);  tft.println("the khong hay, nhung");
    tft.setCursor(10, 48);  tft.println("anh mong no co the");
    tft.setCursor(10, 60);  tft.println("cham toi em. Mon qua");
    tft.setCursor(10, 72);  tft.println("nay co the khong");
    tft.setCursor(10, 84);  tft.println("hoan hao, nhung anh");
    tft.setCursor(10, 96);  tft.println("luon co gang het suc...");
  }
  else if (currentLetterPage == 4) {
    // TRANG 5
    tft.setCursor(10, 24);  tft.println("...de khien no hoan");
    tft.setCursor(10, 36);  tft.println("hao. Anh chuc 2 dua");
    tft.setCursor(10, 48);  tft.println("minh se mai mai ben");
    tft.setCursor(10, 60);  tft.println("nhau, chuc em luon");
    tft.setCursor(10, 72);  tft.println("duoc anh yeu thuong.");
    
    // Phần ký tên kết thúc rực rỡ với màu hồng
    tft.setCursor(10, 95);  tft.setTextColor(CYBER_PINK);
    tft.setTextSize(2);
    tft.println("Anh yeu em.");
  }

  // Hiển thị số trang ở góc dưới bên phải lá thư (Luôn để TextSize là 1)
  tft.setTextSize(1);
  tft.setTextColor(CYBER_LIGHT);
  tft.setCursor(130, 114);
  tft.printf("%d/%d", currentLetterPage + 1, TOTAL_LETTER_PAGES);
}

void checkAlarmTrigger() {
  if (!alarmEnabled || alarmRinging) return;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;

  if (timeinfo.tm_hour == alarmHour && timeinfo.tm_min == alarmMinute && timeinfo.tm_sec == 0) {
    alarmRinging = true;
    lastAlarmDayTriggered = timeinfo.tm_mday;
    currentMode = MODE_ALARM;
    drawAlarmRingingScreen();
  }
}

void updateAlertStateAndLED() {
  unsigned long now = millis();
  unsigned long interval = alarmRinging ? 120 : 1000;

  if (now - lastLedToggleMs > interval) {
    lastLedToggleMs = now;
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState ? HIGH : LOW);
  }
}

// ========= SETUP =========
void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(ENC_A_PIN,   INPUT_PULLUP);
  pinMode(ENC_B_PIN,   INPUT_PULLUP);
  pinMode(ENC_BTN_PIN, INPUT_PULLUP);
  pinMode(KEY0_PIN,    INPUT_PULLUP);

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZ_PIN, OUTPUT);

  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);

  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(CYBER_BG);

  connectWiFiAndSyncTime();

  initClockStaticUI();
  prevTimeStr = "";
  prevLoveDays = -1;
  drawClockTime(getTimeStr('H'), getTimeStr('M'), getTimeStr('S'));
}

// ========= LOOP =========
void loop() {
  static unsigned long lastWifiCheck = 0;
  if (millis() - lastWifiCheck > 30000) {
    lastWifiCheck = millis();
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.begin(); 
    }
  }

  int encStep     = readEncoderStep();
  bool encPressed = checkButtonPressed(ENC_BTN_PIN, lastEncBtn);
  bool k0Pressed  = checkButtonPressed(KEY0_PIN,    lastKey0);

  checkAlarmTrigger();
  updateAlertStateAndLED();

  switch (currentMode) {
    case MODE_MENU: {
      if (encStep != 0) {
        menuIndex += encStep;
        if (menuIndex < 0) menuIndex = MENU_ITEMS - 1;
        if (menuIndex >= MENU_ITEMS) menuIndex = 0;
        drawMenu();
      }
      if (encPressed) {
        if (menuIndex == 0) {
          currentMode = MODE_CLOCK;
          initClockStaticUI();
          prevTimeStr = "";
          prevLoveDays = -1;
          drawClockTime(getTimeStr('H'), getTimeStr('M'), getTimeStr('S'));
        } else if (menuIndex == 1) { 
          currentMode = MODE_ALARM;
          alarmSelectedField = 0;
          drawAlarmScreen(true);
        } else if (menuIndex == 2) { 
          currentMode = MODE_ALBUM;
          currentPicIndex = 0;
          drawAlbumScreen();
        } else if (menuIndex == 3) { // Vào chế độ xem Thư
          currentMode = MODE_LETTER;
          currentLetterPage = 0;
          drawLetterScreen();
        }
      }
      break;
    }

    case MODE_CLOCK: {
      struct tm timeinfo;
      if (getLocalTime(&timeinfo)) {
        int sec = timeinfo.tm_sec;
        if (sec != prevSecond) {
          prevSecond = sec;
          drawClockTime(getTimeStr('H'), getTimeStr('M'), getTimeStr('S'));
        }
      }
      if (k0Pressed) {
        currentMode = MODE_MENU;
        drawMenu();
      }
      break;
    }

    case MODE_ALARM: {
      if (alarmRinging) {
        static unsigned long lastBeep = 0;
        if (millis() - lastBeep > 1000) {
          lastBeep = millis();
          tone(BUZZ_PIN, 2000, 400);
        }
        if (encPressed || k0Pressed) {
          alarmRinging = false;
          lastAlarmDayTriggered = -1;
          noTone(BUZZ_PIN);
          drawAlarmScreen(true);
        }
        break;
      }

      bool changed = false;
      if (encStep != 0) {
        if (alarmSelectedField == 0) {
          if (encStep > 0) alarmHour = (alarmHour + 1) % 24;
          else                 alarmHour = (alarmHour + 23) % 24;
          changed = true;
        } else if (alarmSelectedField == 1) {
          if (encStep > 0) alarmMinute = (alarmMinute + 1) % 60;
          else                 alarmMinute = (alarmMinute + 59) % 60;
          changed = true;
        } else if (alarmSelectedField == 2) {
          alarmEnabled = !alarmEnabled;
          changed = true;
        }
        if (changed) { lastAlarmDayTriggered = -1; }
      }

      if (encPressed) {
        alarmSelectedField = (alarmSelectedField + 1) % 3;
        changed = true;
      }
      if (k0Pressed) {
        currentMode = MODE_MENU;
        drawMenu();
        break;
      }
      if (changed) {
        drawAlarmScreen(false);
      }
      break;
    }

    case MODE_ALBUM: { 
      if (encStep != 0) {
        currentPicIndex += encStep;
        if (currentPicIndex < 0) currentPicIndex = TOTAL_PICS - 1;
        if (currentPicIndex >= TOTAL_PICS) currentPicIndex = 0;
        drawAlbumScreen(); 
      }
      
      if (k0Pressed || encPressed) { 
        currentMode = MODE_MENU;
        drawMenu();
      }
      break;
    }

    case MODE_LETTER: { // Logic tương tác bên trong bức thư
      if (encStep != 0) {
        currentLetterPage += encStep;
        if (currentLetterPage < 0) currentLetterPage = TOTAL_LETTER_PAGES - 1;
        if (currentLetterPage >= TOTAL_LETTER_PAGES) currentLetterPage = 0;
        drawLetterScreen();
      }
      
      // Ấn nút Encoder hoặc nút K0 để thoát ra Menu chính
      if (k0Pressed || encPressed) {
        currentMode = MODE_MENU;
        drawMenu();
      }
      break;
    }
  }
}