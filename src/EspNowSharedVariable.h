// ============================================================================
//  EspNowSharedVariable.h
//  Bibliotheque header-only pour PlatformIO
//  ESP-NOW cross-platform : ESP8266 & ESP32 (IDF 4.x / 5.x / 6.x)
//  Echange de variables synchronisees entre noeuds IoT
//
//  Fonctionnalites :
//    - API unique ESP8266 / ESP32
//    - Variables partagees typees (primitifs + structs)
//    - Heartbeat & surveillance des peers (online/offline)
//    - Auto-decouverte des noeuds (broadcast + reponse)
//    - Chiffrement PMK/KOK (cle maitre 16 octets)
//    - Verification d'integrite des structs par ID
//
//  Auteur  : Genere pour FOURNET Olivier (GPL-3.0)
//  Version : 1.0.1
// ============================================================================

#ifndef ESPNOW_SHARED_VARIABLE_H
#define ESPNOW_SHARED_VARIABLE_H

#include <Arduino.h>
#include <string.h>
#include <stdint.h>

// ----------------------------------------------------------------------------
//  Includes plateforme
// ----------------------------------------------------------------------------
#if defined(ESP8266)
  #include <ESP8266WiFi.h>
  extern "C" {
    #include <espnow.h>
  }
  #define ESVMAC_T  uint8_t
  #define ESVROLE   ESP_NOW_ROLE_COMBO
#elif defined(ESP32)
  #include <WiFi.h>
  #include <esp_now.h>
  #include <esp_wifi.h>
  #define ESVMAC_T  uint8_t
#else
  #error "Plateforme non supportee. Utilisez ESP8266 ou ESP32."
#endif

// ----------------------------------------------------------------------------
//  Constantes configurables (definir AVANT l'include pour surcharger)
// ----------------------------------------------------------------------------
#ifndef ESV_MAX_VARS
  #define ESV_MAX_VARS    16
#endif
#ifndef ESV_MAX_PEERS
  #define ESV_MAX_PEERS   8
#endif
#ifndef ESV_RX_QUEUE
  #define ESV_RX_QUEUE    8
#endif
#ifndef ESV_MAX_NAMELEN
  #define ESV_MAX_NAMELEN 15
#endif
#ifndef ESV_PEER_TIMEOUT_MS
  #define ESV_PEER_TIMEOUT_MS 15000
#endif

static const uint8_t ESV_BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// ----------------------------------------------------------------------------
//  Types supportes
// ----------------------------------------------------------------------------
enum VarType : uint8_t {
  TYPE_CTRL   = 0x00,  // Paquets de controle (heartbeat, discovery)
  TYPE_UINT8  = 0x01,
  TYPE_INT8   = 0x02,
  TYPE_UINT16 = 0x03,
  TYPE_INT16  = 0x04,
  TYPE_UINT32 = 0x05,
  TYPE_INT32  = 0x06,
  TYPE_UINT64 = 0x07,
  TYPE_INT64  = 0x08,
  TYPE_FLOAT  = 0x09,
  TYPE_DOUBLE = 0x0A,
  TYPE_BOOL   = 0x0B,
  TYPE_STRUCT = 0xFF
};

enum CtrlSubType : uint8_t {
  SUB_HEARTBEAT       = 0x01,
  SUB_DISCOVER        = 0x02,
  SUB_DISCOVER_REPLY  = 0x03
};

// ----------------------------------------------------------------------------
//  Traits de type
// ----------------------------------------------------------------------------
template<typename T> struct TypeTraits {
  static constexpr uint8_t id   = TYPE_STRUCT;
  static constexpr size_t  size = sizeof(T);
};

