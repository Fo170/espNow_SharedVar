# EspNowSharedVariable

Bibliothèque **header-only** pour PlatformIO permettant de partager des variables entre microcontrôleurs ESP8266 et ESP32 via le protocole **ESP-NOW**. Une seule API masque les différences de syntaxe entre plateformes (ESP8266 vs ESP32, IDF 4.x / 5.x / 6.x).

> **Version : 1.0.1** (nouveauté v1.0.1 : `esv.rearm()` pour survivre aux reset WiFi, voir [Reset WiFi & `rearm()`](#reset-wifi--rearm-v101)).

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
| `void rearm()` | **v1.0.1** — Ré-initialise ESP-NOW et ré-enregistre les callbacks après un reset WiFi. À appeler à chaque (re)connexion WiFi. |

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

## Reset WiFi & `rearm()` (v1.0.1)

### Le problème

Un gestionnaire WiFi qui réinitialise le module WiFi efface **l'état du stack ESP-NOW et les
callbacks de réception** enregistrés par `begin()`. C'est par exemple le cas de
`WiFiManagerESP._resetWiFi()`, qui passe par `WiFi.mode(WIFI_OFF)` (= `esp_wifi_stop` +
`esp_wifi_deinit`).

Symptôme : après ce reset, un `esp_now_init()` réussit (ESP-NOW « OK ») mais **plus aucun paquet
n'est traité** — les variables partagées cessent silencieusement d'être mises à jour (le callback
de réception a disparu).

### Le correctif : `rearm()`

La méthode publique **`rearm()`** ré-initialise ESP-NOW et **ré-enregistre les callbacks** de
réception/envoi, **sans toucher au WiFi**. Si une clé de chiffrement a été définie via `setKey()`,
elle est ré-appliquée. Compatible ESP8266 et ESP32 (IDF 4.x / 5.x / 6.x).

Elle doit être appelée **à chaque (re)connexion WiFi**, dans la boucle applicative :

```cpp
void loop() {
  // ... gestion WiFi / WiFiManager ...

  // v1.0.1 : à appeler une fois à chaque (re)connexion WiFi
  // (après wifiManager.isConnected(), car un reset WiFi efface les callbacks ESP-NOW)
  esv.rearm();

  esv.update();
  esv.heartbeat(3000);
  // ...
}
```

> ⚠️ **Le même correctif doit être appliqué aux deux nœuds d'un réseau ESP-NOW**
> (émetteur ET récepteur) : un reset WiFi sur l'un ou l'autre efface ses propres callbacks.

> ℹ️ Les exemples fournis (`exemples/`) appellent `rearm()` dans leur `loop()` : sans
> gestionnaire WiFi, la méthode est sans effet mais garantit que le code reste correct si un
> reset WiFi est ajouté.

> 📦 **Distribution de la v1.0.1** : pour bénéficier du correctif, soit copier la lib dans le
> dossier `lib/` du projet (prioritaire sur `lib_deps`), soit la publier sur GitHub pour les
> projets qui utilisent `lib_deps = https://github.com/Fo170/espNow_SharedVar.git`.

---

## Canaux WiFi / ESP-NOW : réglages à connaître

### Une seule radio, un seul canal

- Le microcontrôleur n'a qu'**une seule radio**. En mode **STA** (connecté à un point d'accès
  WiFi), la radio est calée sur le **canal de l'AP** auquel on est connecté (ex. canal 1, 6 ou 11).
- ESP-NOW n'est **pas** un protocole indépendant : il émet/reçoit sur le **canal radio courant**,
  comme le WiFi. Impossible d'être « sur le canal de la box » pour le WiFi **et** sur « un autre
  canal » pour ESP-NOW : c'est le même canal.
- Il n'y a **aucune coordination centrale** en ESP-NOW : pour qu'un paquet passe, **émetteur ET
  récepteur doivent être physiquement sur le même canal au même moment**.

### Le paramètre canal de `begin(channel)`

| Valeur | Comportement | Quand l'utiliser |
|---|---|---|
| **`0`** (défaut) | Ne force **rien** → ESP-NOW suit automatiquement le canal du WiFi (STA = canal de l'AP, AP = canal défini). | **Nœud connecté au WiFi** ✅ |
| **`> 0`** (ex. `1`) | ESP32 : appelle immédiatement `esp_wifi_set_channel(canal, WIFI_SECOND_CHAN_NONE)`. ESP8266 : sert uniquement à `addPeer()`. | Uniquement nœud **standalone** (sans connexion WiFi, mode AP fixe). |

Problèmes si on force un canal > 0 sur un nœud **connecté au WiFi** :

1. **Avant la connexion STA** (cas `begin()` appelé dans `setup()`) : le canal forcé est ensuite
   **écrasé par le driver** lors de l'association à l'AP (l'association se fait sur le canal de
   l'AP). Le forçage ne sert à rien… ou crée un **conflit** si l'AP est sur un autre canal.
2. **Une fois la STA connectée** : forcer un autre canal = **quitter le canal de l'AP** → perte de
   la connexion WiFi (déconnexions/reconnexions en boucle), voire échec de `esp_wifi_set_channel`
   (non autorisé en mode STA associé).

> ⚠️ Les exemples de la lib utilisent `esv.begin(1)` : c'est adapté à un réseau **sans connexion
> WiFi** (ESP-NOW standalone). Dès que le nœud est connecté à un point d'accès WiFi, utilisez
> **`esv.begin(0)`**.

### `addPeer` et le canal des peers

- `addPeer(mac)` avec canal **0** (défaut) → le peer utilise `_channel` (0 = suit la radio) : OK.
- `addPeer(mac, n>0)` : enregistre le peer avec un canal **figé**. Sur **ESP32**, l'émetteur ne
  change **pas** de canal par peer (contrairement à l'ESP8266 qui le permettait) : le canal
  renseigné est purement informatif — la communication ne se fait **que** sur le canal radio
  courant. Un canal ≠ canal courant → timeouts.
- Règle : toujours `addPeer(mac)` **sans canal** (ou explicitement `0`).

### Changement de canal (multi-AP / failover)

- Si le réseau peut basculer entre plusieurs points d'accès, ceux-ci peuvent être sur des
  **canaux différents**.
- En basculant d'un AP à l'autre, la radio **change de canal** → chaque nœud doit suivre **son**
  propre WiFi. Avec `begin(0)` tout le monde suit automatiquement ; avec un canal forcé, le
  failover casse l'ESP-NOW.
- Pendant la bascule, les données ESP-NOW sont **momentanément perdues** : c'est normal, elles
  reprennent dès que chaque nœud est re-stabilisé sur le canal du nouvel AP.

### « Je ne reçois rien en ESP-NOW »

Symptôme classique : un nœud ne reçoit plus les variables d'un autre, alors que peers et init sont
« OK ».

**Cause n°1 : canaux différents.** Ex. émetteur en AP standalone sur canal 1, récepteur connecté au
WiFi sur canal 11 → **aucun paquet**.

Correctifs (par ordre de préférence) :

1. **Connecter aussi l'émetteur au même WiFi** que le récepteur → même AP → même canal, tout suit
   automatiquement. C'est la solution la plus robuste.
2. Sinon (émetteur en AP standalone) :
   - **fixer le canal du routeur** 2,4 GHz sur une valeur stable (1, 6 ou 11) et désactiver le
     **choix auto du canal** (« auto channel »),
   - **régler l'AP de l'émetteur sur ce même canal**.
3. Ne **jamais** forcer `begin(n>0)` côté récepteur connecté au WiFi.

### Interaction avec les reset WiFi

- Les resets du gestionnaire WiFi (`WiFi.mode(WIFI_OFF)` = `esp_wifi_stop` + `esp_wifi_deinit`)
  effacent le stack ESP-NOW **et** le callback de réception (voir « Reset WiFi & `rearm()` »).
- `esv.rearm()` (appelé à chaque reconnexion) ré-initialise tout **sans toucher au WiFi ni au
  canal** → toujours à jour avec le canal courant. Encore une raison de rester en `begin(0)`.

### Lecture du canal au debug

- Sur ESP32, on peut lire le canal courant via `esp_wifi_get_channel()`.
- **`canal=0` tant que la STA n'est pas connectée** = normal (pas encore de canal radio).
- Une fois connecté, on doit voir le canal de l'AP (1/6/11…). S'il reste 0 alors que la STA est
  connectée, vérifier l'état du WiFi.

### Checklist « bons réglages »

- [ ] `esv.begin(0)` pour tout nœud **connecté au WiFi** — ne force rien, ESP-NOW suit le WiFi.
- [ ] Aucun `esp_wifi_set_channel` manuel dans le code applicatif.
- [ ] Aucun `addPeer` avec un canal > 0.
- [ ] `esv.rearm()` à chaque (re)connexion WiFi.
- [ ] Routeur 2,4 GHz : canal fixé (1/6/11), auto-switch de canal désactivé.
- [ ] Nœud émetteur : soit connecté au même WiFi que le récepteur, soit son AP réglé sur le
      **même canal** que le WiFi du récepteur.
- [ ] Ne **jamais** forcer `begin(n>0)` côté nœud connecté au WiFi.

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
