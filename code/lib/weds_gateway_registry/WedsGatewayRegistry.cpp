#include "WedsGatewayRegistry.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <math.h>
#include <string.h>
#include <time.h>

static uint32_t gatewayTimestampOrFallback(uint32_t fallback_timestamp_s) {
    const time_t now = time(nullptr);

    if (now >= static_cast<time_t>(WEDS_MIN_VALID_EPOCH_S)) {
        return static_cast<uint32_t>(now);
    }

    return fallback_timestamp_s;
}

WedsGatewayRegistry::WedsGatewayRegistry()
    : next_event_id_(1) {
    for (size_t i = 0; i < WEDS_MAX_NODES; ++i) {
        initRecord(records_[i], 0);
    }
}

bool WedsGatewayRegistry::begin() {
    for (size_t i = 0; i < WEDS_MAX_NODES; ++i) {
        initRecord(records_[i], 0);
    }

    next_event_id_ = 1;

    Serial.println("[REGISTRY] Mounting LittleFS...");

    if (!LittleFS.begin(true)) {
        Serial.println("[REGISTRY] LittleFS mount failed");
        return false;
    }

    Serial.println("[REGISTRY] LittleFS mounted");
    Serial.println("[REGISTRY] Ready");
    return true;
}

bool WedsGatewayRegistry::loadPersistentConfig() {
    File file = LittleFS.open(WEDS_GATEWAY_CONFIG_PATH, "r", false);

    if (!file) {
        Serial.println("[REGISTRY] No persistent config found");
        return true;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        Serial.print("[REGISTRY] Failed to parse persistent config: ");
        Serial.println(error.c_str());
        return false;
    }

    JsonArray nodes = doc["nodes"].as<JsonArray>();

    if (nodes.isNull()) {
        Serial.println("[REGISTRY] Persistent config has no nodes array");
        return false;
    }

    size_t loaded_count = 0;

    for (JsonObject node : nodes) {
        JsonVariant node_id_value = node["node_id"];

        if (!node_id_value.is<uint32_t>()) {
            Serial.println("[REGISTRY] Skipping config node with invalid id");
            continue;
        }

        const uint32_t node_id = node_id_value.as<uint32_t>();

        if (node_id == 0) {
            Serial.println("[REGISTRY] Skipping config node with invalid id");
            continue;
        }

        const bool location_known = node["location_known"] | false;
        const double latitude = node["latitude"] | 0.0;
        const double longitude = node["longitude"] | 0.0;

        if (location_known) {
            if (setNodeLocation(node_id, latitude, longitude)) {
                loaded_count++;
            }
            continue;
        }

        WedsNodeRecord* record = findOrCreateRecord(node_id);

        if (record == nullptr) {
            Serial.println("[REGISTRY] Config load stopped: registry full");
            return false;
        }

        record->location_known = false;
        record->latitude = 0.0;
        record->longitude = 0.0;
        loaded_count++;
    }

    Serial.print("[REGISTRY] Loaded persistent config nodes=");
    Serial.println(loaded_count);
    return true;
}

bool WedsGatewayRegistry::savePersistentConfig() {
    JsonDocument doc;
    JsonArray nodes = doc["nodes"].to<JsonArray>();

    for (size_t i = 0; i < WEDS_MAX_NODES; ++i) {
        const WedsNodeRecord& record = records_[i];

        if (!record.used || !record.location_known) {
            continue;
        }

        JsonObject node = nodes.add<JsonObject>();
        node["node_id"] = record.node_id;
        node["location_known"] = record.location_known;
        node["latitude"] = record.latitude;
        node["longitude"] = record.longitude;
    }

    File file = LittleFS.open(WEDS_GATEWAY_CONFIG_PATH, "w");

    if (!file) {
        Serial.println("[REGISTRY] Failed to open persistent config for write");
        return false;
    }

    const size_t written = serializeJsonPretty(doc, file);
    file.close();

    if (written == 0) {
        Serial.println("[REGISTRY] Failed to write persistent config");
        return false;
    }

    Serial.print("[REGISTRY] Saved persistent config bytes=");
    Serial.println(written);

    serializeJsonPretty(doc, Serial);
    Serial.println();

    return true;
}