template<> struct TypeTraits<uint8_t>  { static constexpr uint8_t id = TYPE_UINT8;  static constexpr size_t size = 1; };
template<> struct TypeTraits<int8_t>   { static constexpr uint8_t id = TYPE_INT8;   static constexpr size_t size = 1; };
template<> struct TypeTraits<uint16_t> { static constexpr uint8_t id = TYPE_UINT16; static constexpr size_t size = 2; };
template<> struct TypeTraits<int16_t>  { static constexpr uint8_t id = TYPE_INT16;  static constexpr size_t size = 2; };
template<> struct TypeTraits<uint32_t>{ static constexpr uint8_t id = TYPE_UINT32; static constexpr size_t size = 4; };
template<> struct TypeTraits<int32_t>  { static constexpr uint8_t id = TYPE_INT32;  static constexpr size_t size = 4; };
template<> struct TypeTraits<uint64_t>{ static constexpr uint8_t id = TYPE_UINT64; static constexpr size_t size = 8; };
template<> struct TypeTraits<int64_t>  { static constexpr uint8_t id = TYPE_INT64;  static constexpr size_t size = 8; };
template<> struct TypeTraits<float>    { static constexpr uint8_t id = TYPE_FLOAT;  static constexpr size_t size = 4; };
template<> struct TypeTraits<double>  { static constexpr uint8_t id = TYPE_DOUBLE; static constexpr size_t size = 8; };
template<> struct TypeTraits<bool>    { static constexpr uint8_t id = TYPE_BOOL;   static constexpr size_t size = sizeof(bool); };

// ----------------------------------------------------------------------------
//  Structures internes
// ----------------------------------------------------------------------------
struct SharedVar {
  void*    ptr;
  uint8_t  type_id;
  uint16_t size;
  uint16_t struct_id;   // 0 = pas de verification, sinon ID du struct
  char     name[ESV_MAX_NAMELEN + 1];
  uint8_t  name_len;
  bool     active;
};

struct PeerInfo {
  uint8_t  mac[6];
  uint32_t lastSeen;
  uint32_t uptime;
  bool     online;
  bool     autoAdded;
#if defined(ESP32) && ESP_IDF_VERSION_MAJOR >= 5
  int8_t   rssi;
#endif
};

struct RxPacket {
  uint8_t mac[6];
  uint8_t data[250];
  uint8_t len;
};

// ----------------------------------------------------------------------------
//  Instance globale (pour les callbacks C statiques)
// ----------------------------------------------------------------------------
class EspNowSharedVariable;
static EspNowSharedVariable* _esv_instance = nullptr;

// ----------------------------------------------------------------------------
//  Classe principale
// ----------------------------------------------------------------------------
class EspNowSharedVariable {
public:
  // --------------------------------------------------------------------------
  //  Constructeur / Destructeur
  // --------------------------------------------------------------------------
  EspNowSharedVariable() {
    if (!_esv_instance) _esv_instance = this;
    memset(_vars, 0, sizeof(_vars));
    memset(_peerInfo, 0, sizeof(_peerInfo));
    memset(_rxQueue, 0, sizeof(_rxQueue));
    _peerCount = 0;
    _rxHead = 0;
    _rxTail = 0;
    _userCb = nullptr;
    _channel = 0;
    _hasKey = false;
    _autoDiscovery = false;
    _lastHeartbeat = 0;
    _peerTimeout = ESV_PEER_TIMEOUT_MS;
  }

  ~EspNowSharedVariable() {
    if (_esv_instance == this) _esv_instance = nullptr;
  }

  // --------------------------------------------------------------------------
  //  Configuration cle de chiffrement (a appeler AVANT begin())
  // --------------------------------------------------------------------------
  bool setKey(const char* passphrase) {
    if (!passphrase || strlen(passphrase) == 0) return false;
    _deriveKey(passphrase, _key);
    _hasKey = true;
    return true;
  }

  bool setKey(const uint8_t* key16) {
    if (!key16) return false;
    memcpy(_key, key16, 16);
    _hasKey = true;
    return true;
  }

