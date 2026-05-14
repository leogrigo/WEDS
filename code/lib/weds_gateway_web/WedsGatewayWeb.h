#pragma once

#include <Arduino.h>

class WedsGatewayWeb {
public:
    static const char* indexHtml(); // Returns the HTML content for the index page
    static const char* adminHtml(); // Returns the HTML content for the admin page
};