bool WedsGatewayRegistry::clearPersistentConfig() {
    if (!LittleFS.exists(WEDS_GATEWAY_CONFIG_PATH)) {
        Serial.println("[REGISTRY] No persistent config to clear");
        return true;
    }

    if (!LittleFS.remove(WEDS_GATEWAY_CONFIG_PATH)) {
        Serial.println("[REGISTRY] Failed to delete persistent config");
        return false;
    }

    Serial.println("[REGISTRY] Persistent config cleared");
    return true;
}

bool WedsGatewayRegistry::updateNodeStatus(
    uint32_t node_id,
    uint16_t sequence_id,
    uint8_t msg_type,
    const WedsNodeStatusPayload& status
) {
    WedsNodeRecord* record = findOrCreateRecord(node_id);

    if (record == nullptr) {
        Serial.println("[GATEWAY_REGISTRY] Full, cannot store node");
        return false;
    }

    WedsNodeStatusPayload stored_status = status;
    stored_status.timestamp_s = gatewayTimestampOrFallback(status.timestamp_s);

    record->last_seen_ms = millis();
    record->last_seen_timestamp_s = stored_status.timestamp_s;
    record->last_sequence_id = sequence_id;
    record->last_msg_type = msg_type;
    record->latest_status = stored_status;

    updateEventStreak(*record, stored_status);
    updateTrend(*record, stored_status);

    Serial.print("[GATEWAY_REGISTRY] Updated node=");
    Serial.print(node_id);
    Serial.print(" known_nodes=");
    Serial.println(getKnownNodeCount());

    return true;
}

bool WedsGatewayRegistry::getNodeState(
    uint32_t node_id,
    WedsNodeRecord& out_record
) const {
    const WedsNodeRecord* record = findRecord(node_id);

    if (record == nullptr) {
        return false;
    }

    out_record = *record;
    return true;
}

size_t WedsGatewayRegistry::getAllNodes(
    WedsNodeRecord* out_records,
    size_t max_records
) const {
    if (out_records == nullptr || max_records == 0) {
        return 0;
    }

    size_t count = 0;

    for (size_t i = 0; i < WEDS_MAX_NODES && count < max_records; ++i) {
        if (records_[i].used) {
            out_records[count++] = records_[i];
        }
    }

    return count;
}

size_t WedsGatewayRegistry::getUnlocatedNodes(
    WedsNodeRecord* out_records,
    size_t max_records
) const {
    if (out_records == nullptr || max_records == 0) {
        return 0;
    }

    size_t count = 0;

    for (size_t i = 0; i < WEDS_MAX_NODES && count < max_records; ++i) {
        if (records_[i].used && !records_[i].location_known) {
            out_records[count++] = records_[i];
        }
    }

    return count;
}

size_t WedsGatewayRegistry::getKnownNodeCount() const {
    size_t count = 0;

    for (size_t i = 0; i < WEDS_MAX_NODES; ++i) {
        if (records_[i].used) {
            ++count;
        }
    }

    return count;
}

bool WedsGatewayRegistry::setNodeLocation(
    uint32_t node_id,
    double latitude,
    double longitude
) {
    WedsNodeRecord* record = findOrCreateRecord(node_id);

    if (record == nullptr) {
        Serial.println("[REGISTRY] setNodeLocation failed: full");
        return false;
    }

    record->location_known = true;
    record->latitude = latitude;
    record->longitude = longitude;

    Serial.print("[REGISTRY] Location set node=");
    Serial.print(node_id);
    Serial.print(" lat=");
    Serial.print(latitude, 6);
    Serial.print(" lon=");
    Serial.println(longitude, 6);

    return true;
}

bool WedsGatewayRegistry::isDuplicateReliablePacket(
    uint32_t node_id,
    uint16_t sequence_id,
    uint8_t msg_type
) const {
    const WedsNodeRecord* record = findRecord(node_id);

    if (record == nullptr) {
        return false;
    }

    return (
        record->last_sequence_id == sequence_id &&
        record->last_msg_type == msg_type
    );
}

