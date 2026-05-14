#pragma once

#include <ArduinoJson.h>
#include <Arduino.h>
#include <IPAddress.h>
#include <WebServer.h>
#include <WiFi.h>

#include "WedsGatewayRegistry.h"

class WedsGatewayApi {
public:
    WedsGatewayApi();

    bool begin(
        const char* ssid,
        const char* password,
        WedsGatewayRegistry* registry,
        uint16_t port = 80
    );  // Begin WiFi connection and start API server

    void handleClient();

    IPAddress localIP() const;

private:
    WedsGatewayRegistry* registry_;
    WebServer server_;
    bool initialized_;

    void setupRoutes();
    void syncClock();

    void handleRootInfo();

    void handleGetStateAll();
    void handleGetNodeState();
    void handleGetUnlocatedNodes();
    void handleGetNodeEvents();
    void handleGetNodeTrend();

    void handleSetLocation();
    void handleClearConfig();

    void sendJson(int status_code, const String& json);
    void sendError(int status_code, const char* error_code, const char* message);

    String detectionStateToString(uint8_t state) const;
    String eventTypeToString(WedsEventType type) const;

    bool parseNodeIdArg(uint32_t& out_node_id);
    void appendNodeRecord(JsonObject obj, const WedsNodeRecord& record);
};
