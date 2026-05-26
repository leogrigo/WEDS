#include "WedsGatewayMqtt.h"

#include <ArduinoJson.h>
#include <WiFi.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "WedsGatewayConfig.h"

namespace {

constexpr const char* WEDS_COMMANDS_TOPIC = "weds/gateway/commands";
constexpr const char* WEDS_RESPONSES_TOPIC = "weds/gateway/command_responses";
constexpr const char* WEDS_GATEWAY_STATUS_TOPIC = "weds/gateway/status";
constexpr const char* WEDS_STATE_TOPIC_PREFIX = "weds/nodes/";
constexpr const char* WEDS_STATE_TOPIC_SUFFIX = "/state";

WedsGatewayMqtt* active_instance = nullptr;

void forwardingCallback(char* topic, uint8_t* payload, unsigned int length) {
    if (active_instance != nullptr) {
        active_instance->handleMessage(topic, payload, length);
    }
}

}  // namespace

WedsGatewayMqtt::WedsGatewayMqtt()
    : mqtt_(wifi_client_),
      host_(nullptr),
      port_(1883),
      registry_(nullptr),
      gateway_comm_(nullptr),
      initialized_(false),
      last_reconnect_attempt_ms_(0),
      last_status_publish_ms_(0) {
    memset(client_id_, 0, sizeof(client_id_));
    memset(node_states_, 0, sizeof(node_states_));
    memset(publish_records_, 0, sizeof(publish_records_));
}

bool WedsGatewayMqtt::begin(
    const char* host,
    uint16_t port,
    WedsGatewayRegistry* registry,
    WedsGatewayComm* gateway_comm
) {
    if (host == nullptr || registry == nullptr) {
        Serial.println("[GATEWAY_MQTT] begin failed: invalid configuration");
        initialized_ = false;
        return false;
    }

    host_ = host;
    port_ = port;
    registry_ = registry;
    gateway_comm_ = gateway_comm;
    active_instance = this;

    mqtt_.setServer(host_, port_);
    mqtt_.setCallback(forwardingCallback);
    mqtt_.setBufferSize(WEDS_GATEWAY_MQTT_PACKET_SIZE);
    mqtt_.setKeepAlive(30);
    mqtt_.setSocketTimeout(WEDS_GATEWAY_MQTT_SOCKET_TIMEOUT_SEC);

    initialized_ = true;

    Serial.print("[GATEWAY_MQTT] Configured broker=");
    Serial.print(host_);
    Serial.print(" port=");
    Serial.println(port_);

    if (!credentialsConfigured()) {
        Serial.println("[GATEWAY_MQTT] MQTT host is empty; MQTT will stay idle");
    }

    return true;
}

void WedsGatewayMqtt::loop() {
    if (!initialized_ || !credentialsConfigured()) {
        return;
    }

    if (!ensureConnected()) {
        return;
    }

    mqtt_.loop();
    publishGatewayStatusIfDue();
    publishKnownNodes();
}

bool WedsGatewayMqtt::ensureConnected() {
    if (mqtt_.connected()) {
        return true;
    }

    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }

    const uint32_t now_ms = millis();

    if (now_ms - last_reconnect_attempt_ms_ <
        WEDS_GATEWAY_MQTT_RECONNECT_INTERVAL_MS) {
        return false;
    }

    last_reconnect_attempt_ms_ = now_ms;
    return connectMqtt();
}

bool WedsGatewayMqtt::connectMqtt() {
    snprintf(
        client_id_,
        sizeof(client_id_),
        "weds-gateway-%08lx",
        static_cast<unsigned long>(ESP.getEfuseMac() & 0xffffffffUL)
    );

    Serial.print("[GATEWAY_MQTT] Connecting to broker as ");
    Serial.println(client_id_);

    JsonDocument offline_doc;
    offline_doc["gateway_id"] = client_id_;
    offline_doc["online"] = false;
    offline_doc["published_at_s"] = time(nullptr);

    String offline_payload;
    serializeJson(offline_doc, offline_payload);

    vTaskDelay(pdMS_TO_TICKS(1));
    const bool connected = mqtt_.connect(
        client_id_,
        WEDS_GATEWAY_STATUS_TOPIC,
        0,
        true,
        offline_payload.c_str()
    );
    vTaskDelay(pdMS_TO_TICKS(1));

    if (!connected) {
        Serial.print("[GATEWAY_MQTT] Connect failed, state=");
        Serial.println(mqtt_.state());
        return false;
    }

    Serial.println("[GATEWAY_MQTT] Connected");
    subscribeTopics();
    publishGatewayStatus(true);
    last_status_publish_ms_ = millis();

    for (size_t i = 0; i < WEDS_MAX_NODES; ++i) {
        node_states_[i].telemetry_published = false;
    }

    return true;
}

