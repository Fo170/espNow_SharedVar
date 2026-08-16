// ============================================================================
//  recepteur — espNow_SharedVar (PlatformIO)
//
//  Noeud recepteur : affiche les variables partagees (temp, count, alert).
//  Aucun addPeer() necessaire grace a l'auto-decouverte.
//
//  Compilation :  pio run -e esp32dev / -e esp8266_d1mini
// ============================================================================

#include <Arduino.h>
#include "EspNowSharedVariable.h"

EspNowSharedVariable esv;

float   temperature = 0.0f;
int32_t compteur    = 0;
bool    alerte      = false;

void onNewValue(const char* name, const uint8_t* mac) {
  Serial.printf("  -> '%s' mise a jour par %02X:%02X:%02X:%02X:%02X:%02X\n",
                name, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("=== Recepteur ESP-NOW ===");

  // esv.setKey("MonReseauSecret");

  esv.begin(1);
  esv.enableAutoDiscovery(true);
  esv.discover();

  // Les memes noms que l'emetteur
  esv.registerVar("temp",  temperature);
  esv.registerVar("count", compteur);
  esv.registerVar("alert", alerte);

  esv.onReceive(onNewValue);
}

void loop() {
  // --- Traiter les paquets recus ---
  // Les variables locales sont MAJ automatiquement !
  esv.update();
  esv.heartbeat(3000);

  // --- Afficher l'etat ---
  Serial.printf("[RX] temp=%.1f  count=%d  alert=%s  |  peers=%d/%d\n",
                temperature, compteur, alerte ? "ON" : "off",
                esv.getOnlinePeerCount(), esv.peerCount());

  if (alerte) {
    Serial.println("  !!! ALERTE TEMPERATURE !!!");
  }

  delay(1000);
}
