#include <Arduino.h>
#include <U8g2lib.h>
#include <SPI.h>
#include <SD.h>
#include <TinyGPS++.h>
#include <WiFi.h>
#include <WebServer.h>

#define VERSION   "V2.1.6"
#define SD_CS     32
#define BTN_OK    26
#define BTN_UP    33
#define BTN_DOWN  25
#define BTN_BACK  21
#define LCD_RST   0   // Retour sur la Pin 0

// Configuration Ecran : Horloge=18, Data=23, CS=5, DC=2, Reset=0
U8G2_ST75256_JLX256160_F_4W_SW_SPI u8g2(U8G2_R0, 18, 23, 5, 2, LCD_RST);

TinyGPSPlus gps;
HardwareSerial SerialGPS(2); 
WebServer server(80);

enum Mode { RACE=0, GPS_VIEW=1, WIFI_MODE=2, CIRCUIT_SEL=3, CHRONO_VIEW=4, SCREEN_SET=5, MENU=100 };
Mode currentMode = MENU;

int menuSelection = 0, contrastValue = 175;
String selectedCircuit = "AUCUN", currentLogFile = "", currentDir = "/";
unsigned long lapStartTime = 0, lastLogTime = 0;

// --- INITIALISATION ---
void setup() {
  Serial.begin(115200);
  
  // Séquence de démarrage pour stabiliser l'écran sur la Pin 0
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
  u8g2.drawStr(10, 40, "CHRONOFORK V2.1.6");
  u8g2.drawStr(10, 70, "MODE: RST G0");
  u8g2.sendBuffer();
  delay(1000);

  // Initialisation SD
  SPI.begin(14, 27, 13, SD_CS);
  if(!SD.begin(SD_CS)) {
    Serial.println("SD Error");
  }

  // Initialisation GPS
  SerialGPS.begin(38400, SERIAL_8N1, 17, 16);
  
  // Initialisation Boutons
  pinMode(BTN_OK, INPUT_PULLUP);
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP);
}

// --- LOGIQUE WIFI / WEB ---
void handleRoot() {
  String html = "<html><body><h1>ChronoFork Web</h1>";
  html += "<p>Repertoire: " + currentDir + "</p></body></html>";
  server.send(200, "text/html", html);
}

void loop() {
  // Lecture GPS
  if (currentMode != WIFI_MODE) {
    while (SerialGPS.available() > 0) gps.encode(SerialGPS.read());
  }

  u8g2.clearBuffer();

  // Gestion du bouton BACK (Sortie de mode)
  if (digitalRead(BTN_BACK) == LOW) {
    if (currentMode != MENU) {
      if (currentMode == WIFI_MODE) { server.stop(); WiFi.mode(WIFI_OFF); }
      currentMode = MENU;
      delay(300);
    }
  }

  // Affichage selon le mode
  if (currentMode == MENU) {
    const char* options[] = {"COURSE", "GPS", "WIFI", "CIRCUITS", "CHRONOS", "ECRAN"};
    for (int i = 0; i < 4; i++) {
      int idx = ((menuSelection >= 4) ? menuSelection - 3 : 0) + i;
      if (idx >= 6) break;
      int yPos = 45 + (i * 28);
      if (idx == menuSelection) { u8g2.drawBox(0, yPos - 18, 256, 26); u8g2.setDrawColor(0); }
      u8g2.setFont(u8g2_font_9x15_tf);
      u8g2.drawStr(30, yPos, options[idx]);
      u8g2.setDrawColor(1);
    }
  } 
  else if (currentMode == RACE) {
    u8g2.setFont(u8g2_font_9x15_tf);
    u8g2.drawStr(10, 40, "SESSION EN COURS");
    u8g2.setCursor(10, 80); u8g2.printf("VIT: %.1f km/h", gps.speed.kmph());
    u8g2.setCursor(10, 110); u8g2.printf("SAT: %d", gps.satellites.value());
  }
  else if (currentMode == WIFI_MODE) {
    server.handleClient();
    u8g2.drawStr(10, 40, "WIFI : ChronoFork");
    u8g2.setCursor(10, 80); u8g2.print("IP: " + WiFi.softAPIP().toString());
  }

  u8g2.sendBuffer();

  // Navigation Menu
  if (digitalRead(BTN_UP) == LOW) { menuSelection = (menuSelection - 1 + 6) % 6; delay(200); }
  if (digitalRead(BTN_DOWN) == LOW) { menuSelection = (menuSelection + 1) % 6; delay(200); }
  if (digitalRead(BTN_OK) == LOW) {
    if (currentMode == MENU) {
      currentMode = (Mode)menuSelection;
      if (currentMode == WIFI_MODE) {
        WiFi.softAP("ChronoFork_WiFi");
        server.on("/", handleRoot);
        server.begin();
      }
    }
    delay(300);
  }
}