size_t WedsGatewayRegistry::findNeighbors(
    uint32_t source_node_id,
    uint32_t* out_neighbors,
    size_t max_neighbors
) const {
    if (out_neighbors == nullptr || max_neighbors == 0) {
        return 0;
    }

    const WedsNodeRecord* source = findRecord(source_node_id);

    if (source == nullptr || !source->location_known) {
        Serial.println("[GATEWAY_REGISTRY] Source location unknown");
        return 0;
    }

    size_t count = 0;

    for (size_t i = 0; i < WEDS_MAX_NODES && count < max_neighbors; ++i) {
        const WedsNodeRecord& candidate = records_[i];

        if (!candidate.used ||
            candidate.node_id == source_node_id ||
            !candidate.location_known) {
            continue;
        }

        const double distance_m = distanceMeters(
            source->latitude,
            source->longitude,
            candidate.latitude,
            candidate.longitude
        );

        Serial.print("[GATEWAY_REGISTRY] Distance source=");
        Serial.print(source_node_id);
        Serial.print(" candidate=");
        Serial.print(candidate.node_id);
        Serial.print(" distance_m=");
        Serial.println(distance_m);

        if (distance_m <= WEDS_NEIGHBOR_RADIUS_M) {
            out_neighbors[count++] = candidate.node_id;
        }
    }

    return count;
}

void WedsGatewayRegistry::createAlertCommandsForNeighbors(
    uint32_t source_node_id,
    const WedsNodeStatusPayload& status
) {
    if (!isAlertStatus(status)) {
        return;
    }

    uint32_t neighbors[WEDS_MAX_NODES];
    const size_t neighbor_count = findNeighbors(
        source_node_id,
        neighbors,
        WEDS_MAX_NODES
    );

    Serial.print("[GATEWAY_REGISTRY] Alert neighbors=");
    Serial.println(neighbor_count);

    for (size_t i = 0; i < neighbor_count; ++i) {
        WedsAlertModeEnablePayload command;
        command.alert_source_node_id = source_node_id;
        command.duration_sec = WEDS_ALERT_MODE_DURATION_SEC;
        command.sampling_interval_sec = WEDS_ALERT_MODE_SAMPLING_INTERVAL_SEC;

        setPendingAlertCommand(neighbors[i], command);
    }
}

bool WedsGatewayRegistry::hasPendingAlertCommand(
    uint32_t node_id,
    WedsAlertModeEnablePayload& out_command
) const {
    const WedsNodeRecord* record = findRecord(node_id);

    if (record == nullptr || !record->pending_alert_mode) {
        return false;
    }

    out_command = record->pending_alert_command;
    return true;
}

void WedsGatewayRegistry::setPendingAlertCommand(
    uint32_t node_id,
    const WedsAlertModeEnablePayload& command
) {
    WedsNodeRecord* record = findOrCreateRecord(node_id);

    if (record == nullptr) {
        Serial.println("[GATEWAY_REGISTRY] Cannot store pending command: full");
        return;
    }

    record->pending_alert_mode = true;
    record->pending_alert_command = command;

    Serial.print("[GATEWAY_REGISTRY] Pending ALERT_MODE_ENABLE node=");
    Serial.println(node_id);
}

void WedsGatewayRegistry::clearPendingAlertCommand(uint32_t node_id) {
    WedsNodeRecord* record = nullptr;

    for (size_t i = 0; i < WEDS_MAX_NODES; ++i) {
        if (records_[i].used && records_[i].node_id == node_id) {
            record = &records_[i];
            break;
        }
    }

    if (record == nullptr) {
        return;
    }

    record->pending_alert_mode = false;
    memset(
        &record->pending_alert_command,
        0,
        sizeof(WedsAlertModeEnablePayload)
    );
}

size_t WedsGatewayRegistry::getNodeEvents(
    uint32_t node_id,
    WedsNodeEvent* out_events,
    size_t max_events
) const {
    if (out_events == nullptr || max_events == 0) {
        return 0;
    }

    const WedsNodeRecord* record = findRecord(node_id);

    if (record == nullptr) {
        return 0;
    }

    size_t count = 0;

    if (record->streak_open && count < max_events) {
        out_events[count++] = record->current_streak;
    }

    for (size_t i = 0; i < record->event_count && count < max_events; ++i) {
        const size_t newest_index =
            (record->event_head + WEDS_MAX_EVENTS_PER_NODE - 1 - i) %
            WEDS_MAX_EVENTS_PER_NODE;

        if (record->events[newest_index].used) {
            out_events[count++] = record->events[newest_index];
        }
    }

    return count;
}

