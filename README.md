# EspNowSharedVariable

Bibliothèque **header-only** pour PlatformIO permettant de partager des variables entre microcontrôleurs ESP8266 et ESP32 via le protocole **ESP-NOW**. Une seule API masque les différences de syntaxe entre plateformes (ESP8266 vs ESP32, IDF 4.x / 5.x / 6.x).

> **Dépôt GitHub :** `https://github.com/Fo170/espNow_SharedVar` (lib `espNow_SharedVar`, classe `EspNowSharedVariable`).

> **Concept :** déclarez une variable, donnez-lui un nom → elle est automatiquement synchronisée sur tous les nœuds du réseau.

---

## Fonctionnalités

| Fonction | Description |
|----------|-------------|
| 🔌 **Cross-platform** | Même code sur ESP8266 et ESP32 (IDF 4.x / 5.x / 6.x) |
| 🔍 **Auto-découverte** | Les nœuds s'ajoutent automatiquement, pas besoin de connaître les adresses MAC |
| 💓 **Heartbeat** | Surveillance de l'état des peers (online / offline) |
| 🔐 **Chiffrement** | Clé PMK/KOK optionnelle pour sécuriser les échanges |
| 🛡️ **Vérification struct** | ID de structure pour éviter les mismatches de type |
| 📦 **Header-only** | Un seul fichier `.h`, pas de dépendance externe |

---

## Installation

### PlatformIO (recommandé)

Dans votre `platformio.ini`, ajoutez (convention Fo170 : toujours la forme git complète) :

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
lib_deps = https://github.com/Fo170/espNow_SharedVar.git

[env:esp8266_d1mini]
platform = espressif8266
board = d1_mini
framework = arduino
monitor_speed = 115200
lib_deps = https://github.com/Fo170/espNow_SharedVar.git
```

> Un `platformio.ini` prêt à l'emploi est fourni dans chaque exemple (`exemples/<nom>/`).

### Arduino IDE

Copiez `src/EspNowSharedVariable.h` dans le dossier de votre sketch.

---

## Utilisation rapide

### Émetteur (capteur)

```cpp
#include "EspNowSharedVariable.h"

EspNowSharedVariable esv;
float temperature = 0.0f;

void setup() {
  esv.begin(1);                    // Canal WiFi 1
  esv.enableAutoDiscovery(true);   // Auto-découverte des nœuds
  esv.registerVar("temp", temperature);
}

void loop() {
  temperature = lireCapteur();
  esv.setVar("temp", temperature);  // Envoi à tous les peers
  esv.update();
  esv.heartbeat(3000);
}
```

### Récepteur (afficheur)

```cpp
#include "EspNowSharedVariable.h"

EspNowSharedVariable esv;
float temperature = 0.0f;  // Même nom, même type

void setup() {
  esv.begin(1);
  esv.enableAutoDiscovery(true);
  esv.registerVar("temp", temperature);
}

void loop() {
  esv.update();              // Met à jour automatiquement la variable
  afficher(temperature);       // Utilise la valeur reçue
  esv.heartbeat(3000);
}
```

> **Pas de `addPeer()` nécessaire** si l'auto-découverte est activée.

---

## API

### Initialisation

| Méthode | Description |
|---------|-------------|
| `bool begin(uint8_t channel = 0)` | Initialise WiFi STA + ESP-NOW. Canal WiFi optionnel. |
| `bool setKey(const char* passphrase)` | Définit une clé de chiffrement (16 octets dérivés). **Avant** `begin()`. |
| `bool setKey(const uint8_t* key16)` | Définit une clé de chiffrement brute (16 octets). |

### Peers

| Méthode | Description |
|---------|-------------|
| `bool addPeer(mac_str)` | Ajoute manuellement un peer (MAC format `AA:BB:CC:DD:EE:FF`). |
| `void enableAutoDiscovery(bool)` | Active/désactive l'auto-découverte. |
| `void discover()` | Broadcast de présence au démarrage. |
| `bool isPeerOnline(mac)` | Vérifie si un peer est en ligne (heartbeat récent). |
| `uint8_t getOnlinePeerCount()` | Nombre de peers actuellement en ligne. |
| `void setPeerTimeout(uint32_t ms)` | Délai avant qu'un peer soit considéré hors ligne (défaut : 15 s). |

### Variables

| Méthode | Description |
|---------|-------------|
| `bool registerVar<T>("nom", var, structId=0)` | Déclare une variable synchronisée. `structId` optionnel pour les structs. |
| `bool setVar<T>("nom", valeur)` | Modifie la variable localement et propage la valeur. |
| `bool getVar<T>("nom", dest)` | Lit la valeur locale (utile pour vérifier). |
| `void update()` | À appeler dans `loop()` : traite la file de réception et met à jour les variables. |
| `bool sendAll()` | Envoie explicitement toutes les variables à tous les peers. |
| `bool sendTo(mac)` | Envoie toutes les variables à un peer spécifique. |

### Callback

| Méthode | Description |
|---------|-------------|
| `void onReceive(cb)` | `cb(const char* name, const uint8_t* mac)` appelé à chaque mise à jour. |

### Heartbeat

| Méthode | Description |
|---------|-------------|
| `void heartbeat(interval_ms)` | Envoie un heartbeat périodique (broadcast). À appeler dans `loop()`. |

---

## Types supportés

- Primitifs : `uint8/16/32/64`, `int8/16/32/64`, `float`, `double`, `bool`
- Tableaux fixes : `char[N]` (traités comme blob binaire)
- Structures : `struct` avec `__attribute__((packed))` + `structId` optionnel

> ⚠️ Utilisez des types à taille fixe (`uint32_t` plutôt que `int`) pour garantir la compatibilité ESP8266 ↔ ESP32.

---

## Partage de structure

```cpp
struct __attribute__((packed)) Capteur {
  float temperature;
  float humidite;
  uint16_t pression;
};

