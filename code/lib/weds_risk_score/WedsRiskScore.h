#pragma once

#include <cstdint>

#include "WedsSensorSample.h"
#include "WedsTypes.h"

struct WedsRiskResult {
    uint8_t detection_state;
    float score;
};

class WedsRiskScoreCalculator {
public:
    WedsRiskResult update(const WedsSensorSample& sample);
};