  // --------------------------------------------------------------------------
  //  Initialisation
  // --------------------------------------------------------------------------
  bool begin(uint8_t channel = 0) {
    _channel = channel;
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false);

#if defined(ESP32)
    if (channel > 0) {
      esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    }
#endif

#if defined(ESP8266)
    if (esp_now_init() != 0) return false;
    if (_hasKey) {
      esp_now_set_kok((u8*)_key, 16);
    }
    esp_now_set_self_role(ESVROLE);
    esp_now_register_recv_cb(_onRecv_8266);
    esp_now_register_send_cb(_onSent_8266);
#elif defined(ESP32)
    if (esp_now_init() != ESP_OK) return false;
    if (_hasKey) {
      esp_now_set_pmk(_key);
    }
    #if ESP_IDF_VERSION_MAJOR >= 5
      esp_now_register_recv_cb(_onRecv_idf5plus);
    #else
      esp_now_register_recv_cb(_onRecv_idf4);
    #endif
    esp_now_register_send_cb(_onSent_32);
#endif
    return true;
  }

  // --------------------------------------------------------------------------
  //  Ré-armement après un reset WiFi
  //  Le manager WiFi (WiFi.mode(WIFI_OFF) → esp_wifi_stop/deinit) efface l'état
  //  du stack ESP-NOW et le callback de réception enregistré par begin(). Cette
  //  méthode ré-initialise ESP-NOW et ré-enregistre les callbacks SANS toucher
  //  au WiFi (à appeler à chaque (re)connexion WiFi, dans le loop).
  // --------------------------------------------------------------------------
  void rearm() {
#if defined(ESP8266)
    esp_now_init();
    if (_hasKey) {
      esp_now_set_kok((u8*)_key, 16);
    }
    esp_now_set_self_role(ESVROLE);
    esp_now_register_recv_cb(_onRecv_8266);
    esp_now_register_send_cb(_onSent_8266);
#elif defined(ESP32)
    esp_now_init();
    if (_hasKey) {
      esp_now_set_pmk(_key);
    }
    #if ESP_IDF_VERSION_MAJOR >= 5
      esp_now_register_recv_cb(_onRecv_idf5plus);
    #else
      esp_now_register_recv_cb(_onRecv_idf4);
    #endif
    esp_now_register_send_cb(_onSent_32);
#endif
  }

  // --------------------------------------------------------------------------
  //  Gestion des peers
  // --------------------------------------------------------------------------
  bool addPeer(const uint8_t* mac, uint8_t channel = 0) {
    if (_findPeerIndex(mac) >= 0) return true;  // deja connu
    if (_peerCount >= ESV_MAX_PEERS) return false;
    if (channel == 0) channel = _channel;

#if defined(ESP8266)
    if (esp_now_add_peer((u8*)mac, ESVROLE, channel, nullptr, 0) != 0) return false;
#elif defined(ESP32)
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, mac, 6);
    peer.channel = channel;
    peer.encrypt = _hasKey;
    if (_hasKey) {
      memcpy(peer.lmk, _key, 16);
    }
    if (esp_now_add_peer(&peer) != ESP_OK) return false;
