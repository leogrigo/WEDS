#include "WedsGatewayApi.h"

#include <ArduinoJson.h>
#include <stdlib.h>
#include <time.h>

#include "WedsGatewayConfig.h"
#include "WedsGatewayWeb.h"

/** @brief Buffer for API operations retrieving node records */
static WedsNodeRecord api_record_buffer[WEDS_MAX_NODES];

/** @brief Buffer for API operations retrieving node events */
static WedsNodeEvent api_event_buffer[WEDS_MAX_EVENTS_PER_NODE];

/** @brief Buffer for API operations retrieving node trend data */
static WedsTrendPoint api_trend_buffer[WEDS_TREND_POINTS_PER_NODE];

WedsGatewayApi::WedsGatewayApi()
    : registry_(nullptr),
      server_(WEDS_HTTP_PORT),
      initialized_(false) {}

bool WedsGatewayApi::begin(
    const char* ssid,
    const char* password,
    WedsGatewayRegistry* registry,
    uint16_t port
) {
    if (registry == nullptr) {
        Serial.println("[GATEWAY_API] begin failed: registry is null");
        return false;
    }

    if (ssid == nullptr || password == nullptr) {
        Serial.println("[GATEWAY_API] begin failed: WiFi credentials are null");
        return false;
    }

    registry_ = registry;

    // v0.1 keeps WebServer fixed at port 80 because WebServer wants its port
    // in the constructor. The parameter is kept for the future API shape.
    if (port != WEDS_HTTP_PORT) {
        Serial.println("[GATEWAY_API] Custom port ignored in v0.1; using configured port");
    }

    Serial.print("[GATEWAY_API] Connecting WiFi SSID=");
    Serial.println(ssid);

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    const uint32_t start_ms = millis();
    uint32_t last_print_ms = 0;

    while (WiFi.status() != WL_CONNECTED &&
           millis() - start_ms < WEDS_WIFI_CONNECT_TIMEOUT_MS) {
        delay(50);

        if (millis() - last_print_ms >= WEDS_WIFI_CONNECT_PRINT_INTERVAL_MS) {
            last_print_ms = millis();
            Serial.print(".");
        }
    }

    Serial.println();

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[GATEWAY_API] WiFi connection failed");
        initialized_ = false;
        return false;
    }

    Serial.print("[GATEWAY_API] WiFi connected, IP=");
    Serial.println(WiFi.localIP());

    WiFi.setSleep(WEDS_WIFI_MODEM_SLEEP_ENABLED);
    Serial.print("[GATEWAY_API] WiFi modem sleep=");
    Serial.println(WEDS_WIFI_MODEM_SLEEP_ENABLED ? "enabled" : "disabled");

    syncClock();

    setupRoutes();
    server_.begin();

    initialized_ = true;
    Serial.println("[GATEWAY_API] HTTP server started");

    return true;
}

void WedsGatewayApi::handleClient() {
    if (initialized_) {
        server_.handleClient();
    }
}

IPAddress WedsGatewayApi::localIP() const {
    return WiFi.localIP();
}

void WedsGatewayApi::syncClock() {
    Serial.println("[GATEWAY_API] Syncing clock with NTP...");

    configTime(0, 0, WEDS_NTP_SERVER_1, WEDS_NTP_SERVER_2);

    const uint32_t start_ms = millis();

    while (millis() - start_ms < WEDS_NTP_SYNC_TIMEOUT_MS) {
        const time_t now = time(nullptr);

        if (now >= static_cast<time_t>(WEDS_MIN_VALID_EPOCH_S)) {
            Serial.print("[GATEWAY_API] NTP time synced, epoch_s=");
            Serial.println(static_cast<uint32_t>(now));
            return;
        }

        delay(WEDS_NTP_SYNC_POLL_MS);
    }

    Serial.println("[GATEWAY_API] NTP sync timeout, using node timestamps until time is available");
}

