// ============================================================================
//  emetteur — espNow_SharedVar (PlatformIO)
//
//  Noeud emetteur : lit un capteur et partage la valeur (temp, count, alert).
//  Compile tel quel pour ESP8266 ET ESP32 (cross-platform).
//
//  Compilation :  pio run -e esp32dev / -e esp8266_d1mini
// ============================================================================

#include <Arduino.h>
#include "EspNowSharedVariable.h"

EspNowSharedVariable esv;

float   temperature = 0.0f;
int32_t compteur    = 0;
bool    alerte      = false;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("=== Emetteur ESP-NOW ===");

  // Optionnel : chiffrement (meme cle sur tous les noeuds)
  // esv.setKey("MonReseauSecret");

  esv.begin(1);                    // Canal WiFi 1
  esv.enableAutoDiscovery(true);   // Les recepteurs s'ajoutent auto
  esv.discover();                  // S'annonce sur le reseau

  // Declarer les variables partagees
  esv.registerVar("temp",  temperature);
  esv.registerVar("count", compteur);
  esv.registerVar("alert", alerte);
}

void loop() {
  // --- Simuler des mesures ---
  temperature = 20.0f + random(0, 150) / 10.0f;  // 20.0 .. 35.0
  compteur++;
  alerte = (temperature > 30.0f);

  // --- Propager automatiquement ---
  esv.setVar("temp",  temperature);
  esv.setVar("count", compteur);
  esv.setVar("alert", alerte);

  Serial.printf("[TX] temp=%.1f count=%d alert=%s  |  peers=%d/%d online\n",
                temperature, compteur, alerte ? "ON" : "off",
                esv.getOnlinePeerCount(), esv.peerCount());

  esv.update();        // Traite les paquets entrants (bidirectionnel)
  esv.heartbeat(3000); // Ping toutes les 3s

  delay(2000);
}