#endif

    memcpy(_peerInfo[_peerCount].mac, mac, 6);
    _peerInfo[_peerCount].lastSeen = 0;
    _peerInfo[_peerCount].uptime = 0;
    _peerInfo[_peerCount].online = false;
    _peerInfo[_peerCount].autoAdded = false;
    _peerCount++;
    return true;
  }

  bool addPeer(const char* macStr, uint8_t channel = 0) {
    uint8_t mac[6];
    if (sscanf(macStr, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
               &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]) != 6) {
      return false;
    }
    return addPeer(mac, channel);
  }

  // --------------------------------------------------------------------------
  //  Enregistrement de variables
  // --------------------------------------------------------------------------
  template<typename T>
  bool registerVar(const char* name, T& var, uint16_t structId = 0) {
    int idx = _findVarSlot();
    if (idx < 0) return false;
    SharedVar& sv = _vars[idx];
    sv.ptr = (void*)&var;
    sv.type_id = TypeTraits<T>::id;
    sv.size = TypeTraits<T>::size;
    sv.struct_id = (sv.type_id == TYPE_STRUCT) ? structId : 0;
    sv.name_len = (uint8_t)strnlen(name, ESV_MAX_NAMELEN);
    memcpy(sv.name, name, sv.name_len);
    sv.name[sv.name_len] = '\0';
    sv.active = true;
    return true;
  }

  // --------------------------------------------------------------------------
  //  Ecriture locale + diffusion
  // --------------------------------------------------------------------------
  template<typename T>
  bool setVar(const char* name, const T& value) {
    int idx = _findVarByName(name);
    if (idx < 0) return false;
    SharedVar& sv = _vars[idx];
    if (sv.size != sizeof(T)) return false;
    memcpy(sv.ptr, &value, sizeof(T));
    return _sendVarToAll(sv);
  }

  // --------------------------------------------------------------------------
  //  Lecture locale
  // --------------------------------------------------------------------------
  template<typename T>
  bool getVar(const char* name, T& value) {
    int idx = _findVarByName(name);
    if (idx < 0) return false;
    SharedVar& sv = _vars[idx];
    if (sv.size != sizeof(T)) return false;
    memcpy(&value, sv.ptr, sizeof(T));
    return true;
  }

  // --------------------------------------------------------------------------
  //  Traitement de la file de reception (a appeler dans loop())
  // --------------------------------------------------------------------------
  void update() {
    while (_rxHead != _rxTail) {
      RxPacket& pkt = _rxQueue[_rxTail];
      _processPacket(pkt.mac, pkt.data, pkt.len);
      _rxTail = (_rxTail + 1) % ESV_RX_QUEUE;
    }
  }

  // --------------------------------------------------------------------------
  //  Envoi explicite de toutes les variables
  // --------------------------------------------------------------------------
  bool sendAll() {
    bool ok = true;
    for (int i = 0; i < ESV_MAX_VARS; i++) {
      if (_vars[i].active) {
        if (!_sendVarToAll(_vars[i])) ok = false;
      }
    }
    return ok;
  }

  bool sendTo(const uint8_t* mac) {
    bool ok = true;
    for (int i = 0; i < ESV_MAX_VARS; i++) {
      if (_vars[i].active) {
        if (!_sendVar(_vars[i], mac)) ok = false;
      }
    }
    return ok;
  }

  // --------------------------------------------------------------------------
  //  Heartbeat (a appeler dans loop())
  // --------------------------------------------------------------------------
  void heartbeat(uint16_t interval_ms = 5000) {
    if (millis() - _lastHeartbeat < interval_ms) return;
    _lastHeartbeat = millis();

    uint8_t buf[32];
    uint8_t idx = 0;
    buf[idx++] = TYPE_CTRL;
    buf[idx++] = SUB_HEARTBEAT;
    uint32_t uptime = millis() / 1000;
    buf[idx++] = uptime & 0xFF;
    buf[idx++] = (uptime >> 8) & 0xFF;
    buf[idx++] = (uptime >> 16) & 0xFF;
    buf[idx++] = (uptime >> 24) & 0xFF;
    buf[idx++] = _peerCount;
    uint8_t flags = (_autoDiscovery ? 0x01 : 0x00);
    buf[idx++] = flags;

    _sendRaw(ESV_BROADCAST_MAC, buf, idx);
  }

  // --------------------------------------------------------------------------
  //  Auto-decouverte
  // --------------------------------------------------------------------------
  void enableAutoDiscovery(bool enable = true) {
    _autoDiscovery = enable;
  }

  bool isAutoDiscoveryEnabled() const {
    return _autoDiscovery;
  }

  void discover() {
    uint8_t buf[250];
    uint8_t idx = 0;
    buf[idx++] = TYPE_CTRL;
    buf[idx++] = SUB_DISCOVER;

    uint8_t varCount = 0;
    for (int i = 0; i < ESV_MAX_VARS; i++) {
      if (_vars[i].active) varCount++;
    }
    buf[idx++] = varCount;

    for (int i = 0; i < ESV_MAX_VARS; i++) {
      if (!_vars[i].active) continue;
      if (idx + 1 + _vars[i].name_len > 245) break;
      buf[idx++] = _vars[i].name_len;
      memcpy(&buf[idx], _vars[i].name, _vars[i].name_len);
      idx += _vars[i].name_len;
    }
    _sendRaw(ESV_BROADCAST_MAC, buf, idx);
  }

  // --------------------------------------------------------------------------
  //  Etat des peers
  // --------------------------------------------------------------------------
  bool isPeerOnline(const uint8_t* mac) const {
    int pidx = _findPeerIndex(mac);
    if (pidx < 0) return false;
    return _peerInfo[pidx].online && (millis() - _peerInfo[pidx].lastSeen < _peerTimeout);
  }

  uint8_t getOnlinePeerCount() const {
    uint8_t n = 0;
    for (uint8_t i = 0; i < _peerCount; i++) {
      if (_peerInfo[i].online && (millis() - _peerInfo[i].lastSeen < _peerTimeout)) {
        n++;
      }
    }
    return n;
  }

  void setPeerTimeout(uint32_t ms) {
    _peerTimeout = ms;
  }

  void getPeerMAC(uint8_t index, uint8_t* outMac) const {
    if (index < _peerCount) {
      memcpy(outMac, _peerInfo[index].mac, 6);
    }
  }

  uint8_t peerCount() const { return _peerCount; }

  // --------------------------------------------------------------------------
  //  Callback utilisateur : onReceive(name, mac_source)
  // --------------------------------------------------------------------------
  void onReceive(void (*cb)(const char* name, const uint8_t* mac)) {
    _userCb = cb;
  }

  // --------------------------------------------------------------------------
  //  MAC locale
  // --------------------------------------------------------------------------
  void getLocalMAC(uint8_t* mac) const {
#if defined(ESP8266)
    WiFi.macAddress(mac);
#elif defined(ESP32)
    esp_wifi_get_mac(WIFI_IF_STA, mac);
#endif
  }

