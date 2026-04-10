#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

#include "lora_gateway.hpp"
#include "secrets.h"

namespace {

WebServer server(80);

constexpr size_t MAX_TRACKED_NODES = 16;

struct NodeStatusEntry {
    bool used;
    String nodeId;
    String payloadJson;
};

NodeStatusEntry nodeStatuses[MAX_TRACKED_NODES] = {};

struct ParsedPayload {
    String nodeId;
    String json;
};

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

String trimCopy(const String& input) {
    String out = input;
    out.trim();
    return out;
}

ParsedPayload parsePayloadToJson(const String& payload, float rssi, float snr, uint32_t updatedMs) {
    String json = "{";
    bool hasFields = false;
    int start = 0;
    String nodeId = "";

    while (start < payload.length()) {
        int comma = payload.indexOf(',', start);
        if (comma < 0) {
            comma = payload.length();
        }

        String token = trimCopy(payload.substring(start, comma));
        int sep = token.indexOf(':');
        if (sep > 0 && sep < token.length() - 1) {
            String key = trimCopy(token.substring(0, sep));
            String value = trimCopy(token.substring(sep + 1));
            String outputKey = key;

            if (key == "id") {
                outputKey = "node_id";
            }
            if (key == "n_sample") {
                outputKey = "samples";
            }
            if (key == "an_state") {
                outputKey = "state";
            }
            if (key == "an_score") {
                outputKey = "anomaly_score";
            }

            if (outputKey == "node_id" && nodeId.length() == 0) {
                nodeId = value;
            }

            if (hasFields) {
                json += ",";
            }

            json += "\"";
            json += escapeJson(outputKey);
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

    if (nodeId.length() == 0) {
        nodeId = "unknown";
        json += "\"node_id\":\"unknown\",";
    }

    json += "\"rssi\":\"";
    json += String(rssi, 2);
    json += "\",\"snr\":\"";
    json += String(snr, 2);
    json += "\",\"updated_ms\":\"";
    json += String(updatedMs);
    json += "\"}";

    return ParsedPayload{nodeId, json};
}

void updateNodeStatuses(const String& nodeId, const String& payloadJson) {
    size_t freeIndex = MAX_TRACKED_NODES;

    for (size_t i = 0; i < MAX_TRACKED_NODES; ++i) {
        if (nodeStatuses[i].used && nodeStatuses[i].nodeId == nodeId) {
            nodeStatuses[i].payloadJson = payloadJson;
            return;
        }

        if (!nodeStatuses[i].used && freeIndex == MAX_TRACKED_NODES) {
            freeIndex = i;
        }
    }

    if (freeIndex < MAX_TRACKED_NODES) {
        nodeStatuses[freeIndex].used = true;
        nodeStatuses[freeIndex].nodeId = nodeId;
        nodeStatuses[freeIndex].payloadJson = payloadJson;
        return;
    }

    Serial.println("Node status table full, cannot track more nodes.");
}

String buildNodeStatusListJson() {
    String json = "[";
    bool first = true;

    for (size_t i = 0; i < MAX_TRACKED_NODES; ++i) {
        if (!nodeStatuses[i].used) {
            continue;
        }

        if (!first) {
            json += ",";
        }

        json += nodeStatuses[i].payloadJson;
        first = false;
    }

    json += "]";
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
    server.send(200, "application/json", buildNodeStatusListJson());
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
        uint32_t lastUpdateMs = millis();
        ParsedPayload parsedPayload = parsePayloadToJson(packet.payload, packet.rssi, packet.snr, lastUpdateMs);
        updateNodeStatuses(parsedPayload.nodeId, parsedPayload.json);

        Serial.print("LoRa RX <- ");
        Serial.println(packet.payload);
        Serial.print("Updated node_id: ");
        Serial.println(parsedPayload.nodeId);
        Serial.print("Gateway API /node_status -> ");
        Serial.println(buildNodeStatusListJson());
    }
}