size_t WedsGatewayRegistry::getNodeTrend(
    uint32_t node_id,
    WedsTrendPoint* out_points,
    size_t max_points
) const {
    if (out_points == nullptr || max_points == 0) {
        return 0;
    }

    const WedsNodeRecord* record = findRecord(node_id);

    if (record == nullptr) {
        return 0;
    }

    const size_t available = record->trend_count;
    const size_t count = (available < max_points) ? available : max_points;
    const size_t skipped = available - count;

    for (size_t i = 0; i < count; ++i) {
        const size_t age_index = skipped + i;
        const size_t index =
            (record->trend_head + WEDS_TREND_POINTS_PER_NODE - available +
             age_index) %
            WEDS_TREND_POINTS_PER_NODE;

        out_points[i] = record->trend[index];
    }

    return count;
}

WedsNodeRecord* WedsGatewayRegistry::findOrCreateRecord(uint32_t node_id) {
    for (size_t i = 0; i < WEDS_MAX_NODES; ++i) {
        if (records_[i].used && records_[i].node_id == node_id) {
            return &records_[i];
        }
    }

    for (size_t i = 0; i < WEDS_MAX_NODES; ++i) {
        if (!records_[i].used) {
            initRecord(records_[i], node_id);
            return &records_[i];
        }
    }

    return nullptr;
}

const WedsNodeRecord* WedsGatewayRegistry::findRecord(
    uint32_t node_id
) const {
    for (size_t i = 0; i < WEDS_MAX_NODES; ++i) {
        if (records_[i].used && records_[i].node_id == node_id) {
            return &records_[i];
        }
    }

    return nullptr;
}

void WedsGatewayRegistry::initRecord(
    WedsNodeRecord& record,
    uint32_t node_id
) {
    memset(&record, 0, sizeof(WedsNodeRecord));
    record.used = (node_id != 0);
    record.node_id = node_id;
}

void WedsGatewayRegistry::updateEventStreak(
    WedsNodeRecord& record,
    const WedsNodeStatusPayload& status
) {
    const bool alert = isAlertStatus(status);

    if (!record.streak_open && alert) {
        openEventStreak(record, getEventTypeFromStatus(status), status);
        return;
    }

    if (record.streak_open && alert) {
        updateOpenEventStreak(record, status);
        return;
    }

    if (record.streak_open && !alert) {
        closeEventStreak(record, status);
    }
}

void WedsGatewayRegistry::openEventStreak(
    WedsNodeRecord& record,
    WedsEventType type,
    const WedsNodeStatusPayload& status
) {
    memset(&record.current_streak, 0, sizeof(WedsNodeEvent));

    record.streak_open = true;
    record.current_streak.used = true;
    record.current_streak.event_id = next_event_id_++;
    record.current_streak.node_id = record.node_id;
    record.current_streak.type = type;
    record.current_streak.start_timestamp_s = status.timestamp_s;
    record.current_streak.end_timestamp_s = status.timestamp_s;
    record.current_streak.still_open = true;
    record.current_streak.peak_anomaly_score = status.anomaly_score;
    record.current_streak.peak_risk_score = status.risk_score;
    record.current_streak.max_temperature = status.temperature;
    record.current_streak.min_humidity = status.humidity;
    record.current_streak.min_gas_resistance = status.gas_resistance;
    record.current_streak.sample_count = 1;
}

