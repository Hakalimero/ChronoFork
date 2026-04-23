# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Projet

ChronoFork est un firmware Arduino/ESP32 pour un chronomètre GPS de circuit avec datalogger SD (5 s) et interface web d'administration. Cible matérielle : ESP32 38 pins + écran JLX256160 (ST75256) + module micro-SD SPI + GPS UART. Brochage complet dans `README.md` — toute modification de pin doit rester cohérente avec ce document.

Les commentaires, chaînes UI et noms de modes sont en français — conserver cette convention.

## Structure — trois sketches indépendants

Le dépôt n'est PAS un projet Arduino multi-fichiers : chaque `.ino` est une esquisse autonome à ouvrir seule dans l'IDE Arduino. Ne pas tenter de les compiler ensemble (symboles dupliqués : `u8g2`, `setup`, `loop`).

- `beta.ino` — version courante jouable (V2.1.6). `LCD_RST = Pin 0` avec séquence de reset manuelle dans `setup()` pour stabiliser l'écran au boot.
- `nonteste.ino` — variante expérimentale (V2.1.4) **non validée sur matériel**. `LCD_RST = Pin 4`, init séquencée anti-brownout, et handlers web supplémentaires (`/download`, navigation répertoire SD). Ne pas fusionner dans `beta.ino` sans test matériel préalable.
- `HardwareTest.ino` — diagnostic bas niveau (écran + SD + 4 boutons). À charger quand un bug matériel est suspecté avant de déboguer la logique applicative.

La différence sur `LCD_RST` (Pin 0 vs Pin 4) est la variable expérimentale centrale du projet — ne pas la « normaliser » sans demander.

## Compilation / Upload

Aucun outillage local (`arduino-cli`/`arduino` absents du PATH). Workflow attendu : Arduino IDE avec board ESP32, ou `arduino-cli` installé par l'utilisateur. Bibliothèques requises : `U8g2`, `TinyGPS++`, plus `SD`, `SPI`, `WiFi`, `WebServer` (fournies par le core ESP32). Baudrate console : 115200 ; baudrate GPS : 38400.

## Architecture applicative (beta.ino / nonteste.ino)

Boucle unique `loop()` pilotée par une machine à états `enum Mode` (`MENU`, `RACE`, `GPS_VIEW`, `WIFI_MODE`, `CIRCUIT_SEL`, `CHRONO_VIEW`, `SCREEN_SET`). À chaque itération :
1. pompe les octets GPS via `SerialGPS` (sauf en mode WiFi),
2. redessine le framebuffer U8g2 complet,
3. lit les 4 boutons en `INPUT_PULLUP` avec anti-rebond par `delay(150–300)` — pas d'interruption.

Le mode WiFi démarre un AP (`ChronoFork_WiFi`, IP `192.168.4.1`) et un `WebServer` sur port 80 ; il coupe la lecture GPS pour libérer la boucle. `BTN_BACK` est le seul chemin de sortie vers `MENU` et s'occupe d'arrêter le serveur.

Le log course écrit des CSV (`lat,lng,kmph`) toutes les 5 s dans un fichier `LOG_<jour>_<time>.csv` à la racine SD. Répertoire `/CIRCUITS` créé au boot si absent (uniquement `nonteste.ino`).

## Points de vigilance

- L'écran utilise **SPI logiciel** (pins 18/23/5/2/RST) ; la SD utilise le **SPI matériel** avec `SPI.begin(14, 27, 13, SD_CS=32)`. Ne pas mélanger les deux bus.
- La pin 0 est un strap ESP32 — d'où la séquence de reset explicite dans `beta.ino:36-41` avant `u8g2.begin()`.
- GPS sur `HardwareSerial(2)` avec `RX=17, TX=16` (croisé côté module GPS).
