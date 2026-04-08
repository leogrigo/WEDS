#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include "secrets.h"
// --- INSERISCI QUI I DATI DEL TUO WIFI ---
const char* ssid = secret_wifi;
const char* password = secret_password;

// Inizializza il Web Server sulla porta 80 (standard HTTP)
WebServer server(80);

// Nome del file da scaricare (lo stesso che abbiamo creato nell'altro sketch)
// const char* filename = "/dati_sensori.csv";
const char* filename = "/dati_bme680.csv";


// --- FUNZIONI DEL SERVER WEB ---

// 1. Pagina principale (HTML)
void handleRoot() {
  String html = "<!DOCTYPE html><html><head><title>Heltec Data Logger</title>";
  html += "<style>body{font-family: Arial, sans-serif; text-align: center; margin-top: 50px;} ";
  html += ".btn{background-color: #4CAF50; color: white; padding: 15px 32px; text-decoration: none; font-size: 16px; border-radius: 8px;}";
  html += "</style></head><body>";
  html += "<h1>Heltec ESP32-S3 Data Logger</h1>";
  
  if (LittleFS.exists(filename)) {
    html += "<p>Il file CSV e' pronto per il download.</p>";
    html += "<br><a class=\"btn\" href=\"/download\">Scarica dati_sensori.csv</a>";
  } else {
    html += "<p style=\"color:red;\"><strong>Errore:</strong> Nessun file trovato nella memoria. Assicurati di aver prima fatto girare il programma di logging.</p>";
  }
  
  html += "</body></html>";
  server.send(200, "text/html", html);
}

// 2. Gestione del download del file
void handleDownload() {
  if (!LittleFS.exists(filename)) {
    server.send(404, "text/plain", "File non trovato.");
    return;
  }

  File file = LittleFS.open(filename, "r");
  if (!file) {
    server.send(500, "text/plain", "Errore nell'apertura del file.");
    return;
  }

  // Diciamo al browser di trattarlo come file da scaricare (allegato)
  server.sendHeader("Content-Disposition", "attachment; filename=\"dati_sensori.csv\"");
  server.sendHeader("Connection", "close");
  
  // Stream del file: manda i dati a blocchi al client in modo efficiente (non carica tutto in RAM!)
  size_t sentBytes = server.streamFile(file, "text/csv");
  file.close();

  Serial.print("File inviato con successo. Byte inviati: ");
  Serial.println(sentBytes);
}

void setup() {
  Serial.begin(115200);
  delay(1000); // Pausa per far avviare il monitor seriale

  Serial.println("\n--- Avvio Modalita' Estrazione Dati ---");

  // 1. Monta LittleFS
  if (!LittleFS.begin(true)) {
    Serial.println("Errore nel mount di LittleFS!");
    return;
  }
  Serial.println("LittleFS montato correttamente.");

  // 2. Connessione al WiFi
  Serial.print("Connessione a: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConnesso al WiFi!");
  Serial.print("Indirizzo IP per scaricare il file: ");
  Serial.println(WiFi.localIP()); // IMPORTANTE: Copia questo IP nel browser

  // 3. Configurazione Endpoint Server
  server.on("/", handleRoot);
  server.on("/download", HTTP_GET, handleDownload);

  // 4. Avvia il server
  server.begin();
  Serial.println("Server Web HTTP avviato.");
}

void loop() {
  // FreeRTOS è in esecuzione in background, il loop gestisce le richieste in entrata dei client
  server.handleClient();
}