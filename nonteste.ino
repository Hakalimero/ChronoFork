#include <Arduino.h>
#include <U8g2lib.h>
#include <SPI.h>
#include <SD.h>
#include <TinyGPS++.h>
#include <WiFi.h>
#include <WebServer.h>

#define VERSION   "V2.1.4"
#define SD_CS     32
#define BTN_OK    26
#define BTN_UP    33
#define BTN_DOWN  25
#define BTN_BACK  21

// Modification ici : le dernier paramètre est passé de 0 à 4
U8G2_ST75256_JLX256160_F_4W_SW_SPI u8g2(U8G2_R0, 18, 23, 5, 2, 4);

TinyGPSPlus gps;
HardwareSerial SerialGPS(2); 
WebServer server(80);

enum Mode { RACE=0, GPS_VIEW=1, WIFI_MODE=2, CIRCUIT_SEL=3, CHRONO_VIEW=4, SCREEN_SET=5, MENU=100 };
Mode currentMode = MENU;

int menuSelection = 0, contrastValue = 175;
String selectedCircuit = "AUCUN", currentLogFile = "", currentDir = "/";
unsigned long lapStartTime = 0, lastLogTime = 0;
File fsUploadFile;

// --- INITIALISATION SÉQUENCÉE (ANTI-BROWNOUT) ---
void setup() {
  u8g2.begin();
  u8g2.setContrast(contrastValue);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_9x15_tf);
  u8g2.drawStr(10, 40, "CHRONOFORK V2.1.4");
  u8g2.drawStr(10, 70, "BOOTING...");
  u8g2.sendBuffer();
  delay(800); 

  SPI.begin(14, 27, 13, SD_CS);
  if(SD.begin(SD_CS)) {
    if (!SD.exists("/CIRCUITS")) SD.mkdir("/CIRCUITS");
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

// --- LOGIQUE WEB (STRICT NECESSAIRE) ---
void handleRoot() {
  if (server.hasArg("dir")) currentDir = server.arg("dir");
  String html = "<html><body><h1>ChronoFork Explorer</h1><h3>Dir: " + currentDir + "</h3>";
  File root = SD.open(currentDir);
  File file = root.openNextFile();
  while(file){
    String name = String(file.name());
    html += "<p><a href='/download?file=" + currentDir + name + "'>" + name + "</a></p>";
    file = root.openNextFile();
  }
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleDownload() {
  String path = server.arg("file");
  File f = SD.open(path, FILE_READ);
  server.streamFile(f, "text/csv");
  f.close();
}

void loop() {
  if (currentMode != WIFI_MODE) while (SerialGPS.available() > 0) gps.encode(SerialGPS.read());
  u8g2.clearBuffer();

  // Bouton Retour
  if (digitalRead(BTN_BACK) == LOW) {
    if (currentMode != MENU) {
      if (currentMode == WIFI_MODE) { server.stop(); WiFi.mode(WIFI_OFF); }
      currentMode = MENU; delay(300);
    }
  }

  if (currentMode == MENU) {
    const char* opts[] = {"COURSE", "GPS", "WIFI", "CIRCUITS", "CHRONOS", "ECRAN"};
    for (int i = 0; i < 4; i++) {
      int idx = ((menuSelection >= 4) ? menuSelection - 3 : 0) + i;
      if (idx >= 6) break;
      if (idx == menuSelection) { u8g2.drawBox(0, 45+(i*28)-18, 256, 26); u8g2.setDrawColor(0); }
      u8g2.setFont(u8g2_font_9x15_tf); u8g2.drawStr(20, 45+(i*28), opts[idx]);
      u8g2.setDrawColor(1);
    }
  } 
  else if (currentMode == RACE) {
    u8g2.drawStr(10, 50, "RACE MODE");
    u8g2.setCursor(10, 90); u8g2.printf("V: %.1f km/h", gps.speed.kmph());
    if (millis() - lastLogTime > 5000 && gps.location.isValid()) {
       File log = SD.open("/" + currentLogFile, FILE_APPEND);
       if (log) { log.printf("%.6f,%.6f,%.1f\n", gps.location.lat(), gps.location.lng(), gps.speed.kmph()); log.close(); }
       lastLogTime = millis();
    }
  }
  else if (currentMode == WIFI_MODE) {
    server.handleClient();
    u8g2.drawStr(10, 50, "WIFI ACTIVE");
    u8g2.setCursor(10, 90); u8g2.print(WiFi.softAPIP().toString());
  }

  u8g2.sendBuffer();

  if (digitalRead(BTN_UP) == LOW) { menuSelection = (menuSelection - 1 + 6) % 6; delay(150); }
  if (digitalRead(BTN_DOWN) == LOW) { menuSelection = (menuSelection + 1) % 6; delay(150); }
  if (digitalRead(BTN_OK) == LOW) {
    currentMode = (Mode)menuSelection;
    if (currentMode == WIFI_MODE) { WiFi.softAP("ChronoFork_WiFi"); server.on("/", handleRoot); server.on("/download", handleDownload); server.begin(); }
    if (currentMode == RACE) { 
      currentLogFile = "LOG_" + String(gps.date.day()) + "_" + String(gps.time.value()) + ".csv";
      lapStartTime = millis(); 
    }
    delay(300);
  }
}
