#include <Arduino.h>
#include <U8g2lib.h>
#include <SPI.h>
#include <SD.h>
#include <TinyGPS++.h>
#include <WiFi.h>
#include <WebServer.h>

#define VERSION "V2.1"
#define SD_CS     32
#define BTN_OK    26
#define BTN_UP    33
#define BTN_DOWN  25

U8G2_ST75256_JLX256160_F_4W_SW_SPI u8g2(U8G2_R0, 18, 23, 5, 2, 0);
TinyGPSPlus gps;
HardwareSerial SerialGPS(2); 
WebServer server(80);

enum Mode { RACE=0, GPS_VIEW=1, WIFI_MODE=2, CIRCUIT_SEL=3, CHRONO_VIEW=4, SCREEN_SET=5, MENU=100 };
Mode currentMode = MENU;

int menuSelection = 0, contrastValue = 175;
String selectedCircuit = "AUCUN", fileList[10], currentLogFile = "", currentDir = "/";
int fileCount = 0, fileIdx = 0, scrollIdx = 0;
float bestLap = 0.0, interTimes[8] = {0}; 
unsigned long lapStartTime = 0, lastLogTime = 0;
File fsUploadFile; 

// --- LOGIQUE SD ---
void scanCircuits() {
  fileCount = 0;
  File dir = SD.open("/CIRCUITS");
  if (!dir) return;
  while (File entry = dir.openNextFile()) {
    if (!entry.isDirectory() && fileCount < 10) fileList[fileCount++] = String(entry.name());
    entry.close();
  }
  dir.close();
}

// --- SERVEUR WEB V2.1 ---
void handleRoot() {
  if (server.hasArg("dir")) currentDir = server.arg("dir");
  if (!currentDir.endsWith("/")) currentDir += "/";

  String html = "<html><head><meta charset='UTF-8'><style>";
  html += "body{font-family:sans-serif;padding:20px;background:#f4f4f4;}";
  html += ".item{display:block;padding:12px;margin:5px 0;background:white;border-radius:5px;text-decoration:none;color:#333;box-shadow:0 1px 3px rgba(0,0,0,0.1);}";
  html += ".dir{border-left:6px solid #ffc107;font-weight:bold;}";
  html += ".file{border-left:6px solid #007bff;}";
  html += ".btn-back{background:#6c757d;color:white;padding:10px;display:inline-block;margin-bottom:15px;border-radius:5px;text-decoration:none;}";
  html += "</style></head><body><h1>Hakalimero " + String(VERSION) + "</h1>";
  
  html += "<h3>Répertoire : " + currentDir + "</h3>";
  if (currentDir != "/") html += "<a href='/?dir=/' class='btn-back'>⬅ Retour Racine</a>";

  File root = SD.open(currentDir);
  File file = root.openNextFile();
  while(file){
    String name = String(file.name());
    String fullPath = currentDir + name;
    if (file.isDirectory()) {
      html += "<a class='item dir' href='/?dir=" + fullPath + "'>📁 " + name + "</a>";
    } else {
      html += "<a class='item file' href='/download?file=" + fullPath + "'>📄 " + name + "</a>";
    }
    file = root.openNextFile();
  }
  html += "<hr><h4>Upload vers /CIRCUITS</h4><form method='POST' action='/upload' enctype='multipart/form-data'><input type='file' name='f'><input type='submit' value='Envoyer'></form>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleDownload() {
  String path = server.arg("file");
  if (SD.exists(path)) {
    File f = SD.open(path, FILE_READ);
    String dataType = "application/octet-stream";
    if (path.endsWith(".csv")) dataType = "text/csv";
    else if (path.endsWith(".json")) dataType = "application/json";
    server.streamFile(f, dataType);
    f.close();
  } else server.send(404, "Fichier inexistant");
}

