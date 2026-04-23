#include <Arduino.h>
#include <U8g2lib.h>
#include <SPI.h>
#include <SD.h>

#define SD_CS     32
#define BTN_OK    26
#define BTN_UP    33
#define BTN_DOWN  25
#define BTN_BACK  21

// Mise à jour ici aussi (Reset sur Pin 4)
U8G2_ST75256_JLX256160_F_4W_SW_SPI u8g2(U8G2_R0, 18, 23, 5, 2, 0);

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("--- TEST MATERIEL CHRONOFORK V2.1.4 ---");

  // 1. Ecran
  u8g2.begin();
  u8g2.setContrast(175);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_9x15_tf);
  u8g2.drawStr(10, 30, "1. ECRAN OK");
  u8g2.sendBuffer();
  Serial.println("Ecran Initialise.");

  // 2. SD
  SPI.begin(14, 27, 13, SD_CS);
  if (SD.begin(SD_CS)) {
    u8g2.drawStr(10, 60, "2. SD CARD OK");
    Serial.println("Carte SD detectee.");
  } else {
    u8g2.drawStr(10, 60, "2. SD CARD ERR");
    Serial.println("Erreur Carte SD.");
  }
  u8g2.sendBuffer();

  // 3. Boutons
  pinMode(BTN_OK, INPUT_PULLUP);
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP);
}

void loop() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.drawStr(10, 20, "APPUIE SUR LES BOUTONS :");

  if(digitalRead(BTN_UP) == LOW)   u8g2.drawStr(10, 50, "UP   [PRESSE]");
  if(digitalRead(BTN_DOWN) == LOW) u8g2.drawStr(10, 70, "DOWN [PRESSE]");
  if(digitalRead(BTN_OK) == LOW)   u8g2.drawStr(10, 90, "OK   [PRESSE]");
  if(digitalRead(BTN_BACK) == LOW) u8g2.drawStr(10, 110, "BACK [PRESSE]");

  u8g2.sendBuffer();
  delay(100);
}
