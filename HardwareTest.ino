#include <Arduino.h>
#include <U8g2lib.h>
#include <SPI.h>
#include <SD.h>

// Définitions des PINS ChronoFork
#define SD_CS     32
#define BTN_OK    26
#define BTN_UP    33
#define BTN_DOWN  25
#define BTN_BACK  21

// Configuration Ecran (SPI Soft)
U8G2_ST75256_JLX256160_F_4W_SW_SPI u8g2(U8G2_R0, 18, 23, 5, 2, 0);

void setup() {
  Serial.begin(115200);
  delay(2000); // Temps pour ouvrir le moniteur série
  Serial.println("\n=== DIAGNOSTIC MATERIEL CHRONOFORK ===");

  // TEST 1 : ECRAN
  Serial.print("1. Initialisation Ecran... ");
  u8g2.begin();
  u8g2.setContrast(175);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_9x15_tf);
  u8g2.drawStr(10, 30, "TEST ECRAN : OK");
  u8g2.sendBuffer();
  Serial.println("OK (Verifiez l'affichage)");
  delay(1000);

  // TEST 2 : CARTE SD
  Serial.print("2. Initialisation SD (Pin 32)... ");
  SPI.begin(14, 27, 13, SD_CS);
  if (SD.begin(SD_CS)) {
    Serial.println("OK !");
    u8g2.drawStr(10, 60, "TEST SD : OK");
    uint8_t cardType = SD.cardType();
    Serial.printf("   Type de carte : %d\n", cardType);
    Serial.printf("   Taille SD : %lluMB\n", SD.cardSize() / (1024 * 1024));
  } else {
    Serial.println("ECHEC ! (Verifiez cablage/formatage)");
    u8g2.drawStr(10, 60, "TEST SD : ERREUR");
  }
  u8g2.sendBuffer();
  delay(1000);

  // TEST 3 : GPS (UART 2)
  Serial.print("3. Ouverture Port GPS (Pins 16/17)... ");
  Serial2.begin(38400, SERIAL_8N1, 17, 16); 
  Serial.println("Port ouvert. Attente de trames...");
  u8g2.drawStr(10, 90, "TEST GPS : PORT OUVERT");
  u8g2.sendBuffer();
  delay(1000);

  // TEST 4 : BOUTONS
  pinMode(BTN_OK, INPUT_PULLUP);
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP);
  Serial.println("4. Test Boutons pret. Appuyez sur vos boutons !");
  u8g2.drawStr(10, 120, "TEST BOUTONS : OK");
  u8g2.sendBuffer();
}

void loop() {
  // Verification du GPS dans le moniteur serie
  if (Serial2.available()) {
    char c = Serial2.read();
    // On affiche juste un point pour confirmer la reception de donnees brutes
    if (millis() % 500 == 0) Serial.print("."); 
  }

  // Test visuel des boutons sur l'ecran
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_9x15_tf);
  u8g2.drawStr(10, 25, "TEST BOUTONS EN DIRECT:");
  
  if (digitalRead(BTN_UP) == LOW)   u8g2.drawStr(10, 60, "-> HAUT (UP) APPUYE");
  if (digitalRead(BTN_DOWN) == LOW) u8g2.drawStr(10, 85, "-> BAS (DOWN) APPUYE");
  if (digitalRead(BTN_OK) == LOW)   u8g2.drawStr(10, 110, "-> OK APPUYE");
  if (digitalRead(BTN_BACK) == LOW) u8g2.drawStr(10, 135, "-> BACK APPUYE");

  u8g2.sendBuffer();
}
