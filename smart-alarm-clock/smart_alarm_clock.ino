/*
 * Smart Alarm Clock — ESP32 firmware
 * ==================================
 * Features:
 *  - WiFi (captive portal on first boot) + NTP time sync, falls back to DS3231 RTC
 *  - 2.4" TFT (ILI9341, SPI): big clock, date, temp/humidity, alarm status
 *  - 5 alarms, settable on-device (buttons) or from your phone (web page)
 *  - Sunrise simulation: WS2812B strip fades up 10 min before the alarm
 *  - Buzzer melodies, snooze (9 min, any button), dismiss (hold MENU 2 s)
 *  - Screen brightness control, settings saved to flash (survive reboot)
 *
 * Required Arduino libraries (install via Library Manager):
 *  - WiFiManager by tzapu
 *  - RTClib by Adafruit
 *  - Adafruit GFX Library
 *  - Adafruit ILI9341
 *  - DHT sensor library by Adafruit
 *  - Adafruit NeoPixel
 *
 * Board: "ESP32 Dev Module"
 */

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <WiFiManager.h>
#include <RTClib.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <DHT.h>
#include <Adafruit_NeoPixel.h>
#include <esp_arduino_version.h>
#include <time.h>

// ----------------------------- Pins ---------------------------------------
#define TFT_CS    5
#define TFT_DC    16
#define TFT_RST   17
#define TFT_LED   4    // backlight (PWM brightness)
#define BUZZER    25
#define BTN_MENU  32
#define BTN_UP    33
#define BTN_DOWN  27
#define DHT_PIN   26
#define DHT_TYPE  DHT22
#define LED_PIN   13
#define LED_COUNT 8

// ----------------------------- Config -------------------------------------
// Change to your timezone (POSIX TZ string). Examples:
//  US Pacific: "PST8PDT,M3.2.0,M11.1.0"   US Eastern: "EST5EDT,M3.2.0,M11.1.0"
//  UK: "GMT0BST,M3.5.0/1,M10.5.0"          Central Europe: "CET-1CEST,M3.5.0,M10.5.0"
//  India: "IST-5:30"                       Japan: "JST-9"
const char* TZ_INFO = "PST8PDT,M3.2.0,M11.1.0";

const int   SNOOZE_MINUTES   = 9;
const int   SUNRISE_MINUTES  = 10;   // glow ramp before alarm
const int   NUM_ALARMS       = 5;
const char* AP_NAME          = "SmartClock-Setup";

// ----------------------------- Globals ------------------------------------
Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);
RTC_DS3231 rtc;
DHT dht(DHT_PIN, DHT_TYPE);
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
WebServer server(80);
Preferences prefs;

struct Alarm { uint8_t hour; uint8_t minute; bool enabled; };
Alarm alarms[NUM_ALARMS];

int  brightness = 200;              // 40..255 TFT backlight
bool ringing = false;
bool snoozing = false;
time_t snoozeUntil = 0;
time_t sunriseStart = 0;            // 0 = no sunrise in progress
time_t alarmFireTime = 0;

// UI state machine
enum UIState { SHOW, MENU, SET_CLOCK_H, SET_CLOCK_M, SET_ALRM_SEL,
               SET_ALRM_H, SET_ALRM_M, SET_ALRM_EN, SET_BRIGHT };
UIState ui = SHOW;
int menuIdx = 0, selAlarm = 0, tmpH = 0, tmpM = 0;
const char* MENU_ITEMS[] = {"Set clock", "Set alarm", "Brightness", "Exit"};
const int MENU_N = 4;

// Button helpers (active LOW, internal pull-ups)
bool btnRaw(int pin) { return digitalRead(pin) == LOW; }
struct Button { int pin; bool last = false; uint32_t downAt = 0; };
Button bMenu{BTN_MENU}, bUp{BTN_UP}, bDown{BTN_DOWN};

bool pressed(Button &b) {                       // edge detect w/ debounce
  bool now = btnRaw(b.pin);
  bool hit = false;
  uint32_t t = millis();
  if (now && !b.last && (t - b.downAt > 200)) { hit = true; b.downAt = t; }
  if (now && !b.last) b.downAt = t;
  b.last = now;
  return hit;
}
bool held2s(Button &b) { return btnRaw(b.pin) && (millis() - b.downAt > 2000); }