void WedsGatewayRegistry::updateOpenEventStreak(
    WedsNodeRecord& record,
    const WedsNodeStatusPayload& status
) {
    WedsNodeEvent& event = record.current_streak;
    const WedsEventType new_type = getEventTypeFromStatus(status);

    if (event.type != new_type) {
        event.type = WEDS_EVENT_BOTH_ALERT;
    }

    event.end_timestamp_s = status.timestamp_s;
    event.peak_anomaly_score = max(event.peak_anomaly_score, status.anomaly_score);
    event.peak_risk_score = max(event.peak_risk_score, status.risk_score);
    event.max_temperature = max(event.max_temperature, status.temperature);
    event.min_humidity = min(event.min_humidity, status.humidity);
    event.min_gas_resistance = min(
        event.min_gas_resistance,
        status.gas_resistance
    );
    event.sample_count++;
}

void WedsGatewayRegistry::closeEventStreak(
    WedsNodeRecord& record,
    const WedsNodeStatusPayload& status
) {
    record.current_streak.end_timestamp_s = status.timestamp_s;
    record.current_streak.still_open = false;

    pushEvent(record, record.current_streak);

    record.streak_open = false;
    memset(&record.current_streak, 0, sizeof(WedsNodeEvent));
}

void WedsGatewayRegistry::pushEvent(
    WedsNodeRecord& record,
    const WedsNodeEvent& event
) {
    record.events[record.event_head] = event;
    record.event_head =
        (record.event_head + 1) % WEDS_MAX_EVENTS_PER_NODE;

    if (record.event_count < WEDS_MAX_EVENTS_PER_NODE) {
        record.event_count++;
    }
}

void WedsGatewayRegistry::updateTrend(
    WedsNodeRecord& record,
    const WedsNodeStatusPayload& status
) {
    if (record.trend_count > 0) {
        const uint32_t elapsed_s =
            status.timestamp_s - record.last_trend_sample_timestamp_s;

        if (elapsed_s < WEDS_TREND_SAMPLE_INTERVAL_SEC) {
            return;
        }
    }

    WedsTrendPoint& point = record.trend[record.trend_head];
    point.used = true;
    point.timestamp_s = status.timestamp_s;
    point.temperature = status.temperature;
    point.humidity = status.humidity;
    point.pressure = status.pressure;
    point.gas_resistance = status.gas_resistance;
    point.battery_level = status.battery_level;
    point.anomaly_score = status.anomaly_score;
    point.risk_score = status.risk_score;

    record.last_trend_sample_timestamp_s = status.timestamp_s;
    record.trend_head =
        (record.trend_head + 1) % WEDS_TREND_POINTS_PER_NODE;

    if (record.trend_count < WEDS_TREND_POINTS_PER_NODE) {
        record.trend_count++;
    }
}

bool WedsGatewayRegistry::isAlertStatus(
    const WedsNodeStatusPayload& status
) {
    return (
        status.anomaly_state == WEDS_DETECTION_ALERT ||
        status.risk_state == WEDS_DETECTION_ALERT
    );
}

WedsEventType WedsGatewayRegistry::getEventTypeFromStatus(
    const WedsNodeStatusPayload& status
) {
    const bool anomaly_alert = status.anomaly_state == WEDS_DETECTION_ALERT;
    const bool risk_alert = status.risk_state == WEDS_DETECTION_ALERT;

    if (anomaly_alert && risk_alert) {
        return WEDS_EVENT_BOTH_ALERT;
    }

    if (anomaly_alert) {
        return WEDS_EVENT_ANOMALY_ALERT;
    }

    if (risk_alert) {
        return WEDS_EVENT_RISK_ALERT;
    }

    return WEDS_EVENT_NONE;
}

double WedsGatewayRegistry::distanceMeters(
    double lat1,
    double lon1,
    double lat2,
    double lon2
) {
    static constexpr double EARTH_RADIUS_M = 6371000.0;

    const double phi1 = lat1 * DEG_TO_RAD;
    const double phi2 = lat2 * DEG_TO_RAD;

    const double d_phi = (lat2 - lat1) * DEG_TO_RAD;
    const double d_lambda = (lon2 - lon1) * DEG_TO_RAD;

    const double sin_d_phi = sin(d_phi / 2.0);
    const double sin_d_lambda = sin(d_lambda / 2.0);

    const double a =
        sin_d_phi * sin_d_phi +
        cos(phi1) * cos(phi2) * sin_d_lambda * sin_d_lambda;

    const double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));

    return EARTH_RADIUS_M * c;
}