private:
  // --------------------------------------------------------------------------
  //  Callbacks reception -- ESP8266
  // --------------------------------------------------------------------------
#if defined(ESP8266)
  static void _onRecv_8266(u8* mac, u8* data, u8 len) {
    if (!_esv_instance) return;
    _esv_instance->_queuePacket(mac, data, len);
  }
  static void _onSent_8266(u8* mac, u8 status) {
    (void)mac; (void)status;
  }
#endif

  // --------------------------------------------------------------------------
  //  Callbacks reception -- ESP32 IDF 4.x (mac, data, len)
  // --------------------------------------------------------------------------
#if defined(ESP32) && ESP_IDF_VERSION_MAJOR < 5
  static void _onRecv_idf4(const uint8_t* mac, const uint8_t* data, int len) {
    if (!_esv_instance) return;
    _esv_instance->_queuePacket(mac, data, (uint8_t)len);
  }
#endif

  // --------------------------------------------------------------------------
  //  Callbacks reception -- ESP32 IDF >= 5 (esp_now_recv_info_t*)
  // --------------------------------------------------------------------------
#if defined(ESP32) && ESP_IDF_VERSION_MAJOR >= 5
  static void _onRecv_idf5plus(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
    if (!_esv_instance || !info) return;
    _esv_instance->_queuePacket(info->src_addr, data, (uint8_t)len);
  }
#endif

  // --------------------------------------------------------------------------
  //  Callback envoi -- ESP32
  // --------------------------------------------------------------------------