void WedsGatewayMqtt::subscribeTopics() {
    if (mqtt_.subscribe(WEDS_COMMANDS_TOPIC)) {
        Serial.println("[GATEWAY_MQTT] Subscribed to WEDS commands");
        return;
    }

    Serial.println("[GATEWAY_MQTT] Failed to subscribe to WEDS commands");
}

void WedsGatewayMqtt::publishGatewayStatus(bool online) {
    if (!mqtt_.connected()) {
        return;
    }

    JsonDocument doc;
    doc["gateway_id"] = client_id_;
    doc["online"] = online;
    doc["ip"] = WiFi.localIP().toString();
    doc["known_nodes"] = registry_ != nullptr ? registry_->getKnownNodeCount() : 0;
    doc["uptime_ms"] = millis();
    doc["firmware"] = "weds-gateway-mqtt";
    doc["published_at_s"] = time(nullptr);

    String payload;
    serializeJson(doc, payload);

    const bool ok = mqtt_.publish(WEDS_GATEWAY_STATUS_TOPIC, payload.c_str(), true);
    if (!ok) {
        Serial.println("[GATEWAY_MQTT] gateway status publish failed");
    }
}

void WedsGatewayMqtt::publishGatewayStatusIfDue() {
    const uint32_t now_ms = millis();
    if (last_status_publish_ms_ != 0 &&
        now_ms - last_status_publish_ms_ < WEDS_GATEWAY_MQTT_STATUS_INTERVAL_MS) {
        return;
    }

    publishGatewayStatus(true);
    last_status_publish_ms_ = now_ms;
}

void WedsGatewayMqtt::publishKnownNodes() {
    if (registry_ == nullptr) {
        return;
    }

    const size_t count = registry_->getAllNodes(
        publish_records_,
        WEDS_MAX_NODES
    );

    for (size_t i = 0; i < count; ++i) {
        WedsNodeRecord& record = publish_records_[i];
        NodePublishState* state = stateForNode(record.node_id);

        if (state == nullptr) {
            continue;
        }

        const bool sequence_changed =
            !state->telemetry_published ||
            state->last_sequence_id != record.last_sequence_id ||
            state->last_msg_type != record.last_msg_type;

        if (!sequence_changed) {
            continue;
        }

        if (publishNodeTelemetry(record)) {
            state->last_sequence_id = record.last_sequence_id;
            state->last_msg_type = record.last_msg_type;
            state->telemetry_published = true;
        }
    }
}

bool WedsGatewayMqtt::publishNodeTelemetry(const WedsNodeRecord& record) {
    char topic[48];
    snprintf(
        topic,
        sizeof(topic),
        "%s%lu%s",
        WEDS_STATE_TOPIC_PREFIX,
        static_cast<unsigned long>(record.node_id),
        WEDS_STATE_TOPIC_SUFFIX
    );

    const WedsNodeStatusPayload& status = record.latest_status;

    JsonDocument doc;
    doc["node_id"] = record.node_id;
    doc["timestamp_s"] = record.last_seen_timestamp_s;
    doc["last_sequence_id"] = record.last_sequence_id;
    doc["last_msg_type"] = record.last_msg_type;
    doc["temperature"] = status.temperature;
    doc["humidity"] = status.humidity;
    doc["pressure"] = status.pressure;
    doc["gas_resistance"] = status.gas_resistance;
    doc["battery_level"] = status.battery_level;
    doc["anomaly_state"] = status.anomaly_state;
    doc["anomaly_score"] = status.anomaly_score;
    doc["risk_state"] = status.risk_state;
    doc["risk_score"] = status.risk_score;
    doc["location_known"] = record.location_known;
    doc["latitude"] = record.latitude;
    doc["longitude"] = record.longitude;
    doc["pending_alert_mode"] = record.pending_alert_mode;
    doc["streak_open"] = record.streak_open;

    String payload;
    serializeJson(doc, payload);

    const bool ok = mqtt_.publish(topic, payload.c_str(), true);

    if (!ok) {
        Serial.print("[GATEWAY_MQTT] state publish failed topic=");
        Serial.println(topic);
    }

    return ok;
}

void WedsGatewayMqtt::handleMessage(
    char* topic,
    uint8_t* payload,
    unsigned int length
) {
    if (strcmp(topic, WEDS_COMMANDS_TOPIC) == 0) {
        handleCommand(payload, length);
    }
}

