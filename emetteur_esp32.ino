#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>
#include "Adafruit_SHT31.h"

// Structure du message
typedef struct struct_message {
  float temperature;
  float humidite;
  unsigned long timestamp;
} struct_message;

struct_message myData;
Adafruit_SHT31 sht31 = Adafruit_SHT31();

// Adresse MAC du récepteur
uint8_t broadcastAddress[] = {0x00, 0x4B, 0x12, 0x3C, 0x6B, 0x80};

esp_now_peer_info_t peerInfo;

// Variables pour statistiques
uint32_t messageCount = 0;
uint32_t successCount = 0;
uint32_t failCount = 0;

// Intervalle d'envoi (1 minute = 60000 ms)
const unsigned long SEND_INTERVAL = 60000;

// Callback d'envoi (sans LED)
void OnDataSent(const wifi_tx_info_t *mac_addr, esp_now_send_status_t status) {
  messageCount++;

  if (status == ESP_NOW_SEND_SUCCESS) {
    successCount++;
    Serial.printf("[CALLBACK] Message #%d : Envoi réussi\n", messageCount);
  } else {
    failCount++;
    Serial.printf("[CALLBACK] Message #%d : Échec d'envoi\n", messageCount);
  }

  // Afficher statistiques toutes les 5 transmissions
  if (messageCount % 5 == 0) {
    float rate = (float)successCount / messageCount * 100.0;
    Serial.printf("[STATS] %d/%d messages réussis (%.1f%%)\n",
                  successCount, messageCount, rate);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n========================================");
  Serial.println("   EMETTEUR ESP32 + SHT31 (ESP-NOW)");
  Serial.println("========================================\n");

  // 1. Initialisation Wi-Fi mode station
  Serial.println("[INIT] Configuration WiFi mode Station...");
  WiFi.mode(WIFI_STA);
  Serial.print("[INIT] Adresse MAC : ");
  Serial.println(WiFi.macAddress());

  // 2. Initialisation ESP-NOW
  Serial.println("[INIT] Initialisation ESP-NOW...");
  if (esp_now_init() != ESP_OK) {
    Serial.println("[ERREUR] Echec initialisation ESP-NOW");
    return;
  }

  esp_now_register_send_cb(OnDataSent);
  Serial.println("[INIT] ESP-NOW initialisé");

  // 3. Ajout du récepteur
  Serial.println("[INIT] Ajout du récepteur...");
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 1;  // canal fixe
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("[ERREUR] Echec ajout du récepteur");
    return;
  }

  Serial.print("[INIT] Récepteur ajouté - MAC : ");
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02x", broadcastAddress[i]);
    if (i < 5) Serial.print(":");
  }
  Serial.println();

  // 4. Détection et initialisation SHT31
  Serial.println("[INIT] Détection capteur SHT31...");
  if (!sht31.begin(0x44)) {
    Serial.println("[ERREUR] SHT31 non détecté - ARRÊT DU SYSTÈME");
    while (1) delay(10);
  }
  Serial.println("[INIT] SHT31 initialisé avec succès");

  Serial.println("\n========================================");
  Serial.println("   SYSTÈME OPÉRATIONNEL");
  Serial.printf("   Intervalle: %d seconde(s)\n", SEND_INTERVAL / 1000);
  Serial.println("========================================\n");
}

void loop() {
  Serial.println("\n--- CYCLE D'ACQUISITION ---");

  // Lecture température et humidité
  float t = sht31.readTemperature();
  float h = sht31.readHumidity();

  if (!isnan(t) && !isnan(h)) {
    // Création du message structuré avec horodatage
    myData.temperature = t;
    myData.humidite = h;
    myData.timestamp = millis();

    // Affichage des données
    Serial.printf("[DONNÉES] Température: %.1f°C\n", t);
    Serial.printf("[DONNÉES] Humidité: %.1f%%\n", h);
    Serial.printf("[DONNÉES] Timestamp: %lu ms\n", myData.timestamp);

    // Envoi via ESP-NOW
    Serial.println("[ENVOI] Transmission ESP-NOW...");
    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *)&myData, sizeof(myData));

    if (result == ESP_OK) {
      Serial.println("[ENVOI] Requête acceptée");
    } else {
      Serial.printf("[ENVOI] Erreur: %d\n", result);
    }
  } else {
    Serial.println("[ERREUR] Échec lecture capteur SHT31");
  }

  Serial.println("------------------------------------------");

  // Attente avant prochain cycle (1 minute)
  Serial.println("[ATTENTE] Prochain envoi dans 1 minute...\n");
  delay(SEND_INTERVAL);
}