// ----------------------------- Sound --------------------------------------
struct Note { int freq; int ms; };
// Returns 0 = melody finished, 1 = MENU, 2 = UP, 3 = DOWN pressed
int playTones(const Note* seq, int n) {
  for (int i = 0; i < n; i++) {
    if (seq[i].freq > 0) tone(BUZZER, seq[i].freq);
    else noTone(BUZZER);
    uint32_t t0 = millis();
    while (millis() - t0 < (uint32_t)seq[i].ms) {
      if (pressed(bMenu)) { noTone(BUZZER); return 1; }
      if (pressed(bUp))   { noTone(BUZZER); return 2; }
      if (pressed(bDown)) { noTone(BUZZER); return 3; }
      delay(5);
    }
  }
  noTone(BUZZER);
  return 0;
}
// Cheerful wake-up melody
const Note MELODY[] = {
  {523,180},{659,180},{784,180},{1047,320},{784,180},{1047,420},{0,120},
  {880,180},{988,180},{1047,360},{0,200}
};
const int MELODY_N = sizeof(MELODY)/sizeof(MELODY[0]);
// Gentle chime (used as 2nd option / menu beep base)
const Note CHIME[] = {{784,250},{988,250},{1175,500},{0,150}};
const int CHIME_N = sizeof(CHIME)/sizeof(CHIME[0]);

// ----------------------------- Persistence --------------------------------
void loadSettings() {
  prefs.begin("clock", false);
  brightness = prefs.getInt("bright", 200);
  for (int i = 0; i < NUM_ALARMS; i++) {
    alarms[i].hour    = prefs.getUChar(("aH"+String(i)).c_str(), 7);
    alarms[i].minute  = prefs.getUChar(("aM"+String(i)).c_str(), 0);
    alarms[i].enabled = prefs.getBool(("aE"+String(i)).c_str(), i == 0);
  }
  prefs.end();
}
void saveSettings() {
  prefs.begin("clock", false);
  prefs.putInt("bright", brightness);
  for (int i = 0; i < NUM_ALARMS; i++) {
    prefs.putUChar(("aH"+String(i)).c_str(), alarms[i].hour);
    prefs.putUChar(("aM"+String(i)).c_str(), alarms[i].minute);
    prefs.putBool(("aE"+String(i)).c_str(), alarms[i].enabled);
  }
  prefs.end();
}

// ----------------------------- Display ------------------------------------
#define C_BG    ILI9341_BLACK
#define C_FG    ILI9341_WHITE
#define C_ACC   ILI9341_CYAN
#define C_DIM   0x8410
#define C_ALARM ILI9341_ORANGE

void setBrightness() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(TFT_LED, brightness);   // Arduino-ESP32 core 3.x: write by pin
#else
  ledcWrite(0, brightness);         // core 2.x: write by channel
#endif
}

void drawCentered(const String& s, int y, int size, uint16_t color) {
  tft.setTextSize(size);
  tft.setTextColor(color, C_BG);
  int16_t x1, y1; uint16_t w, h;
  tft.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  tft.setCursor((tft.width() - w) / 2, y);
  tft.print(s);
}

const char* DOW[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};

