// ============================================================================
//  struct — espNow_SharedVar (PlatformIO)
//
//  Partage d'une structure complete entre noeuds, avec verification
//  d'integrite par struct_id (0xCAFE). Compile pour ESP8266 et ESP32.
//
//  Compilation :  pio run -e esp32dev / -e esp8266_d1mini
// ============================================================================

#include <Arduino.h>
#include "EspNowSharedVariable.h"

EspNowSharedVariable esv;

struct __attribute__((packed)) CapteurEnv {
  float    temperature;
  float    humidite;
  uint16_t pression;
  uint8_t  batterie;   // pourcentage
};

CapteurEnv capteur = {0};

void setup() {
  Serial.begin(115200);
  delay(500);

  esv.begin(1);
  esv.enableAutoDiscovery(true);
  esv.discover();

  // struct_id = 0xCAFE : verification d'integrite entre noeuds
  esv.registerVar("env", capteur, 0xCAFE);
}

void loop() {
  // --- Emetteur ---
  capteur.temperature = 22.5f;
  capteur.humidite    = 55.0f;
  capteur.pression    = 1013;
  capteur.batterie    = 87;

  esv.setVar("env", capteur);

  // --- Recepteur (sur un autre noeud) ---
  esv.update();

  Serial.printf("[STRUCT] T=%.1f H=%.1f P=%d Bat=%d%%  |  peers=%d\n",
                capteur.temperature, capteur.humidite,
                capteur.pression, capteur.batterie,
                esv.getOnlinePeerCount());

  esv.heartbeat(5000);

  // v1.0.1 : re-arme ESP-NOW apres un eventuel reset WiFi (ex. WiFiManagerESP).
  // A appeler a chaque (re)connexion WiFi (sans effet ici, pas de gestionnaire WiFi).
  esv.rearm();

  delay(3000);
}
