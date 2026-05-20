#pragma once

#include <ArduinoJson.h>
#include <Arduino.h>
#include <IPAddress.h>
#include <WebServer.h>
#include <WiFi.h>

#include "WedsGatewayRegistry.h"

/**
 * @class WedsGatewayApi
 * @brief Manages the HTTP API server for the gateway.
 */
class WedsGatewayApi {
public:
    /**
     * @brief Constructor for the WedsGatewayApi.
     */
    WedsGatewayApi();

    /**
     * @brief Initializes the WiFi connection and starts the HTTP API server.
     * 
     * @param ssid WiFi SSID to connect to.
     * @param password WiFi password.
     * @param registry Pointer to the gateway registry.
     * @param port Port for the HTTP server.
     * @return true If initialized successfully.
     * @return false If initialization fails.
     */
    bool begin(
        const char* ssid,
        const char* password,
        WedsGatewayRegistry* registry,
        uint16_t port = 80
    );

    /**
     * @brief Handles incoming client requests. Must be called in the main loop.
     */
    void handleClient();

    /**
     * @brief Gets the local IP address of the gateway on the WiFi network.
     * 
     * @return IPAddress The local IP address.
     */
    IPAddress localIP() const;

private:
    /** @brief Pointer to the node registry */
    WedsGatewayRegistry* registry_;
    
    /** @brief Internal web server instance */
    WebServer server_;
    
    /** @brief Indicates if the API has been initialized */
    bool initialized_;

    /** @brief Sets up the routing for the HTTP server endpoints. */
    void setupRoutes();
    
    /** @brief Synchronizes the system clock using NTP. */
    void syncClock();

    /** @brief Handles the root API info endpoint. */
    void handleRootInfo();

    /** @brief Handles the endpoint for getting all node states. */
    void handleGetStateAll();
    
    /** @brief Handles the endpoint for getting a specific node state. */
    void handleGetNodeState();
    
    /** @brief Handles the endpoint for getting unlocated nodes. */
    void handleGetUnlocatedNodes();
    
    /** @brief Handles the endpoint for getting node events. */
    void handleGetNodeEvents();
    
    /** @brief Handles the endpoint for getting node trend data. */
    void handleGetNodeTrend();

    /** @brief Handles the endpoint for setting a node's location. */
    void handleSetLocation();
    
    /** @brief Handles the endpoint for clearing the persistent config. */
    void handleClearConfig();

    /**
     * @brief Sends a JSON response to the client.
     * 
     * @param status_code HTTP status code.
     * @param json JSON string to send.
     */
    void sendJson(int status_code, const String& json);
    
    /**
     * @brief Sends a JSON error response to the client.
     * 
     * @param status_code HTTP status code.
     * @param error_code Short string identifier for the error.
     * @param message Human-readable error message.
     */
    void sendError(int status_code, const char* error_code, const char* message);

    /**
     * @brief Converts a detection state value to a string.
     * 
     * @param state Detection state byte.
     * @return String String representation of the state.
     */
    String detectionStateToString(uint8_t state) const;
    
    /**
     * @brief Converts an event type to a string.
     * 
     * @param type Event type enumeration.
     * @return String String representation of the event type.
     */
    String eventTypeToString(WedsEventType type) const;

    /**
     * @brief Parses the 'node_id' argument from the request parameters.
     * 
     * @param out_node_id Reference to store the parsed node ID.
     * @return true If parsing was successful.
     * @return false If the argument is missing or invalid.
     */
    bool parseNodeIdArg(uint32_t& out_node_id);
    
    /**
     * @brief Appends a node record object to a JSON object.
     * 
     * @param obj JSON object to populate.
     * @param record The node record to append data from.
     */
    void appendNodeRecord(JsonObject obj, const WedsNodeRecord& record);
};