void WedsGatewayApi::setupRoutes() {
    server_.on("/", HTTP_GET, [this]() {
        server_.send_P(200, "text/html", WedsGatewayWeb::indexHtml());
    });

    server_.on("/admin", HTTP_GET, [this]() {
        server_.send_P(200, "text/html", WedsGatewayWeb::adminHtml());
    });

    server_.on("/api", HTTP_GET, [this]() {
        handleRootInfo();
    });

    server_.on("/api/state/all", HTTP_GET, [this]() {
        handleGetStateAll();
    });

    server_.on("/api/state", HTTP_GET, [this]() {
        handleGetNodeState();
    });

    server_.on("/api/nodes/unlocated", HTTP_GET, [this]() {
        handleGetUnlocatedNodes();
    });

    server_.on("/api/node/events", HTTP_GET, [this]() {
        handleGetNodeEvents();
    });

    server_.on("/api/node/trend", HTTP_GET, [this]() {
        handleGetNodeTrend();
    });

    server_.on("/api/admin/setlocation", HTTP_POST, [this]() {
        handleSetLocation();
    });

    server_.on("/api/admin/clearconfig", HTTP_POST, [this]() {
        handleClearConfig();
    });

    server_.onNotFound([this]() {
        sendError(404, "not_found", "route not found");
    });
}

void WedsGatewayApi::handleRootInfo() {
    JsonDocument doc;

    doc["name"] = "WEDS Gateway API";
    doc["status"] = "ok";
    doc["known_nodes"] = registry_->getKnownNodeCount();

    String json;
    serializeJson(doc, json);
    sendJson(200, json);
}

void WedsGatewayApi::handleGetStateAll() {
    const size_t count = registry_->getAllNodes(api_record_buffer, WEDS_MAX_NODES);

    JsonDocument doc;
    JsonArray nodes = doc.to<JsonArray>();

    for (size_t i = 0; i < count; ++i) {
        JsonObject node = nodes.add<JsonObject>();
        appendNodeRecord(node, api_record_buffer[i]);
    }

    String json;
    serializeJson(doc, json);
    sendJson(200, json);
}

void WedsGatewayApi::handleGetNodeState() {
    uint32_t node_id = 0;

    if (!parseNodeIdArg(node_id)) {
        return;
    }

    WedsNodeRecord& record = api_record_buffer[0];

    if (!registry_->getNodeState(node_id, record)) {
        sendError(404, "node_not_found", "node_id was not found");
        return;
    }

    JsonDocument doc;
    JsonObject node = doc.to<JsonObject>();
    appendNodeRecord(node, record);

    String json;
    serializeJson(doc, json);
    sendJson(200, json);
}

void WedsGatewayApi::handleGetUnlocatedNodes() {
    const size_t count = registry_->getUnlocatedNodes(
        api_record_buffer,
        WEDS_MAX_NODES
    );

    JsonDocument doc;
    JsonArray nodes = doc.to<JsonArray>();

    for (size_t i = 0; i < count; ++i) {
        JsonObject node = nodes.add<JsonObject>();
        node["node_id"] = api_record_buffer[i].node_id;
        node["last_seen_ms"] = api_record_buffer[i].last_seen_ms;
        node["last_seen_timestamp_s"] = api_record_buffer[i].last_seen_timestamp_s;
    }

    String json;
    serializeJson(doc, json);
    sendJson(200, json);
}