void WedsGatewayMqtt::handleCommand(const uint8_t* payload, unsigned int length) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload, length);

    if (error) {
        Serial.print("[GATEWAY_MQTT] Invalid command JSON: ");
        Serial.println(error.c_str());
        return;
    }

    const char* command_id = doc["id"] | "";
    const char* method = doc["method"] | "";
    const uint32_t node_id = doc["node_id"] | 0UL;
    JsonVariantConst params = doc["params"];

    if (command_id[0] == '\0' || method[0] == '\0') {
        Serial.println("[GATEWAY_MQTT] Command missing id or method");
        return;
    }

    if (strcmp(method, "enableAlertMode") == 0) {
        if (node_id == 0) {
            publishCommandResponse(command_id, method, node_id, false, "invalid_node_id");
            return;
        }

        handleEnableAlertModeCommand(command_id, node_id, params);
        return;
    }

    if (strcmp(method, "setLocation") == 0) {
        if (node_id == 0) {
            publishCommandResponse(command_id, method, node_id, false, "invalid_node_id");
            return;
        }

        handleSetLocationCommand(command_id, node_id, params);
        return;
    }

    if (strcmp(method, "clearConfig") == 0) {
        handleClearConfigCommand(command_id);
        return;
    }

    publishCommandResponse(command_id, method, node_id, false, "unsupported_method");
}

void WedsGatewayMqtt::handleEnableAlertModeCommand(
    const char* command_id,
    uint32_t node_id,
    JsonVariantConst params
) {
    const uint16_t duration_sec =
        params["duration_sec"] | WEDS_ALERT_MODE_DURATION_SEC;
    const uint16_t sampling_interval_sec =
        params["sampling_interval_sec"] | WEDS_ALERT_MODE_SAMPLING_INTERVAL_SEC;

    WedsAlertModeEnablePayload command;
    command.alert_source_node_id = WEDS_GATEWAY_ID;
    command.duration_sec = duration_sec;
    command.sampling_interval_sec = sampling_interval_sec;

    const bool delivered = gateway_comm_ != nullptr &&
        gateway_comm_->sendAlertModeEnableReliable(node_id, command);

    bool pending = false;

    if (delivered) {
        registry_->clearPendingAlertCommand(node_id);
    } else {
        registry_->setPendingAlertCommand(node_id, command);
        pending = true;
    }

    publishCommandResponse(
        command_id,
        "enableAlertMode",
        node_id,
        true,
        delivered ? "delivered" : "pending"
    );
}

void WedsGatewayMqtt::handleSetLocationCommand(
    const char* command_id,
    uint32_t node_id,
    JsonVariantConst params
) {
    if (!params["latitude"].is<double>() || !params["longitude"].is<double>()) {
        publishCommandResponse(
            command_id,
            "setLocation",
            node_id,
            false,
            "missing_location"
        );
        return;
    }

    const double latitude = params["latitude"].as<double>();
    const double longitude = params["longitude"].as<double>();

    if (latitude < -90.0 || latitude > 90.0 ||
        longitude < -180.0 || longitude > 180.0) {
        publishCommandResponse(
            command_id,
            "setLocation",
            node_id,
            false,
            "invalid_location"
        );
        return;
    }

    const bool set_ok = registry_->setNodeLocation(node_id, latitude, longitude);
    const bool save_ok = set_ok && registry_->savePersistentConfig();

    publishCommandResponse(
        command_id,
        "setLocation",
        node_id,
        set_ok && save_ok,
        set_ok && save_ok ? "location_saved" : "location_save_failed"
    );
}

void WedsGatewayMqtt::handleClearConfigCommand(const char* command_id) {
    const bool ok = registry_ != nullptr && registry_->clearPersistentConfig();

    publishCommandResponse(
        command_id,
        "clearConfig",
        0,
        ok,
        ok ? "config_cleared" : "clear_failed"
    );
}

void WedsGatewayMqtt::publishCommandResponse(
    const char* command_id,
    const char* method,
    uint32_t node_id,
    bool success,
    const char* message
) {
    JsonDocument doc;
    doc["id"] = command_id;
    doc["method"] = method;
    doc["node_id"] = node_id;
    doc["success"] = success;
    doc["message"] = message;
    doc["responded_at_s"] = time(nullptr);

    String payload;
    serializeJson(doc, payload);

    const bool ok = mqtt_.publish(WEDS_RESPONSES_TOPIC, payload.c_str());

    Serial.print("[GATEWAY_MQTT] command response id=");
    Serial.print(command_id);
    Serial.println(ok ? " OK" : " failed");
}

WedsGatewayMqtt::NodePublishState* WedsGatewayMqtt::stateForNode(uint32_t node_id) {
    for (size_t i = 0; i < WEDS_MAX_NODES; ++i) {
        if (node_states_[i].node_id == node_id) {
            return &node_states_[i];
        }
    }

    for (size_t i = 0; i < WEDS_MAX_NODES; ++i) {
        if (node_states_[i].node_id == 0) {
            node_states_[i].node_id = node_id;
            return &node_states_[i];
        }
    }

    return nullptr;
}

bool WedsGatewayMqtt::credentialsConfigured() const {
    return host_ != nullptr &&
        host_[0] != '\0' &&
        strcmp(host_, "YOUR_MQTT_HOST") != 0;
}