#if defined(ESP32)
  static void _onSent_32(const uint8_t* mac, esp_now_send_status_t status) {
    (void)mac; (void)status;
  }
#endif

  // --------------------------------------------------------------------------
  //  File de reception (ISR-safe)
  // --------------------------------------------------------------------------
  void _queuePacket(const uint8_t* mac, const uint8_t* data, uint8_t len) {
    if (len > 250) len = 250;
    uint8_t next = (_rxHead + 1) % ESV_RX_QUEUE;
    if (next == _rxTail) {
      _rxTail = (_rxTail + 1) % ESV_RX_QUEUE;
    }
    RxPacket& pkt = _rxQueue[_rxHead];
    memcpy(pkt.mac, mac, 6);
    memcpy(pkt.data, data, len);
    pkt.len = len;
    _rxHead = next;
  }

  // --------------------------------------------------------------------------
  //  Traitement d'un paquet recu
  // --------------------------------------------------------------------------
  void _processPacket(const uint8_t* mac, const uint8_t* data, uint8_t len) {
    if (len < 2) return;
    uint8_t idx = 0;
    uint8_t type_id = data[idx++];

    // --- Paquet de controle ---
    if (type_id == TYPE_CTRL) {
      uint8_t sub_type = data[idx++];
      switch (sub_type) {
        case SUB_HEARTBEAT:
          _processCtrlHeartbeat(mac, &data[idx], len - idx);
          break;
        case SUB_DISCOVER:
          _processCtrlDiscover(mac, &data[idx], len - idx, false);
          break;
        case SUB_DISCOVER_REPLY:
          _processCtrlDiscover(mac, &data[idx], len - idx, true);
          break;
      }
      return;
    }

    // --- Paquet de donnees ---
    uint16_t struct_id = 0;
    if (type_id == TYPE_STRUCT) {
      if (idx + 2 > len) return;
      struct_id = (data[idx] << 8) | data[idx + 1];
      idx += 2;
    }

    if (idx >= len) return;
    uint8_t name_len = data[idx++];
    if (name_len > ESV_MAX_NAMELEN) return;
    if (idx + name_len + 2 > len) return;

    char name[ESV_MAX_NAMELEN + 1];
    memcpy(name, &data[idx], name_len);
    name[name_len] = '\0';
    idx += name_len;

    uint16_t data_len = data[idx] | (data[idx + 1] << 8);
    idx += 2;
    if (idx + data_len > len) return;

    int vidx = _findVarByName(name);
    if (vidx < 0) return;
    SharedVar& sv = _vars[vidx];
    if (sv.size != data_len) return;
    if (sv.type_id != type_id) return;
    if (type_id == TYPE_STRUCT && sv.struct_id != 0 && struct_id != 0 && sv.struct_id != struct_id) {
      return;  // Mismatch de struct ID
    }

    memcpy(sv.ptr, &data[idx], data_len);

    if (_userCb) {
      _userCb(name, mac);
    }
  }

  // --------------------------------------------------------------------------
  //  Traitement Heartbeat
  // --------------------------------------------------------------------------
  void _processCtrlHeartbeat(const uint8_t* mac, const uint8_t* data, uint8_t len) {
    if (len < 6) return;
    uint32_t uptime = (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
    uint8_t remotePeerCount = data[4];
    uint8_t flags = data[5];
    (void)remotePeerCount;
    (void)flags;

    int pidx = _findPeerIndex(mac);
    if (pidx < 0) {
      if (_autoDiscovery) {
        addPeer(mac);
        pidx = _findPeerIndex(mac);
      } else {
        return;
      }
    }
    if (pidx >= 0) {
      _peerInfo[pidx].lastSeen = millis();
      _peerInfo[pidx].uptime = uptime;
      _peerInfo[pidx].online = true;
    }
  }

  // --------------------------------------------------------------------------
  //  Traitement Discover / Discover Reply
  // --------------------------------------------------------------------------
  void _processCtrlDiscover(const uint8_t* mac, const uint8_t* data, uint8_t len, bool isReply) {
    if (!isReply) {
      // Repondre par un DISCOVER_REPLY en unicast
      uint8_t buf[250];
      uint8_t idx = 0;
      buf[idx++] = TYPE_CTRL;
      buf[idx++] = SUB_DISCOVER_REPLY;
      uint8_t varCount = 0;
      for (int i = 0; i < ESV_MAX_VARS; i++) {
        if (_vars[i].active) varCount++;
      }
      buf[idx++] = varCount;
      for (int i = 0; i < ESV_MAX_VARS; i++) {
        if (!_vars[i].active) continue;
        if (idx + 1 + _vars[i].name_len > 245) break;
        buf[idx++] = _vars[i].name_len;
        memcpy(&buf[idx], _vars[i].name, _vars[i].name_len);
        idx += _vars[i].name_len;
      }
      _sendRaw(mac, buf, idx);
    }

    int pidx = _findPeerIndex(mac);
    if (pidx < 0 && _autoDiscovery) {
      addPeer(mac);
    }
  }

  // --------------------------------------------------------------------------
  //  Serialisation / Envoi
  // --------------------------------------------------------------------------
  bool _serializeVar(const SharedVar& sv, uint8_t* buf, uint8_t& outLen) {
    if (!sv.active || sv.name_len == 0) return false;
    uint8_t idx = 0;
    buf[idx++] = sv.type_id;
    if (sv.type_id == TYPE_STRUCT) {
      buf[idx++] = (sv.struct_id >> 8) & 0xFF;
      buf[idx++] = sv.struct_id & 0xFF;
    }
    buf[idx++] = sv.name_len;
    memcpy(&buf[idx], sv.name, sv.name_len);
    idx += sv.name_len;
    buf[idx++] = sv.size & 0xFF;
    buf[idx++] = (sv.size >> 8) & 0xFF;
    if (idx + sv.size > 250) return false;
    memcpy(&buf[idx], sv.ptr, sv.size);
    idx += sv.size;
    outLen = idx;
    return true;
  }

  bool _sendVar(const SharedVar& sv, const uint8_t* mac) {
    uint8_t buf[250];
    uint8_t len;
    if (!_serializeVar(sv, buf, len)) return false;
    return _sendRaw(mac, buf, len);
  }

  bool _sendVarToAll(const SharedVar& sv) {
    if (_peerCount == 0) {
      return _sendVar(sv, ESV_BROADCAST_MAC);
    }
    bool ok = true;
    for (uint8_t i = 0; i < _peerCount; i++) {
      if (!_sendVar(sv, _peerInfo[i].mac)) ok = false;
    }
    return ok;
  }

  bool _sendRaw(const uint8_t* mac, const uint8_t* data, uint8_t len) {
#if defined(ESP8266)
    return esp_now_send((u8*)mac, (u8*)data, (int)len) == 0;
#elif defined(ESP32)
    return esp_now_send(mac, data, len) == ESP_OK;
#else
    return false;
#endif
  }

  // --------------------------------------------------------------------------
  //  Derivation de cle (passphrase -> 16 octets)
  // --------------------------------------------------------------------------
  void _deriveKey(const char* pass, uint8_t* out16) {
    memset(out16, 0, 16);
    size_t len = strlen(pass);
    for (size_t i = 0; i < len; i++) {
      out16[i % 16] ^= (uint8_t)pass[i];
      out16[i % 16] = (out16[i % 16] << 1) | (out16[i % 16] >> 7);
    }
  }

  // --------------------------------------------------------------------------
  //  Helpers registre
  // --------------------------------------------------------------------------
  int _findVarSlot() {
    for (int i = 0; i < ESV_MAX_VARS; i++) {
      if (!_vars[i].active) return i;
    }
    return -1;
  }

  int _findVarByName(const char* name) {
    for (int i = 0; i < ESV_MAX_VARS; i++) {
      if (_vars[i].active && strncmp(_vars[i].name, name, ESV_MAX_NAMELEN) == 0) return i;
    }
    return -1;
  }

  int _findPeerIndex(const uint8_t* mac) const {
    for (uint8_t i = 0; i < _peerCount; i++) {
      if (memcmp(_peerInfo[i].mac, mac, 6) == 0) return i;
    }
    return -1;
  }

  // --------------------------------------------------------------------------
  //  Membres
  // --------------------------------------------------------------------------
  SharedVar   _vars[ESV_MAX_VARS];
  PeerInfo    _peerInfo[ESV_MAX_PEERS];
  uint8_t     _peerCount;
  uint8_t     _channel;
  uint32_t    _peerTimeout;

  RxPacket    _rxQueue[ESV_RX_QUEUE];
  volatile uint8_t _rxHead;
  volatile uint8_t _rxTail;

  void (*_userCb)(const char* name, const uint8_t* mac);

  // Chiffrement
  uint8_t     _key[16];
  bool        _hasKey;

  // Heartbeat / discovery
  bool        _autoDiscovery;
  uint32_t    _lastHeartbeat;
};

