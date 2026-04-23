#include <Arduino.h>
#include <U8g2lib.h>
#include <SPI.h>
#include <SD.h>
#include <TinyGPS++.h>
#include <WiFi.h>
#include <WebServer.h>

#define VERSION       "V2.1.7"
#define SD_CS         32
#define BTN_OK        26
#define BTN_UP        33
#define BTN_DOWN      25
#define BTN_BACK      21
#define LCD_RST       0
#define DEBOUNCE_MS   180
#define AP_SSID       "ChronoFork_WiFi"
#define AP_PASS       "chrono2024"   // WPA2, 8 car. min
#define LOG_PERIOD_MS 5000

U8G2_ST75256_JLX256160_F_4W_SW_SPI u8g2(U8G2_R0, 18, 23, 5, 2, LCD_RST);

TinyGPSPlus gps;
HardwareSerial SerialGPS(2);
WebServer server(80);

enum Mode { RACE=0, GPS_VIEW=1, WIFI_MODE=2, CIRCUIT_SEL=3, CHRONO_VIEW=4, SCREEN_SET=5, MENU=100 };
Mode currentMode = MENU;

int menuSelection = 0, contrastValue = 175;
String selectedCircuit = "AUCUN", currentLogFile = "", currentDir = "/";
unsigned long lapStartTime = 0, lastLogTime = 0;

unsigned long lastBtnMs[4] = {0, 0, 0, 0}; // OK, UP, DOWN, BACK
bool webRoutesBound = false;

// Anti-rebond non-bloquant : retourne true une seule fois par appui.
bool btnEdge(uint8_t pin, uint8_t idx) {
  if (digitalRead(pin) == LOW && millis() - lastBtnMs[idx] > DEBOUNCE_MS) {
    lastBtnMs[idx] = millis();
    return true;
  }
  return false;
}

void pumpGps() {
  while (SerialGPS.available() > 0) gps.encode(SerialGPS.read());
}

void handleRoot() {
  String html = "<html><body><h1>ChronoFork Web</h1>";
  html += "<p>Repertoire: " + currentDir + "</p></body></html>";
  server.send(200, "text/html", html);
}

void bindWebRoutes() {
  if (webRoutesBound) return;
  server.on("/", handleRoot);
  webRoutesBound = true;
}

void startWifi() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  bindWebRoutes();
  server.begin();
}

void stopWifi() {
  server.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
}

void setup() {
  Serial.begin(115200);

  pinMode(LCD_RST, OUTPUT);
  digitalWrite(LCD_RST, HIGH);
  delay(100);
  digitalWrite(LCD_RST, LOW);
  delay(200);
  digitalWrite(LCD_RST, HIGH);

  u8g2.begin();
  u8g2.setContrast(contrastValue);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_9x15_tf);
  u8g2.drawStr(10, 40, "CHRONOFORK " VERSION);
  u8g2.drawStr(10, 70, "BOOT...");
  u8g2.sendBuffer();
  delay(800);

  SPI.begin(14, 27, 13, SD_CS);
  if (!SD.begin(SD_CS)) Serial.println("SD Error");

  SerialGPS.begin(38400, SERIAL_8N1, 17, 16);

  pinMode(BTN_OK, INPUT_PULLUP);
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP);
}

void drawMenu() {
  const char* options[] = {"COURSE", "GPS", "WIFI", "CIRCUITS", "CHRONOS", "ECRAN"};
  int first = (menuSelection >= 4) ? menuSelection - 3 : 0;
  for (int i = 0; i < 4; i++) {
    int idx = first + i;
    if (idx >= 6) break;
    int yPos = 45 + (i * 28);
    if (idx == menuSelection) { u8g2.drawBox(0, yPos - 18, 256, 26); u8g2.setDrawColor(0); }
    u8g2.setFont(u8g2_font_9x15_tf);
    u8g2.drawStr(30, yPos, options[idx]);
    u8g2.setDrawColor(1);
  }
}

String buildLogName() {
  // Nom unique même sans fix GPS (fallback millis).
  if (gps.date.isValid() && gps.time.isValid()) {
    return "LOG_" + String(gps.date.day()) + "_" + String(gps.time.value()) + ".csv";
  }
  return "LOG_" + String(millis()) + ".csv";
}