void WedsGatewayApi::handleGetNodeEvents() {
    uint32_t node_id = 0;

    if (!parseNodeIdArg(node_id)) {
        return;
    }

    const size_t count = registry_->getNodeEvents(
        node_id,
        api_event_buffer,
        WEDS_MAX_EVENTS_PER_NODE
    );

    JsonDocument doc;
    doc["node_id"] = node_id;
    JsonArray items = doc["events"].to<JsonArray>();

    for (size_t i = 0; i < count; ++i) {
        JsonObject event = items.add<JsonObject>();
        event["event_id"] = api_event_buffer[i].event_id;
        event["type"] = static_cast<uint8_t>(api_event_buffer[i].type);
        event["type_label"] = eventTypeToString(api_event_buffer[i].type);
        event["start_timestamp_s"] = api_event_buffer[i].start_timestamp_s;
        event["end_timestamp_s"] = api_event_buffer[i].end_timestamp_s;
        event["still_open"] = api_event_buffer[i].still_open;
        event["peak_anomaly_score"] = api_event_buffer[i].peak_anomaly_score;
        event["peak_risk_score"] = api_event_buffer[i].peak_risk_score;
        event["max_temperature"] = api_event_buffer[i].max_temperature;
        event["min_humidity"] = api_event_buffer[i].min_humidity;
        event["min_gas_resistance"] = api_event_buffer[i].min_gas_resistance;
        event["sample_count"] = api_event_buffer[i].sample_count;
    }

    String json;
    serializeJson(doc, json);
    sendJson(200, json);
}

void WedsGatewayApi::handleGetNodeTrend() {
    uint32_t node_id = 0;

    if (!parseNodeIdArg(node_id)) {
        return;
    }

    const size_t count = registry_->getNodeTrend(
        node_id,
        api_trend_buffer,
        WEDS_TREND_POINTS_PER_NODE
    );

    JsonDocument doc;
    doc["node_id"] = node_id;
    JsonArray items = doc["points"].to<JsonArray>();

    for (size_t i = 0; i < count; ++i) {
        JsonObject point = items.add<JsonObject>();
        point["timestamp_s"] = api_trend_buffer[i].timestamp_s;
        point["temperature"] = api_trend_buffer[i].temperature;
        point["humidity"] = api_trend_buffer[i].humidity;
        point["pressure"] = api_trend_buffer[i].pressure;
        point["gas_resistance"] = api_trend_buffer[i].gas_resistance;
        point["battery_level"] = api_trend_buffer[i].battery_level;
        point["anomaly_score"] = api_trend_buffer[i].anomaly_score;
        point["risk_score"] = api_trend_buffer[i].risk_score;
    }

    String json;
    serializeJson(doc, json);
    sendJson(200, json);
}

void WedsGatewayApi::handleSetLocation() {
    const String body = server_.arg("plain");

    if (body.length() == 0) {
        sendError(400, "empty_body", "JSON request body is required");
        return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);

    if (error) {
        sendError(400, "invalid_json", "request body is not valid JSON");
        return;
    }

    if (!doc["node_id"].is<uint32_t>()) {
        sendError(400, "missing_node_id", "node_id is required");
        return;
    }

    if (!doc["latitude"].is<double>()) {
        sendError(400, "missing_latitude", "latitude is required");
        return;
    }

    if (!doc["longitude"].is<double>()) {
        sendError(400, "missing_longitude", "longitude is required");
        return;
    }

    const uint32_t node_id = doc["node_id"].as<uint32_t>();
    const double latitude = doc["latitude"].as<double>();
    const double longitude = doc["longitude"].as<double>();

    if (node_id == 0) {
        sendError(400, "invalid_node_id", "node_id must be non-zero");
        return;
    }

    if (latitude < -90.0 || latitude > 90.0) {
        sendError(400, "invalid_latitude", "latitude must be between -90 and 90");
        return;
    }

    if (longitude < -180.0 || longitude > 180.0) {
        sendError(
            400,
            "invalid_longitude",
            "longitude must be between -180 and 180"
        );
        return;
    }

    if (!registry_->setNodeLocation(node_id, latitude, longitude)) {
        sendError(500, "registry_full", "failed to store node location");
        return;
    }

    if (!registry_->savePersistentConfig()) {
        sendError(500, "save_failed", "failed to save persistent config");
        return;
    }

    JsonDocument response;
    response["status"] = "ok";
    response["node_id"] = node_id;
    response["location_known"] = true;
    response["latitude"] = latitude;
    response["longitude"] = longitude;

    String json;
    serializeJson(response, json);
    sendJson(200, json);
}

