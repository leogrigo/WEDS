#pragma once

#include <Arduino.h>
#include "WedsGatewayConfig.h"
#include "WedsProtocol.h"

enum WedsEventType : uint8_t {
    WEDS_EVENT_NONE = 0,
    WEDS_EVENT_ANOMALY_ALERT = 1,
    WEDS_EVENT_RISK_ALERT = 2,
    WEDS_EVENT_BOTH_ALERT = 3
};

struct WedsNodeEvent {
    bool used;

    uint32_t event_id;
    uint32_t node_id;

    WedsEventType type;

    uint32_t start_timestamp_s;
    uint32_t end_timestamp_s;

    bool still_open;

    float peak_anomaly_score;
    float peak_risk_score;
    float max_temperature;
    float min_humidity;
    float min_gas_resistance;

    uint16_t sample_count;
};

struct WedsTrendPoint {
    bool used;
    uint32_t timestamp_s;

    float temperature;
    float humidity;
    float pressure;
    float gas_resistance;
    float battery_level;
    float anomaly_score;
    float risk_score;
};

// Data structure to hold all relevant information about a node
struct WedsNodeRecord {
    bool used;

    uint32_t node_id;

    // Latest runtime state
    uint32_t last_seen_ms;
    uint32_t last_seen_timestamp_s;
    uint16_t last_sequence_id;
    uint8_t last_msg_type;

    WedsNodeStatusPayload latest_status;

    // Persistent location config
    bool location_known;
    double latitude;
    double longitude;

    // Pending gateway command
    bool pending_alert_mode;
    WedsAlertModeEnablePayload pending_alert_command;

    // Event streaks
    bool streak_open;
    WedsNodeEvent current_streak;

    WedsNodeEvent events[WEDS_MAX_EVENTS_PER_NODE];
    uint16_t event_head;
    uint16_t event_count;

    // Downsampled trend
    WedsTrendPoint trend[WEDS_TREND_POINTS_PER_NODE];
    uint16_t trend_head;
    uint16_t trend_count;
    uint32_t last_trend_sample_timestamp_s;
};

class WedsGatewayRegistry {
public:
    WedsGatewayRegistry();

    bool begin();   // Load persistent config and initialize registry
    bool loadPersistentConfig();   // Load persistent configuration from storage
    bool savePersistentConfig();   // Save persistent configuration to storage
    bool clearPersistentConfig();  // Clear persistent configuration

    bool updateNodeStatus(
        uint32_t node_id,
        uint16_t sequence_id,
        uint8_t msg_type,
        const WedsNodeStatusPayload& status
    );  // Update node status and manage events/trends

    bool getNodeState(
        uint32_t node_id,
        WedsNodeRecord& out_record
    ) const;    // Retrieve current state of a node

    size_t getAllNodes(
        WedsNodeRecord* out_records,
        size_t max_records
    ) const;    // Retrieve records of all known nodes

    size_t getUnlocatedNodes(
        WedsNodeRecord* out_records,
        size_t max_records
    ) const;    // Retrieve records of nodes without known location

    size_t getKnownNodeCount() const; // Get count of known nodes

    bool setNodeLocation(
        uint32_t node_id,
        double latitude,
        double longitude
    );  // Set location for a node in ram memory

    bool isDuplicateReliablePacket(
        uint32_t node_id,
        uint16_t sequence_id,
        uint8_t msg_type
    ) const;    // Check if a received reliable packet is a duplicate

    size_t findNeighbors(
        uint32_t source_node_id,
        uint32_t* out_neighbors,
        size_t max_neighbors
    ) const;    // Find neighboring nodes within a certain radius

    void createAlertCommandsForNeighbors(
        uint32_t source_node_id,
        const WedsNodeStatusPayload& status
    );  // Create pending alert commands for neighbors of source node

    bool hasPendingAlertCommand(
        uint32_t node_id,
        WedsAlertModeEnablePayload& out_command
    ) const;    // Check if there is a pending alert command for a node and retrieve it

    void setPendingAlertCommand(
        uint32_t node_id,
        const WedsAlertModeEnablePayload& command
    );  // Set a pending alert command for a node

    void clearPendingAlertCommand(uint32_t node_id);    // Clear any pending alert command for a node

    size_t getNodeEvents(
        uint32_t node_id,
        WedsNodeEvent* out_events,
        size_t max_events
    ) const;    // Retrieve events for a node

    size_t getNodeTrend(
        uint32_t node_id,
        WedsTrendPoint* out_points,
        size_t max_points
    ) const;    // Retrieve trend points for a node

private:
    WedsNodeRecord records_[WEDS_MAX_NODES];

    uint32_t next_event_id_;

    WedsNodeRecord* findOrCreateRecord(uint32_t node_id);
    const WedsNodeRecord* findRecord(uint32_t node_id) const;

    void initRecord(WedsNodeRecord& record, uint32_t node_id);

    void updateEventStreak(
        WedsNodeRecord& record,
        const WedsNodeStatusPayload& status
    );

    void openEventStreak(
        WedsNodeRecord& record,
        WedsEventType type,
        const WedsNodeStatusPayload& status
    );

    void updateOpenEventStreak(
        WedsNodeRecord& record,
        const WedsNodeStatusPayload& status
    );

    void closeEventStreak(
        WedsNodeRecord& record,
        const WedsNodeStatusPayload& status
    );

    void pushEvent(
        WedsNodeRecord& record,
        const WedsNodeEvent& event
    );

    void updateTrend(
        WedsNodeRecord& record,
        const WedsNodeStatusPayload& status
    );

    static bool isAlertStatus(const WedsNodeStatusPayload& status);
    static WedsEventType getEventTypeFromStatus(const WedsNodeStatusPayload& status);

    static double distanceMeters(
        double lat1,
        double lon1,
        double lat2,
        double lon2
    );
};


