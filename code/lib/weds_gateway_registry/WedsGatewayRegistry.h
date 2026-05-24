#pragma once

#include <Arduino.h>
#include "WedsGatewayConfig.h"
#include "WedsProtocol.h"

/**
 * @enum WedsEventType
 * @brief Represents the type of an alert event.
 */
enum WedsEventType : uint8_t {
    WEDS_EVENT_NONE = 0,
    WEDS_EVENT_ANOMALY_ALERT = 1,
    WEDS_EVENT_RISK_ALERT = 2,
    WEDS_EVENT_BOTH_ALERT = 3
};

/**
 * @struct WedsNodeEvent
 * @brief Represents a specific event triggered by a node.
 */
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

/**
 * @struct WedsTrendPoint
 * @brief A single data point representing a node's telemetry at a specific time.
 */
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

/**
 * @struct WedsNodeRecord
 * @brief Holds all relevant information about a node, including state, config, and trends.
 */
struct WedsNodeRecord {
    bool used;

    uint32_t node_id;

    uint32_t last_seen_ms;
    uint32_t last_seen_timestamp_s;
    uint16_t last_sequence_id;
    uint8_t last_msg_type;

    WedsNodeStatusPayload latest_status;

    bool location_known;
    double latitude;
    double longitude;

    bool pending_alert_mode;
    WedsAlertModeEnablePayload pending_alert_command;

    bool streak_open;
    WedsNodeEvent current_streak;

    WedsNodeEvent events[WEDS_MAX_EVENTS_PER_NODE];
    uint16_t event_head;
    uint16_t event_count;

    WedsTrendPoint trend[WEDS_TREND_POINTS_PER_NODE];
    uint16_t trend_head;
    uint16_t trend_count;
    uint32_t last_trend_sample_timestamp_s;
};

/**
 * @class WedsGatewayRegistry
 * @brief Maintains the state, location, events, and trends of all known nodes.
 */
class WedsGatewayRegistry {
public:
    /**
     * @brief Constructor for the registry.
     */
    WedsGatewayRegistry();

    /**
     * @brief Initializes the registry.
     * 
     * @return true If successful.
     * @return false If initialization fails.
     */
    bool begin();
    
    /**
     * @brief Loads persistent configuration from storage.
     * 
     * @return true If successful.
     * @return false If loading fails.
     */
    bool loadPersistentConfig();
    
    /**
     * @brief Saves persistent configuration to storage.
     * 
     * @return true If successful.
     * @return false If saving fails.
     */
    bool savePersistentConfig();
    
    /**
     * @brief Clears the persistent configuration.
     * 
     * @return true If successful.
     * @return false If clearing fails.
     */
    bool clearPersistentConfig();

    /**
     * @brief Updates a node's status and manages its events/trends.
     * 
     * @param node_id The ID of the node.
     * @param sequence_id Sequence ID of the incoming packet.
     * @param msg_type Type of the incoming message.
     * @param status The payload status.
     * @return true If updated successfully.
     * @return false If update fails.
     */
    bool updateNodeStatus(
        uint32_t node_id,
        uint16_t sequence_id,
        uint8_t msg_type,
        const WedsNodeStatusPayload& status
    );

    /**
     * @brief Retrieves the current state of a specific node.
     * 
     * @param node_id Node ID to fetch.
     * @param out_record Reference to store the retrieved record.
     * @return true If the node exists.
     * @return false If the node was not found.
     */
    bool getNodeState(
        uint32_t node_id,
        WedsNodeRecord& out_record
    ) const;

    /**
     * @brief Retrieves records of all known nodes.
     * 
     * @param out_records Array to store the records.
     * @param max_records Maximum capacity of the array.
     * @return size_t The number of records retrieved.
     */
    size_t getAllNodes(
        WedsNodeRecord* out_records,
        size_t max_records
    ) const;

    /**
     * @brief Retrieves records of nodes that don't have a known location.
     * 
     * @param out_records Array to store the records.
     * @param max_records Maximum capacity of the array.
     * @return size_t The number of unlocated nodes retrieved.
     */
    size_t getUnlocatedNodes(
        WedsNodeRecord* out_records,
        size_t max_records
    ) const;

    /**
     * @brief Gets the count of known nodes.
     * 
     * @return size_t Node count.
     */
    size_t getKnownNodeCount() const;

    /**
     * @brief Sets the location for a node in RAM.
     * 
     * @param node_id Node ID.
     * @param latitude Latitude to set.
     * @param longitude Longitude to set.
     * @return true If location was successfully set.
     * @return false If setting location failed.
     */
    bool setNodeLocation(
        uint32_t node_id,
        double latitude,
        double longitude
    );

    /**
     * @brief Checks if a received reliable packet is a duplicate.
     * 
     * @param node_id Source node ID.
     * @param sequence_id Sequence ID.
     * @param msg_type Message type.
     * @return true If it is a duplicate.
     * @return false Otherwise.
     */
    bool isDuplicateReliablePacket(
        uint32_t node_id,
        uint16_t sequence_id,
        uint8_t msg_type
    ) const;

    /**
     * @brief Finds neighboring nodes within a certain radius.
     * 
     * @param source_node_id The ID of the center node.
     * @param out_neighbors Array to store the IDs of neighbor nodes.
     * @param max_neighbors Maximum capacity of the array.
     * @return size_t Number of neighbors found.
     */
    size_t findNeighbors(
        uint32_t source_node_id,
        uint32_t* out_neighbors,
        size_t max_neighbors
    ) const;

    /**
     * @brief Creates pending alert commands for neighbors of a source node.
     * 
     * @param source_node_id The source node's ID.
     * @param status Status payload indicating an alert.
     */
    void createAlertCommandsForNeighbors(
        uint32_t source_node_id,
        const WedsNodeStatusPayload& status
    );

    /**
     * @brief Checks if there is a pending alert command for a node.
     * 
     * @param node_id The node ID to check.
     * @param out_command Reference to store the pending command.
     * @return true If a command is pending.
     * @return false Otherwise.
     */
    bool hasPendingAlertCommand(
        uint32_t node_id,
        WedsAlertModeEnablePayload& out_command
    ) const;

    /**
     * @brief Sets a pending alert command for a node.
     * 
     * @param node_id Node ID.
     * @param command The command to set.
     */
    void setPendingAlertCommand(
        uint32_t node_id,
        const WedsAlertModeEnablePayload& command
    );

    /**
     * @brief Clears any pending alert command for a given node.
     * 
     * @param node_id Node ID.
     */
    void clearPendingAlertCommand(uint32_t node_id);

    /**
     * @brief Retrieves events for a specific node.
     * 
     * @param node_id Node ID.
     * @param out_events Array to store the events.
     * @param max_events Maximum capacity of the array.
     * @return size_t Number of events retrieved.
     */
    size_t getNodeEvents(
        uint32_t node_id,
        WedsNodeEvent* out_events,
        size_t max_events
    ) const;

    /**
     * @brief Retrieves trend points for a specific node.
     * 
     * @param node_id Node ID.
     * @param out_points Array to store trend points.
     * @param max_points Maximum capacity of the array.
     * @return size_t Number of points retrieved.
     */
    size_t getNodeTrend(
        uint32_t node_id,
        WedsTrendPoint* out_points,
        size_t max_points
    ) const;

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