void doRaceLogging() {
  if (millis() - lastLogTime < LOG_PERIOD_MS) return;
  if (!gps.location.isValid()) return;
  if (currentLogFile.length() == 0) currentLogFile = buildLogName();
  File log = SD.open("/" + currentLogFile, FILE_APPEND);
  if (log) {
    log.printf("%.6f,%.6f,%.1f\n", gps.location.lat(), gps.location.lng(), gps.speed.kmph());
    log.close();
  }
  lastLogTime = millis();
}

void enterMode(Mode m) {
  currentMode = m;
  if (m == WIFI_MODE) startWifi();
  if (m == RACE) {
    currentLogFile = "";
    lapStartTime = millis();
    lastLogTime = 0;
  }
}

void leaveToMenu() {
  if (currentMode == WIFI_MODE) stopWifi();
  currentMode = MENU;
}

void loop() {
  if (currentMode != WIFI_MODE) pumpGps();
  if (currentMode == WIFI_MODE) server.handleClient();

  u8g2.clearBuffer();

  if (btnEdge(BTN_BACK, 3) && currentMode != MENU) {
    leaveToMenu();
  }

  if (currentMode == MENU) {
    drawMenu();
  } else if (currentMode == RACE) {
    u8g2.setFont(u8g2_font_9x15_tf);
    u8g2.drawStr(10, 40, "SESSION EN COURS");
    u8g2.setCursor(10, 80);  u8g2.printf("VIT: %.1f km/h", gps.speed.kmph());
    u8g2.setCursor(10, 110); u8g2.printf("SAT: %lu", (unsigned long)gps.satellites.value());
    u8g2.setCursor(10, 140); u8g2.printf("FIC: %s", currentLogFile.c_str());
    doRaceLogging();
  } else if (currentMode == GPS_VIEW) {
    u8g2.setFont(u8g2_font_9x15_tf);
    u8g2.drawStr(10, 30, "GPS INFO");
    u8g2.setCursor(10, 60);  u8g2.printf("LAT: %.6f", gps.location.lat());
    u8g2.setCursor(10, 85);  u8g2.printf("LNG: %.6f", gps.location.lng());
    u8g2.setCursor(10, 110); u8g2.printf("SAT: %lu", (unsigned long)gps.satellites.value());
    u8g2.setCursor(10, 135); u8g2.printf("HDOP: %.1f", gps.hdop.hdop());
  } else if (currentMode == WIFI_MODE) {
    u8g2.setFont(u8g2_font_9x15_tf);
    u8g2.drawStr(10, 40, "WIFI : " AP_SSID);
    u8g2.setCursor(10, 80);  u8g2.print("IP: " + WiFi.softAPIP().toString());
    u8g2.setCursor(10, 110); u8g2.print("PASS: " AP_PASS);
  } else if (currentMode == CIRCUIT_SEL) {
    u8g2.setFont(u8g2_font_9x15_tf);
    u8g2.drawStr(10, 40, "CIRCUITS");
    u8g2.drawStr(10, 70, "(non implemente)");
  } else if (currentMode == CHRONO_VIEW) {
    u8g2.setFont(u8g2_font_9x15_tf);
    u8g2.drawStr(10, 40, "CHRONOS");
    u8g2.drawStr(10, 70, "(non implemente)");
  } else if (currentMode == SCREEN_SET) {
    u8g2.setFont(u8g2_font_9x15_tf);
    u8g2.drawStr(10, 40, "ECRAN");
    u8g2.setCursor(10, 80); u8g2.printf("CONTRASTE: %d", contrastValue);
    u8g2.drawStr(10, 110, "UP/DOWN pour ajuster");
  }

  u8g2.sendBuffer();

  // Navigation
  if (btnEdge(BTN_UP, 1)) {
    if (currentMode == MENU) menuSelection = (menuSelection - 1 + 6) % 6;
    else if (currentMode == SCREEN_SET) {
      contrastValue = min(255, contrastValue + 5);
      u8g2.setContrast(contrastValue);
    }
  }
  if (btnEdge(BTN_DOWN, 2)) {
    if (currentMode == MENU) menuSelection = (menuSelection + 1) % 6;
    else if (currentMode == SCREEN_SET) {
      contrastValue = max(0, contrastValue - 5);
      u8g2.setContrast(contrastValue);
    }
  }
  if (btnEdge(BTN_OK, 0)) {
    if (currentMode == MENU) enterMode((Mode)menuSelection);
  }
}