// ============================================================================
//  EXEMPLE D'UTILISATION COMPLET (a copier dans votre main.cpp)
// ============================================================================
/*
#include <EspNowSharedVariable.h>

EspNowSharedVariable esv;

float   temperature = 0.0f;
int32_t compteur    = 0;
bool    ledState    = false;

// Exemple de struct avec verification d'ID
struct SensorData {
  float temp;
  float hum;
  uint16_t co2;
};
SensorData capteur = {0};

void onRecv(const char* name, const uint8_t* mac) {
  Serial.printf("[ESV] Variable '%s' mise a jour par %02X:%02X:%02X:%02X:%02X:%02X\n",
                name, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Optionnel : chiffrement (meme passphrase sur tous les noeuds)
  // esv.setKey("MaPassphraseSecrete123");

  esv.begin(1);  // canal WiFi 1

  // Option 1 : peer manuel
  esv.addPeer("AA:BB:CC:DD:EE:FF");

  // Option 2 : auto-decouverte (les peers s'ajoutent automatiquement)
  esv.enableAutoDiscovery(true);
  esv.discover();  // broadcast de presence au demarrage

  // Enregistrer les variables partagees
  esv.registerVar("temp",  temperature);
  esv.registerVar("count", compteur);
  esv.registerVar("led",   ledState);
  esv.registerVar("sensor", capteur, 0xABCD);  // struct avec ID 0xABCD

  esv.onReceive(onRecv);
}

void loop() {
  // --- Emetteur : simuler une mesure et propager ---
  temperature = 23.5f + random(0, 50) / 10.0f;
  compteur++;
  ledState = !ledState;
  capteur.temp = temperature;
  capteur.hum  = 55.0f;
  capteur.co2  = 420;

  esv.setVar("temp",   temperature);
  esv.setVar("count",  compteur);
  esv.setVar("led",    ledState);
  esv.setVar("sensor", capteur);

  // --- Recepteur : traiter les paquets entrants ---
  esv.update();
  esv.heartbeat(5000);  // envoyer un heartbeat toutes les 5s

  // v1.0.1 : re-arme ESP-NOW apres un eventuel reset WiFi (ex. WiFiManagerESP).
  // A appeler a chaque (re)connexion WiFi (sans effet ici, pas de gestionnaire WiFi).
  esv.rearm();

  // --- Afficher l'etat des peers ---
  Serial.printf("Peers en ligne : %d / %d\n", esv.getOnlinePeerCount(), esv.peerCount());

  delay(2000);
}
*/

#endif // ESPNOW_SHARED_VARIABLE_H
