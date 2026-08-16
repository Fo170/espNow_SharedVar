// ============================================================================
//  bidirectionnel — espNow_SharedVar (PlatformIO)
//
//  Noeud bidirectionnel : capteur + telecommande.
//  Partage sa temperature ET recoit des commandes (LED + PWM).
//  Compile tel quel pour ESP8266 ET ESP32 (broches mappees par cible).
//
//  Compilation :  pio run -e esp32dev / -e esp8266_d1mini
// ============================================================================

#include <Arduino.h>
#include "EspNowSharedVariable.h"

EspNowSharedVariable esv;

// Variables que JE partage (capteur)
float   maTemperature = 0.0f;

// Variables que JE recois (commandes)
bool    ledRouge  = false;
bool    ledVerte  = false;
uint8_t pwmMoteur = 0;

// Broches : D1/D2/D5 sur ESP8266, memes GPIO (5/4/14) sur ESP32
#if defined(ESP8266)
  const int PIN_LED_R = D1;
  const int PIN_LED_V = D2;
  const int PIN_PWM   = D5;
#else
  const int PIN_LED_R = 5;
  const int PIN_LED_V = 4;
  const int PIN_PWM   = 14;
#endif

void onNewValue(const char* name, const uint8_t* mac) {
  Serial.printf("[EVT] '%s' recu de %02X:%02X:%02X:%02X:%02X:%02X\n",
                name, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_LED_V, OUTPUT);
  pinMode(PIN_PWM,   OUTPUT);

  // esv.setKey("MonReseauSecret");

  esv.begin(1);
  esv.enableAutoDiscovery(true);
  esv.discover();

  // Mes variables (je les mets a jour, les autres les recoivent)
  esv.registerVar("temp", maTemperature);

  // Variables distantes (mises a jour par les autres, je les lis)
  esv.registerVar("ledR", ledRouge);
  esv.registerVar("ledV", ledVerte);
  esv.registerVar("pwm",  pwmMoteur);

  esv.onReceive(onNewValue);
}

void loop() {
  // --- 1. Lire mon capteur et partager ---
  maTemperature = 20.0f + random(0, 100) / 10.0f;
  esv.setVar("temp", maTemperature);

  // --- 2. Recevoir les commandes ---
  esv.update();
  esv.heartbeat(3000);

  // --- 3. Appliquer les commandes sur le hardware ---
  digitalWrite(PIN_LED_R, ledRouge ? HIGH : LOW);
  digitalWrite(PIN_LED_V, ledVerte ? HIGH : LOW);
  analogWrite(PIN_PWM, pwmMoteur);

  Serial.printf("[BIDIR] TX: temp=%.1f  |  RX: ledR=%d ledV=%d pwm=%d  |  peers=%d\n",
                maTemperature, ledRouge, ledVerte, pwmMoteur,
                esv.getOnlinePeerCount());

  delay(1000);
}
