#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

#include "lora_gateway.hpp"
#include "secrets.h"

namespace {

WebServer server(80);

String lastNodeStatus = "{}";
String lastRawPayload = "";
float lastRssi = 0.0f;
float lastSnr = 0.0f;
uint32_t lastUpdateMs = 0;

String escapeJson(const String& input) {
    String out;
    out.reserve(input.length() + 8);

    for (size_t i = 0; i < input.length(); ++i) {
        char c = input[i];
        if (c == '\"' || c == '\\') {
            out += '\\';
        }
        out += c;
    }

    return out;
}

String payloadToJson(const String& payload, float rssi, float snr, uint32_t updatedMs) {
    String json = "{";
    bool hasFields = false;
    int start = 0;

    while (start < payload.length()) {
        int comma = payload.indexOf(',', start);
        if (comma < 0) {
            comma = payload.length();
        }

        String token = payload.substring(start, comma);
        int eq = token.indexOf('=');
        if (eq > 0 && eq < token.length() - 1) {
            String key = token.substring(0, eq);
            String value = token.substring(eq + 1);

            if (hasFields) {
                json += ",";
            }

            json += "\"";
            json += escapeJson(key);
            json += "\":\"";
            json += escapeJson(value);
            json += "\"";
            hasFields = true;
        }

        start = comma + 1;
    }

    if (hasFields) {
        json += ",";
    }

    json += "\"raw\":\"";
    json += escapeJson(payload);
    json += "\",\"rssi\":\"";
    json += String(rssi, 2);
    json += "\",\"snr\":\"";
    json += String(snr, 2);
    json += "\",\"updated_ms\":\"";
    json += String(updatedMs);
    json += "\"}";

    return json;
}

void handleRoot() {
    String html = "<!DOCTYPE html><html><head><title>LoRa Gateway</title></head><body>";
    html += "<h1>LoRa Gateway</h1>";
    html += "<p>Endpoint disponibile: <a href=\"/node_status\">/node_status</a></p>";
    html += "</body></html>";
    server.send(200, "text/html", html);
}

void handleNodeStatus() {
    server.send(200, "application/json", lastNodeStatus);
}

void connectWifi() {
    Serial.print("Connecting to WiFi ");
    Serial.println(secret_wifi);

    WiFi.mode(WIFI_STA);
    WiFi.begin(secret_wifi, secret_password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println();
    Serial.print("WiFi connected, IP: ");
    Serial.println(WiFi.localIP());
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("Minimal LoRa gateway boot");
    connectWifi();

    server.on("/", handleRoot);
    server.on("/node_status", HTTP_GET, handleNodeStatus);
    server.begin();
    Serial.println("HTTP server ready");

    LoraGateway::begin();
}

void loop() {
    server.handleClient();

    LoraGateway::ReceivedPacket packet;
    if (LoraGateway::pollReceive(packet)) {
        lastRawPayload = packet.payload;
        lastRssi = packet.rssi;
        lastSnr = packet.snr;
        lastUpdateMs = millis();
        lastNodeStatus = payloadToJson(packet.payload, packet.rssi, packet.snr, lastUpdateMs);

        Serial.print("LoRa RX <- ");
        Serial.println(packet.payload);
        Serial.print("Gateway API /node_status -> ");
        Serial.println(lastNodeStatus);
    }
}
