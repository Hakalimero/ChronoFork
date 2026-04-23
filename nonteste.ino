#include <Arduino.h>
#include <U8g2lib.h>
#include <SPI.h>
#include <SD.h>
#include <TinyGPS++.h>
#include <WiFi.h>
#include <WebServer.h>

#define VERSION       "V2.1.5"
#define SD_CS         32
#define BTN_OK        26
#define BTN_UP        33
#define BTN_DOWN      25
#define BTN_BACK      21
#define LCD_RST       4   // Variante Pin 4 (vs beta.ino en Pin 0)
#define DEBOUNCE_MS   180
#define AP_SSID       "ChronoFork_WiFi"
#define AP_PASS       "chrono2024"
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
File fsUploadFile;

unsigned long lastBtnMs[4] = {0, 0, 0, 0}; // OK, UP, DOWN, BACK
bool webRoutesBound = false;

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

// Refuse les chemins suspects (path traversal, chemins relatifs).
bool safePath(const String& p) {
  if (p.length() == 0 || p[0] != '/') return false;
  if (p.indexOf("..") >= 0) return false;
  return true;
}

String joinPath(const String& dir, const String& name) {
  // L'API SD ESP32 renvoie parfois un chemin complet, parfois juste le nom.
  if (name.startsWith("/")) return name;
  if (dir.endsWith("/"))    return dir + name;
  return dir + "/" + name;
}

void handleRoot() {
  String reqDir = server.hasArg("dir") ? server.arg("dir") : "/";
  if (!safePath(reqDir)) { server.send(400, "text/plain", "bad dir"); return; }
  currentDir = reqDir;

  String html = "<html><body><h1>ChronoFork Explorer</h1><h3>Dir: " + currentDir + "</h3>";
  File root = SD.open(currentDir);
  if (!root) { server.send(404, "text/plain", "dir not found"); return; }
  File file = root.openNextFile();
  while (file) {
    String name = String(file.name());
    String full = joinPath(currentDir, name);
    html += "<p><a href='/download?file=" + full + "'>" + name + "</a></p>";
    file = root.openNextFile();
  }
  root.close();
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleDownload() {
  if (!server.hasArg("file")) { server.send(400, "text/plain", "missing file"); return; }
  String path = server.arg("file");
  if (!safePath(path)) { server.send(400, "text/plain", "bad path"); return; }
  File f = SD.open(path, FILE_READ);
  if (!f) { server.send(404, "text/plain", "not found"); return; }
  server.streamFile(f, "text/csv");
  f.close();
}

void bindWebRoutes() {
  if (webRoutesBound) return;
  server.on("/", handleRoot);
  server.on("/download", handleDownload);
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

  u8g2.begin();
  u8g2.setContrast(contrastValue);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_9x15_tf);
  u8g2.drawStr(10, 40, "CHRONOFORK " VERSION);
  u8g2.drawStr(10, 70, "BOOTING...");
  u8g2.sendBuffer();
  delay(800);

  SPI.begin(14, 27, 13, SD_CS);
  if (SD.begin(SD_CS)) {
    if (!SD.exists("/CIRCUITS")) SD.mkdir("/CIRCUITS");
  } else {
    Serial.println("SD Error");
  }
  delay(500);

  SerialGPS.begin(38400, SERIAL_8N1, 17, 16);

  pinMode(BTN_OK, INPUT_PULLUP);
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP);

  u8g2.clearBuffer();
  u8g2.drawStr(10, 80, "SYSTEM READY");
  u8g2.sendBuffer();
  delay(800);
}

void drawMenu() {
  const char* opts[] = {"COURSE", "GPS", "WIFI", "CIRCUITS", "CHRONOS", "ECRAN"};
  int first = (menuSelection >= 4) ? menuSelection - 3 : 0;
  for (int i = 0; i < 4; i++) {
    int idx = first + i;
    if (idx >= 6) break;
    int yPos = 45 + (i * 28);
    if (idx == menuSelection) { u8g2.drawBox(0, yPos - 18, 256, 26); u8g2.setDrawColor(0); }
    u8g2.setFont(u8g2_font_9x15_tf);
    u8g2.drawStr(20, yPos, opts[idx]);
    u8g2.setDrawColor(1);
  }
}

String buildLogName() {
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
    u8g2.drawStr(10, 50, "RACE MODE");
    u8g2.setCursor(10, 90);  u8g2.printf("V: %.1f km/h", gps.speed.kmph());
    u8g2.setCursor(10, 120); u8g2.printf("FIC: %s", currentLogFile.c_str());
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
    u8g2.drawStr(10, 50, "WIFI ACTIVE");
    u8g2.setCursor(10, 90);  u8g2.print(WiFi.softAPIP().toString());
    u8g2.setCursor(10, 120); u8g2.print("PASS: " AP_PASS);
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
