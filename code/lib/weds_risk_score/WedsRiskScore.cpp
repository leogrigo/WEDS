#include "WedsRiskScore.h"

WedsRiskResult WedsRiskScoreCalculator::update(const WedsSensorSample& sample) {
    (void)sample;

    // TODO: replace with ML risk-score model.
    WedsRiskResult result{};
    result.detection_state = WEDS_DETECTION_NORMAL;
    result.score = 0.0f;
    return result;
}