void handleFileUpload() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) fsUploadFile = SD.open("/CIRCUITS/" + upload.filename, FILE_WRITE);
  else if (upload.status == UPLOAD_FILE_WRITE && fsUploadFile) fsUploadFile.write(upload.buf, upload.currentSize);
  else if (upload.status == UPLOAD_FILE_END && fsUploadFile) { 
    fsUploadFile.close(); 
    server.sendHeader("Location", "/"); 
    server.send(303); 
  }
}

void setup() {
  SerialGPS.begin(38400, SERIAL_8N1, 17, 16);
  u8g2.begin(); u8g2.setContrast(contrastValue);
  pinMode(BTN_OK, INPUT_PULLUP); pinMode(BTN_UP, INPUT_PULLUP); pinMode(BTN_DOWN, INPUT_PULLUP);
  SPI.begin(14, 27, 13, SD_CS);
  if(SD.begin(SD_CS)) {
    if (!SD.exists("/CIRCUITS")) SD.mkdir("/CIRCUITS");
    if (!SD.exists("/RECORDS")) SD.mkdir("/RECORDS");
  }
}

void loop() {
  if (currentMode != WIFI_MODE) while (SerialGPS.available() > 0) gps.encode(SerialGPS.read());
  u8g2.clearBuffer();

  if (currentMode == MENU) {
    const char* options[] = {"MODE COURSE", "INFOS GPS", "WIFI ADMIN", "CIRCUITS", "CHRONOS", "ECRAN"};
    int icons[] = {64, 80, 123, 75, 108, 73}; 
    int startIdx = (menuSelection >= 4) ? menuSelection - 3 : 0;
    for (int i = 0; i < 4; i++) {
      int idx = startIdx + i; if (idx >= 6) break;
      int yPos = 45 + (i * 28);
      if (idx == menuSelection) { u8g2.drawBox(0, yPos - 18, 256, 26); u8g2.setDrawColor(0); }
      u8g2.setFont(u8g2_font_open_iconic_all_2x_t); u8g2.drawGlyph(15, yPos + 2, icons[idx]);
      u8g2.setFont(u8g2_font_9x15_tf); u8g2.drawStr(50, yPos, options[idx]);
      u8g2.setDrawColor(1);
    }
  }
  else if (currentMode == RACE) {
    u8g2.setFont(u8g2_font_9x15_tf);
    u8g2.setCursor(10, 45); u8g2.print("RACE: " + selectedCircuit.substring(0,10));
    u8g2.setCursor(10, 80); u8g2.printf("VIT: %.1f KM/H", gps.speed.kmph());
    u8g2.setCursor(10, 110); u8g2.printf("CHRO: %.2f S", (millis() - lapStartTime)/1000.0);

    // --- DATALOGGER V2.1 (Toutes les 5 secondes) ---
    if (millis() - lastLogTime > 5000 && gps.location.isValid()) {
      File log = SD.open("/" + currentLogFile, FILE_APPEND);
      if (log) {
        // Format: Lat, Lon, Vitesse, T1, T2, T3...
        log.printf("%.6f,%.6f,%.1f", gps.location.lat(), gps.location.lng(), gps.speed.kmph());
        for(int i=0; i<4; i++) { log.printf(",%.2f", interTimes[i]); } 
        log.println();
        log.close();
      }
      lastLogTime = millis();
    }
  }
  else if (currentMode == GPS_VIEW) {
    u8g2.setFont(u8g2_font_9x15_tf);
    u8g2.drawStr(10, 35, "RECEPTION GPS");
    u8g2.setCursor(10, 65); u8g2.printf("SATS: %d", gps.satellites.value());
    u8g2.setCursor(10, 95); u8g2.printf("HEURE: %02d:%02d", gps.time.hour(), gps.time.minute());
    u8g2.setCursor(10, 125); u8g2.printf("LAT:%.5f", gps.location.lat());
    u8g2.setCursor(10, 155); u8g2.printf("LON:%.5f", gps.location.lng());
  }
  else if (currentMode == WIFI_MODE) {
    server.handleClient();
    u8g2.setFont(u8g2_font_9x15_tf);
    u8g2.drawStr(10, 45, "WIFI ADMIN");
    u8g2.setCursor(10, 85); u8g2.print(WiFi.softAPIP().toString());
  }
  else if (currentMode == CIRCUIT_SEL) {
    u8g2.setFont(u8g2_font_9x15_tf);
    u8g2.drawStr(10, 35, "CHOIX CIRCUIT");
    for (int i = 0; i < fileCount; i++) {
      int y = 65 + (i * 25);
      if (i == fileIdx) { u8g2.drawBox(0, y-18, 256, 22); u8g2.setDrawColor(0); }
      u8g2.drawStr(15, y, fileList[i].c_str()); u8g2.setDrawColor(1);
    }
  }
  else if (currentMode == CHRONO_VIEW) {
    u8g2.setFont(u8g2_font_9x15_tf);
    u8g2.drawStr(10, 35, "MES CHRONOS");
    for (int i = 0; i < 4; i++) {
      int dIdx = scrollIdx + i; int y = 65 + (i * 25);
      if (dIdx == 0) { u8g2.setCursor(10, y); u8g2.printf("BEST: %.2f", bestLap); }
      else if (dIdx <= 8) { u8g2.setCursor(10, y); u8g2.printf("T%d: %.2f", dIdx, interTimes[dIdx-1]); }
    }
  }
  else if (currentMode == SCREEN_SET) {
    u8g2.setFont(u8g2_font_9x15_tf);
    u8g2.setCursor(10, 95); u8g2.printf("CONTRASTE: %d", contrastValue);
    if (digitalRead(BTN_UP) == LOW) { contrastValue = min(255, contrastValue+5); u8g2.setContrast(contrastValue); delay(50); }
    if (digitalRead(BTN_DOWN) == LOW) { contrastValue = max(0, contrastValue-5); u8g2.setContrast(contrastValue); delay(50); }
  }

  u8g2.sendBuffer();

  // NAVIGATION
  if (digitalRead(BTN_UP) == LOW) {
    if (currentMode == MENU) menuSelection = (menuSelection - 1 + 6) % 6;
    else if (currentMode == CIRCUIT_SEL) fileIdx = (fileIdx - 1 + fileCount) % fileCount;
    else if (currentMode == CHRONO_VIEW) scrollIdx = max(0, scrollIdx - 1);
    delay(150);
  }
  if (digitalRead(BTN_DOWN) == LOW) {
    if (currentMode == MENU) menuSelection = (menuSelection + 1) % 6;
    else if (currentMode == CIRCUIT_SEL) fileIdx = (fileIdx + 1) % fileCount;
    else if (currentMode == CHRONO_VIEW) scrollIdx++;
    delay(150);
  }
  if (digitalRead(BTN_OK) == LOW) {
    delay(200);
    if (currentMode == MENU) {
      currentMode = (Mode)menuSelection;
      if (currentMode == WIFI_MODE) {
        WiFi.softAP("Hakalimero_V2");
        server.on("/", handleRoot); 
        server.on("/download", handleDownload);
        server.on("/upload", HTTP_POST, [](){ server.send(200); }, handleFileUpload);
        server.begin();
      }
      if (currentMode == CIRCUIT_SEL) scanCircuits();
      if (currentMode == RACE) {
        lapStartTime = millis();
        String prefix = (selectedCircuit == "AUCUN") ? "LOG" : selectedCircuit;
        if(prefix.indexOf('.') > 0) prefix = prefix.substring(0, prefix.indexOf('.')); 
        currentLogFile = prefix + "_" + String(gps.date.day()) + String(gps.date.month()) + "_" + String(gps.time.hour()) + String(gps.time.minute()) + ".csv";
        File log = SD.open("/" + currentLogFile, FILE_WRITE);
        if (log) { log.println("lat,lon,kmh,T1,T2,T3,T4"); log.close(); }
      }
      scrollIdx = 0;
    } else {
      if (currentMode == WIFI_MODE) { server.stop(); WiFi.mode(WIFI_OFF); }
      currentMode = MENU;
    }
    delay(300);
  }
}
