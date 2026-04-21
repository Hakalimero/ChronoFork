
FICHE DE MONTAGE


--- DESCRIPTION ---
Appareil : Chronomètre GPS + Datalogger (5s) + Web Admin
Cerveau : ESP32 (38 pins recommandé)
Écran : JLX256160 (Contrôleur ST75256)
Stockage : Module Micro SD SPI

--- CONNEXIONS ALIMENTATION ---
[+] ESP32 VIN (ou 5V) -> Alimentation 5V DC
[+] ESP32 3.3V       -> VCC Écran, VCC SD (si 3.3v), VCC GPS (si 3.3v)
[-] ESP32 GND        -> RELIER TOUS LES GND ENSEMBLE (Masse commune)

--- CONNEXIONS ÉCRAN (SPI LOGICIEL) ---
A (Écran Pin)      | B (ESP32 Pin) | Fonction
-------------------|---------------|-----------------
VCC                | 3.3V          | Alimentation
GND                | GND           | Masse
SCL (CLK)          | Pin 18        | Horloge
SDA (DIN)          | Pin 23        | Données
CS                 | Pin 5         | Chip Select
RS (DC)            | Pin 2         | Data/Command
RST                | Pin 0         | Reset

--- CONNEXIONS CARTE SD (SPI MATÉRIEL) ---
A (Module SD Pin)  | B (ESP32 Pin) | Fonction
-------------------|---------------|-----------------
VCC                | 5V ou 3.3V    | Alimentation
GND                | GND           | Masse
SCK                | Pin 14        | Horloge SPI
MISO               | Pin 27        | Data Out
MOSI               | Pin 13        | Data In
CS                 | Pin 32        | Chip Select SD

--- CONNEXIONS GPS (UART 2) ---
A (Module GPS Pin) | B (ESP32 Pin) | Fonction
-------------------|---------------|-----------------
VCC                | 3.3V ou 5V    | Alimentation
GND                | GND           | Masse
TX                 | Pin 17        | RX2 (Réception)
RX                 | Pin 16        | TX2 (Transmission)
* Note : Toujours croiser TX(GPS)->RX(ESP) et RX(GPS)->TX(ESP).

--- CONNEXIONS BOUTONS (INPUT_PULLUP) ---
Bouton             | B (ESP32 Pin) | Action
-------------------|---------------|-----------------
BOUTON 1           | Pin 33        | NAVIGATION HAUT (UP)
BOUTON 2           | Pin 25        | NAVIGATION BAS (DOWN)
BOUTON 3           | Pin 26        | VALIDER / MENU (OK)
BOUTON 4           | Pin 21        | RETOUR / CANCEL (BACK)
* Note : Connecter une borne du bouton à la Pin ESP, l'autre au GND.

--- RAPPEL LOGICIEL ---
Version : V2.1.1
Baudrate GPS : 38400
Baudrate Console : 115200
SSID WiFi : Hakalimero_V2
IP Admin : 192.168.4.1

=====================================================================
                      FIN DU PLAN DE MONTAGE
=====================================================================
Basé sur l'idée de foxlap . Schema ci-dessous non controlé.

<img width="1163" height="912" alt="Gemini_Generated_Image_1vjvv1vjvv1vjvv1" src="https://github.com/user-attachments/assets/4bd7667a-af0c-4e8a-a187-ce19ef33fd54" />
