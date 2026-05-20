#pragma once

#include <Arduino.h>

/**
 * @class WedsGatewayWeb
 * @brief Provides static HTML content for the gateway web interface.
 */
class WedsGatewayWeb {
public:
    /**
     * @brief Returns the HTML content for the dashboard index page.
     * 
     * @return const char* Pointer to the PROGMEM string containing the index HTML.
     */
    static const char* indexHtml();

    /**
     * @brief Returns the HTML content for the administration page.
     * 
     * @return const char* Pointer to the PROGMEM string containing the admin HTML.
     */
    static const char* adminHtml();
};