Capteur capteur = {0};

// structId = 0xABCD : vérification d'intégrité entre nœuds
esv.registerVar("env", capteur, 0xABCD);

// Envoi
esv.setVar("env", capteur);

// Réception (automatique via update())
esv.update();
// capteur.temperature, capteur.humidite... sont mis à jour
```

---

## Sécurité

Activez le chiffrement en appelant `setKey()` **avant** `begin()` :

```cpp
esv.setKey("MaPassphraseSecrete123");  // Dérivation automatique en 16 octets
// ou
uint8_t key[16] = {0x01, 0x02, ...};
esv.setKey(key);

esv.begin(1);
```

> La même clé doit être configurée sur **tous** les nœuds du réseau.

---

## Architecture

```
┌─────────────┐      ESP-NOW       ┌─────────────┐
│   Nœud A    │  ═══════════════►  │   Nœud B    │
│             │  ◄═══════════════  │             │
│  temp=25.3  │                    │  temp=25.3  │
│  led=ON     │                    │  led=ON     │
│  count=42   │                    │  count=42   │
└─────────────┘                    └─────────────┘
       │                                  │
       └─────────── Heartbeat ───────────┘
```

- **Auto-découverte** : un nœud inconnu qui émet un heartbeat ou un paquet discover est automatiquement ajouté.
- **File de réception** : buffer circulaire de 8 paquets, sans allocation dynamique, safe en ISR.
- **Compatibilité signatures** : détection automatique de `ESP_IDF_VERSION_MAJOR` pour basculer entre les anciennes et nouvelles signatures de callback ESP-NOW.

---

## Exemples fournis

Chaque exemple est un projet PlatformIO autonome (deux envs `esp32dev` et `esp8266_d1mini`), à compiler depuis son dossier :

```bash
cd exemples/emetteur && pio run -e esp32dev -e esp8266_d1mini
```

| Fichier | Description |
|---------|-------------|
| `exemples/emetteur/` | Capteur qui envoie température, compteur et alerte |
| `exemples/recepteur/` | Afficheur qui reçoit et affiche les variables |
| `exemples/bidirectionnel/` | Télécommande (LED + PWM) + capteur (les deux sens) |
| `exemples/struct/` | Partage d'une structure complète avec vérification d'ID |

---

## Configuration avancée

Définissez ces macros **avant** d'inclure `EspNowSharedVariable.h` pour personnaliser :

```cpp
#define ESV_MAX_VARS     32   // Variables partagées max (défaut: 16)
#define ESV_MAX_PEERS    16   // Peers max (défaut: 8)
#define ESV_RX_QUEUE     16   // File de réception (défaut: 8)
#define ESV_MAX_NAMELEN  31   // Longueur max des noms (défaut: 15)
#define ESV_PEER_TIMEOUT_MS 30000  // Timeout peer (défaut: 15s)

#include "EspNowSharedVariable.h"
```

---

## Licence

GPL-3.0 — Généré pour FOURNET Olivier.