void WedsGatewayApi::handleClearConfig() {
    if (!registry_->clearPersistentConfig()) {
        sendError(500, "clear_failed", "failed to clear persistent config");
        return;
    }

    JsonDocument doc;
    doc["status"] = "ok";
    doc["message"] = "persistent config cleared";

    String json;
    serializeJson(doc, json);
    sendJson(200, json);
}

void WedsGatewayApi::sendJson(int status_code, const String& json) {
    server_.send(status_code, "application/json", json);
}

void WedsGatewayApi::sendError(
    int status_code,
    const char* error_code,
    const char* message
) {
    JsonDocument doc;
    doc["error"] = error_code;
    doc["message"] = message;

    String json;
    serializeJson(doc, json);
    sendJson(status_code, json);
}

String WedsGatewayApi::detectionStateToString(uint8_t state) const {
    switch (state) {
        case WEDS_DETECTION_NORMAL:
            return "NORMAL";
        case WEDS_DETECTION_ALERT:
            return "ALERT";
        default:
            return "UNKNOWN";
    }
}

String WedsGatewayApi::eventTypeToString(WedsEventType type) const {
    switch (type) {
        case WEDS_EVENT_NONE:
            return "NONE";
        case WEDS_EVENT_ANOMALY_ALERT:
            return "ANOMALY_ALERT";
        case WEDS_EVENT_RISK_ALERT:
            return "RISK_ALERT";
        case WEDS_EVENT_BOTH_ALERT:
            return "BOTH_ALERT";
        default:
            return "UNKNOWN";
    }
}

bool WedsGatewayApi::parseNodeIdArg(uint32_t& out_node_id) {
    if (!server_.hasArg("node_id")) {
        sendError(
            400,
            "missing_node_id",
            "node_id query parameter is required"
        );
        return false;
    }

    const String value = server_.arg("node_id");
    char* end_ptr = nullptr;
    const unsigned long parsed = strtoul(value.c_str(), &end_ptr, 10);

    if (value.length() == 0 || end_ptr == value.c_str() ||
        *end_ptr != '\0' || parsed == 0) {
        sendError(400, "invalid_node_id", "node_id must be a non-zero integer");
        return false;
    }

    out_node_id = static_cast<uint32_t>(parsed);
    return true;
}

void WedsGatewayApi::appendNodeRecord(
    JsonObject obj,
    const WedsNodeRecord& record
) {
    const WedsNodeStatusPayload& status = record.latest_status;

    obj["node_id"] = record.node_id;
    obj["last_seen_ms"] = record.last_seen_ms;
    obj["last_seen_timestamp_s"] = record.last_seen_timestamp_s;
    obj["last_sequence_id"] = record.last_sequence_id;
    obj["last_msg_type"] = record.last_msg_type;

    obj["temperature"] = status.temperature;
    obj["humidity"] = status.humidity;
    obj["pressure"] = status.pressure;
    obj["gas_resistance"] = status.gas_resistance;
    obj["battery_level"] = status.battery_level;

    obj["anomaly_state"] = status.anomaly_state;
    obj["anomaly_state_label"] = detectionStateToString(status.anomaly_state);
    obj["anomaly_score"] = status.anomaly_score;
    obj["risk_state"] = status.risk_state;
    obj["risk_state_label"] = detectionStateToString(status.risk_state);
    obj["risk_score"] = status.risk_score;

    obj["location_known"] = record.location_known;
    obj["latitude"] = record.latitude;
    obj["longitude"] = record.longitude;

    obj["pending_alert_mode"] = record.pending_alert_mode;
    obj["streak_open"] = record.streak_open;
    obj["trend_count"] = record.trend_count;
    obj["event_count"] = record.event_count;
}