void drawClock(const tm& t) {
  tft.fillScreen(C_BG);
  // WiFi dot
  tft.fillCircle(308, 12, 5, WiFi.status() == WL_CONNECTED ? ILI9341_GREEN : ILI9341_RED);

  // Big time  HH:MM
  char buf[9];
  snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
  drawCentered(buf, 40, 8, C_FG);

  // Seconds bar under time
  int sw = map(t.tm_sec, 0, 59, 0, 200);
  tft.fillRect(60, 128, 200, 6, C_DIM);
  tft.fillRect(60, 128, sw, 6, C_ACC);

  // Date
  char dbuf[24];
  snprintf(dbuf, sizeof(dbuf), "%s %04d-%02d-%02d",
           DOW[t.tm_wday], t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
  drawCentered(dbuf, 150, 2, C_ACC);

  // Temp / humidity
  float temp = dht.readTemperature();
  float hum  = dht.readHumidity();
  String th;
  if (!isnan(temp) && !isnan(hum)) {
    char tbuf[24];
    snprintf(tbuf, sizeof(tbuf), "%.1f C   %.0f%%", temp, hum);
    th = tbuf;
  } else th = "--.- C   --%";
  drawCentered(th, 180, 2, C_DIM);

  // Alarms row
  String a = "";
  for (int i = 0; i < NUM_ALARMS; i++) {
    if (alarms[i].enabled) {
      char ab[8];
      snprintf(ab, sizeof(ab), "%02d:%02d ", alarms[i].hour, alarms[i].minute);
      a += ab;
    }
  }
  if (a.length() == 0) a = "no alarms";
  drawCentered(a, 210, 2, C_ALARM);

  if (ringing)      drawCentered("ALARM! press any btn: snooze", 232, 1, ILI9341_RED);
  else if (snoozing) drawCentered("snoozing...", 232, 1, C_DIM);
}

void drawMenu() {
  tft.fillScreen(C_BG);
  drawCentered("MENU", 20, 3, C_ACC);
  for (int i = 0; i < MENU_N; i++) {
    tft.setTextSize(2);
    tft.setTextColor(i == menuIdx ? ILI9341_YELLOW : C_FG, C_BG);
    tft.setCursor(60, 80 + i * 30);
    tft.print((i == menuIdx ? "> " : "  ") + String(MENU_ITEMS[i]));
  }
  tft.setTextSize(1); tft.setTextColor(C_DIM, C_BG);
  tft.setCursor(40, 220); tft.print("UP/DOWN move - MENU select");
}

void drawSetClock() {
  tft.fillScreen(C_BG);
  drawCentered("SET CLOCK", 20, 3, C_ACC);
  char buf[9]; snprintf(buf, sizeof(buf), "%02d:%02d", tmpH, tmpM);
  drawCentered(buf, 90, 7, C_FG);
  tft.setTextSize(2);
  if (ui == SET_CLOCK_H) { tft.setTextColor(ILI9341_YELLOW, C_BG); tft.setCursor(88, 170); }
  else                   { tft.setTextColor(ILI9341_YELLOW, C_BG); tft.setCursor(180, 170); }
  tft.print("^");
  tft.setTextSize(1); tft.setTextColor(C_DIM, C_BG);
  tft.setCursor(30, 220); tft.print("UP/DOWN change - MENU next - hold MENU exit");
}

void drawSetAlarm() {
  tft.fillScreen(C_BG);
  drawCentered("SET ALARM", 20, 3, C_ACC);
  if (ui == SET_ALRM_SEL) {
    for (int i = 0; i < NUM_ALARMS; i++) {
      tft.setTextSize(2);
      tft.setTextColor(i == selAlarm ? ILI9341_YELLOW : C_FG, C_BG);
      tft.setCursor(70, 70 + i * 26);
      char ab[16];
      snprintf(ab, sizeof(ab), "%d: %02d:%02d %s", i + 1,
               alarms[i].hour, alarms[i].minute, alarms[i].enabled ? "ON" : "off");
      tft.print((i == selAlarm ? "> " : "  ") + String(ab));
    }
  } else {
    char buf[32];
    snprintf(buf, sizeof(buf), "Alarm %d", selAlarm + 1);
    drawCentered(buf, 60, 2, C_DIM);
    if (ui == SET_ALRM_EN) {
      drawCentered(alarms[selAlarm].enabled ? "ON" : "OFF", 110, 6,
                   alarms[selAlarm].enabled ? ILI9341_GREEN : ILI9341_RED);
    } else {
      snprintf(buf, sizeof(buf), "%02d:%02d", tmpH, tmpM);
      drawCentered(buf, 100, 7, C_FG);
      tft.setTextSize(2); tft.setTextColor(ILI9341_YELLOW, C_BG);
      tft.setCursor(ui == SET_ALRM_H ? 88 : 180, 180); tft.print("^");
    }
  }
  tft.setTextSize(1); tft.setTextColor(C_DIM, C_BG);
  tft.setCursor(30, 220); tft.print("UP/DOWN change - MENU next - hold MENU exit");
}

void drawBright() {
  tft.fillScreen(C_BG);
  drawCentered("BRIGHTNESS", 20, 3, C_ACC);
  char buf[8]; snprintf(buf, sizeof(buf), "%d%%", map(brightness, 40, 255, 0, 100));
  drawCentered(buf, 100, 6, C_FG);
  tft.fillRect(60, 180, 200, 10, C_DIM);
  tft.fillRect(60, 180, map(brightness, 40, 255, 0, 200), 10, ILI9341_YELLOW);
}

// ----------------------------- Sunrise LEDs -------------------------------
void sunriseTick(time_t now) {
  if (sunriseStart == 0) { strip.clear(); strip.show(); return; }
  float p = (float)(now - sunriseStart) / (SUNRISE_MINUTES * 60);
  if (p < 0) p = 0; if (p > 1) p = 1;
  // deep orange -> warm white as p goes 0..1
  uint8_t r = 255;
  uint8_t g = (uint8_t)(60 + 150 * p);
  uint8_t b = (uint8_t)(10 + 170 * p * p);
  uint8_t v = (uint8_t)(20 + 235 * p);
  for (int i = 0; i < LED_COUNT; i++)
    strip.setPixelColor(i, strip.Color(r * v / 255, g * v / 255, b * v / 255));
  strip.show();
}

// ----------------------------- Web config page ----------------------------
String alarmForm() {
  String h = "<html><head><meta name=viewport content='width=device-width,initial-scale=1'>"
    "<style>body{font-family:sans-serif;max-width:420px;margin:2em auto;padding:0 1em}"
    "input{font-size:1.2em;margin:.3em}</style></head><body>"
    "<h2>Smart Alarm Clock</h2><form action=/save method=GET>";
  for (int i = 0; i < NUM_ALARMS; i++) {
    char t[8]; snprintf(t, sizeof(t), "%02d:%02d", alarms[i].hour, alarms[i].minute);
    h += "Alarm " + String(i + 1) + ": <input type=time name=a" + String(i) +
         " value=" + t + "> <input type=checkbox name=e" + String(i) +
         (alarms[i].enabled ? " checked" : "") + "> on<br>";
  }
  h += "<br><input type=submit value='Save alarms'></form>"
       "<p>Clock IP: " + WiFi.localIP().toString() + "</p></body></html>";
  return h;
}

void handleRoot() { server.send(200, "text/html", alarmForm()); }

void handleSave() {
  for (int i = 0; i < NUM_ALARMS; i++) {
    if (server.hasArg("a" + String(i))) {
      String v = server.arg("a" + String(i));   // "HH:MM"
      alarms[i].hour   = v.substring(0, 2).toInt();
      alarms[i].minute = v.substring(3, 5).toInt();
    }
    alarms[i].enabled = server.hasArg("e" + String(i));
  }
  saveSettings();
  server.send(200, "text/html",
    "<html><body><h2>Saved!</h2><a href=/>Back</a></body></html>");
}

// ----------------------------- Alarm engine -------------------------------
bool timeMatches(const tm& t, const Alarm& a) {
  return t.tm_hour == a.hour && t.tm_min == a.minute;
}

void checkAlarms(const tm& t, time_t now) {
  // Start sunrise ramp SUNRISE_MINUTES before an enabled alarm
  bool upcoming = false;
  if (!ringing && !snoozing) {
    for (int i = 0; i < NUM_ALARMS; i++) {
      if (!alarms[i].enabled) continue;
      tm ft = t; ft.tm_hour = alarms[i].hour; ft.tm_min = alarms[i].minute; ft.tm_sec = 0;
      time_t f = mktime(&ft);
      if (f > now && f - now <= SUNRISE_MINUTES * 60) {
        upcoming = true;
        if (sunriseStart == 0) sunriseStart = f - SUNRISE_MINUTES * 60;
      }
    }
    if (!upcoming) sunriseStart = 0;   // alarm disabled/passed: kill the glow
  }
  // Snooze expired -> ring again
  if (snoozing && now >= snoozeUntil) { snoozing = false; ringing = true; alarmFireTime = now; }
  // Alarm time hit
  if (!ringing && !snoozing) {
    for (int i = 0; i < NUM_ALARMS; i++) {
      if (alarms[i].enabled && timeMatches(t, alarms[i]) && t.tm_sec < 5) {
        ringing = true; alarmFireTime = now; sunriseStart = 0;
        break;
      }
    }
  }
}

void handleRinging() {
  strip.fill(strip.Color(255, 255, 255)); strip.show();  // full bright while ringing
  time_t n = time(nullptr);
  drawClock(*localtime(&n));
  int b = playTones(MELODY, MELODY_N);   // 0 = finished, 1/2/3 = button pressed
  if (b == 1) {
    // MENU: short press = snooze, 2 s hold = dismiss
    uint32_t t0 = millis();
    while (btnRaw(BTN_MENU) && millis() - t0 < 2000) delay(20);
    b = btnRaw(BTN_MENU) ? 10 : 1;   // 10 = dismiss
  }
  if (b == 10) {
    ringing = false; snoozing = false; sunriseStart = 0;
    strip.clear(); strip.show(); noTone(BUZZER); ui = SHOW;
  } else if (b > 0) {
    // snooze: any short press
    ringing = false; snoozing = true;
    snoozeUntil = time(nullptr) + SNOOZE_MINUTES * 60;
    sunriseStart = 0; strip.clear(); strip.show(); noTone(BUZZER); ui = SHOW;
  }
  // b == 0: melody finished untouched -> keep ringing (loop repeats)
}

// ----------------------------- Setup / loop -------------------------------
void applyClockToRtc(const tm& t) {
  rtc.adjust(DateTime(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                      t.tm_hour, t.tm_min, t.tm_sec));
}

void setup() {
  Serial.begin(115200);
  pinMode(BTN_MENU, INPUT_PULLUP);
  pinMode(BTN_UP,   INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BUZZER, OUTPUT);

  tft.begin();
  tft.setRotation(1);               // landscape 320x240
  tft.fillScreen(C_BG);
  drawCentered("SmartClock", 100, 4, C_ACC);

#if ESP_ARDUINO_VERSION_MAJOR < 3
  ledcSetup(0, 5000, 8);
  ledcAttachPin(TFT_LED, 0);
#else
  ledcAttach(TFT_LED, 5000, 8);
#endif

  dht.begin();
  strip.begin(); strip.clear(); strip.show();
  loadSettings();
  setBrightness();

  if (!rtc.begin()) { drawCentered("RTC missing!", 140, 2, ILI9341_RED); delay(2000); }
  if (rtc.lostPower()) rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  // WiFi with captive portal on first boot
  drawCentered("WiFi...", 140, 2, C_DIM);
  WiFiManager wm;
  wm.setConfigPortalTimeout(120);
  if (wm.autoConnect(AP_NAME)) {
    configTzTime(TZ_INFO, "pool.ntp.org", "time.nist.gov");
    // wait up to 10 s for NTP
    for (int i = 0; i < 20; i++) {
      time_t n = time(nullptr); tm* t = localtime(&n);
      if (t->tm_year > 120) { applyClockToRtc(*t); break; }
      delay(500);
    }
  }
  // If no WiFi, seed system time from RTC so everything still works
  { DateTime d = rtc.now();
    tm t{}; t.tm_year = d.year()-1900; t.tm_mon = d.month()-1; t.tm_mday = d.day();
    t.tm_hour = d.hour(); t.tm_min = d.minute(); t.tm_sec = d.second();
    time_t epoch = mktime(&t); timeval tv{epoch, 0}; settimeofday(&tv, nullptr); }

  server.on("/", handleRoot);
  server.on("/save", handleSave);
  server.begin();
  ui = SHOW;
}

void handleMenu() {
  if (ui == SHOW) {
    if (pressed(bMenu)) { ui = MENU; menuIdx = 0; drawMenu(); }
    return;
  }
  if (held2s(bMenu)) { ui = SHOW; saveSettings(); return; }   // exit anywhere

  switch (ui) {
    case MENU:
      if (pressed(bUp))   { menuIdx = (menuIdx + MENU_N - 1) % MENU_N; drawMenu(); }
      if (pressed(bDown)) { menuIdx = (menuIdx + 1) % MENU_N; drawMenu(); }
      if (pressed(bMenu)) {
        time_t n = time(nullptr); tm* t = localtime(&n);
        if (menuIdx == 0)      { ui = SET_CLOCK_H; tmpH = t->tm_hour; tmpM = t->tm_min; drawSetClock(); }
        else if (menuIdx == 1) { ui = SET_ALRM_SEL; selAlarm = 0; drawSetAlarm(); }
        else if (menuIdx == 2) { ui = SET_BRIGHT; drawBright(); }
        else                   { ui = SHOW; saveSettings(); }
      }
      break;
    case SET_CLOCK_H:
      if (pressed(bUp))   { tmpH = (tmpH + 1) % 24; drawSetClock(); }
      if (pressed(bDown)) { tmpH = (tmpH + 23) % 24; drawSetClock(); }
      if (pressed(bMenu)) { ui = SET_CLOCK_M; drawSetClock(); }
      break;
    case SET_CLOCK_M:
      if (pressed(bUp))   { tmpM = (tmpM + 1) % 60; drawSetClock(); }
      if (pressed(bDown)) { tmpM = (tmpM + 59) % 60; drawSetClock(); }
      if (pressed(bMenu)) {
        time_t n = time(nullptr); tm t = *localtime(&n);
        t.tm_hour = tmpH; t.tm_min = tmpM; t.tm_sec = 0;
        time_t e = mktime(&t); timeval tv{e, 0}; settimeofday(&tv, nullptr);
        applyClockToRtc(t); saveSettings(); ui = SHOW;
      }
      break;
    case SET_ALRM_SEL:
      if (pressed(bUp))   { selAlarm = (selAlarm + NUM_ALARMS - 1) % NUM_ALARMS; drawSetAlarm(); }
      if (pressed(bDown)) { selAlarm = (selAlarm + 1) % NUM_ALARMS; drawSetAlarm(); }
      if (pressed(bMenu)) { ui = SET_ALRM_H; tmpH = alarms[selAlarm].hour; tmpM = alarms[selAlarm].minute; drawSetAlarm(); }
      break;
    case SET_ALRM_H:
      if (pressed(bUp))   { tmpH = (tmpH + 1) % 24; drawSetAlarm(); }
      if (pressed(bDown)) { tmpH = (tmpH + 23) % 24; drawSetAlarm(); }
      if (pressed(bMenu)) { ui = SET_ALRM_M; drawSetAlarm(); }
      break;
    case SET_ALRM_M:
      if (pressed(bUp))   { tmpM = (tmpM + 1) % 60; drawSetAlarm(); }
      if (pressed(bDown)) { tmpM = (tmpM + 59) % 60; drawSetAlarm(); }
      if (pressed(bMenu)) {
        alarms[selAlarm].hour = tmpH; alarms[selAlarm].minute = tmpM;
        ui = SET_ALRM_EN; drawSetAlarm();
      }
      break;
    case SET_ALRM_EN:
      if (pressed(bUp) || pressed(bDown)) { alarms[selAlarm].enabled = !alarms[selAlarm].enabled; drawSetAlarm(); }
      if (pressed(bMenu)) { saveSettings(); ui = SHOW; }
      break;
    case SET_BRIGHT:
      if (pressed(bUp))   { brightness = min(255, brightness + 15); setBrightness(); drawBright(); }
      if (pressed(bDown)) { brightness = max(40,  brightness - 15); setBrightness(); drawBright(); }
      if (pressed(bMenu)) { saveSettings(); ui = SHOW; }
      break;
    default: break;
  }
}

void loop() {
  server.handleClient();
  time_t now = time(nullptr);
  tm t = *localtime(&now);

  if (ringing) { handleRinging(); return; }

  checkAlarms(t, now);
  sunriseTick(now);
  handleMenu();

  static int lastSec = -1;
  if (ui == SHOW && t.tm_sec != lastSec) { lastSec = t.tm_sec; drawClock(t); }
  delay(20);
}
