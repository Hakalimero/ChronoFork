#include <Arduino.h>
#include <U8g2lib.h>
#include <SPI.h>
#include <SD.h>

#define SD_CS     32
#define BTN_OK    26
#define BTN_UP    33
#define BTN_DOWN  25
#define BTN_BACK  21
#define LCD_RST   0

U8G2_ST75256_JLX256160_F_4W_SW_SPI u8g2(U8G2_R0, 18, 23, 5, 2, LCD_RST);

bool sdOk = false;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("--- TEST MATERIEL CHRONOFORK V2.1.5 ---");

  // Reset ecran explicite (Pin 0 est un strap : piloter APRES le boot).
  pinMode(LCD_RST, OUTPUT);
  digitalWrite(LCD_RST, HIGH); delay(100);
  digitalWrite(LCD_RST, LOW);  delay(200);
  digitalWrite(LCD_RST, HIGH);

  u8g2.begin();
  u8g2.setContrast(175);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_9x15_tf);
  u8g2.drawStr(10, 30, "1. ECRAN OK");
  u8g2.sendBuffer();
  Serial.println("Ecran Initialise.");

  SPI.begin(14, 27, 13, SD_CS);
  sdOk = SD.begin(SD_CS);
  u8g2.drawStr(10, 60, sdOk ? "2. SD CARD OK" : "2. SD CARD ERR");
  Serial.println(sdOk ? "Carte SD detectee." : "Erreur Carte SD.");
  u8g2.sendBuffer();

  pinMode(BTN_OK, INPUT_PULLUP);
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP);
}

void loop() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.drawStr(10, 20, "APPUIE SUR LES BOUTONS :");
  u8g2.drawStr(10, 140, sdOk ? "SD: OK" : "SD: ERR");

  if (digitalRead(BTN_UP)   == LOW) u8g2.drawStr(10, 50,  "UP   [PRESSE]");
  if (digitalRead(BTN_DOWN) == LOW) u8g2.drawStr(10, 70,  "DOWN [PRESSE]");
  if (digitalRead(BTN_OK)   == LOW) u8g2.drawStr(10, 90,  "OK   [PRESSE]");
  if (digitalRead(BTN_BACK) == LOW) u8g2.drawStr(10, 110, "BACK [PRESSE]");

  u8g2.sendBuffer();
  delay(50);
}
