#include "WedsNodeSimulation.h"

namespace {

constexpr float PI_F = 3.14159265f;
constexpr uint8_t WEDS_SIMULATION_WARMUP_SAMPLES = 20;

/**
 * @brief Generates a random number from a normal distribution.
 * @param mean The mean of the distribution.
 * @param stddev The standard deviation of the distribution.
 * @return float A random number.
 */
float sampleNormal(float mean, float stddev) {
    const float u1 = random(1, 10001) / 10001.0f;
    const float u2 = random(0, 10000) / 10000.0f;
    const float radius = sqrtf(-2.0f * logf(u1));
    const float angle = 2.0f * PI_F * u2;
    return mean + radius * cosf(angle) * stddev;
}

/**
 * @brief Updates the simulation tick counters and phase.
 * @param state The simulation state to update.
 */
void updateSimulationState(WedsSimulationState& state) {
    state.tick++;

    if (state.tick <= WEDS_SIMULATION_WARMUP_SAMPLES) {
        state.in_warmup = true;
        state.mode_tick = 0;
    } else {
        state.in_warmup = false;
        state.mode_tick++;
    }
}

/**
 * @brief Generates a simulated sensor sample applying the selected simulation mode logic.
 * @param state The current simulation state.
 * @return WedsSensorSample A generated sensor sample.
 */
WedsSensorSample generateSimulatedSample(WedsSimulationState& state) {
    const float tick = static_cast<float>(state.tick);
    const float mode_tick = static_cast<float>(state.mode_tick);

    float temperature = 30.0f + sinf(0.10f * tick) + sampleNormal(0.0f, 0.35f);
    float humidity = 76.0f - 3.0f * sinf(0.10f * tick) + sampleNormal(0.0f, 1.0f);
    float pressure = 95000.0f + sampleNormal(0.0f, 500.0f);
    float gas_resistance =
        18000.0f - 80.0f * sinf(0.06f * tick) + sampleNormal(0.0f, 120.0f);

    if (!state.in_warmup) {
        switch (WEDS_SELECTED_SIMULATION_MODE) {
            case WEDS_SIM_MODE_DRY_PERIOD:
                temperature += min(0.08f * mode_tick, 6.0f);
                humidity -= min(0.25f * mode_tick, 18.0f);
                break;

            case WEDS_SIM_MODE_GAS_DROP:
                if (state.mode_tick >= 5 && state.mode_tick <= 8) {
                    gas_resistance -= 2500.0f;
                }
                break;

            case WEDS_SIM_MODE_FIRE_EVENT:
                if (state.mode_tick >= 5 && state.mode_tick <= 16) {
                    float phase = static_cast<float>(state.mode_tick - 5) / 11.0f;
                    phase = constrain(phase, 0.0f, 1.0f);
                    temperature += 2.0f + 4.0f * phase;
                    humidity -= 4.0f + 8.0f * phase;
                    gas_resistance -= 1500.0f + 3500.0f * phase;
                }
                break;

            case WEDS_SIM_MODE_SENSOR_FAULT:
                if (!state.fault_initialized) {
                    state.fault_temperature = temperature;
                    state.fault_humidity = humidity;
                    state.fault_pressure = pressure;
                    state.fault_gas_resistance = gas_resistance;
                    state.fault_initialized = true;
                }
                gas_resistance = state.fault_gas_resistance;
                break;

            case WEDS_SIM_MODE_NORMAL:
            default:
                break;
        }
    }

    WedsSensorSample sample{};
    sample.temperature = temperature;
    sample.humidity = humidity;
    sample.pressure = pressure;
    sample.gas_resistance = gas_resistance;
    return sample;
}

/**
 * @brief Prints a simulated sensor sample to the serial console.
 * @param sample The sample to print.
 */
void printSample(const WedsSensorSample& sample) {
    Serial.printf(
        "[SENSOR_SIM] temp=%.2f C hum=%.2f %% pressure=%.2f Pa gas=%.0f\n",
        sample.temperature,
        sample.humidity,
        sample.pressure,
        sample.gas_resistance
    );
}

/**
 * @brief Prints the current simulation phase based on the selected mode.
 */
void printSimulationPhase() {
    Serial.print("[SENSOR_SIM] phase=");

    if (weds_simulation_state.in_warmup) {
        Serial.println("WARMUP_NORMAL");
        return;
    }

    switch (WEDS_SELECTED_SIMULATION_MODE) {
        case WEDS_SIM_MODE_NORMAL:
            Serial.println("NORMAL");
            break;
        case WEDS_SIM_MODE_DRY_PERIOD:
            Serial.println("DRY_PERIOD");
            break;
        case WEDS_SIM_MODE_GAS_DROP:
            Serial.println("GAS_DROP");
            break;
        case WEDS_SIM_MODE_FIRE_EVENT:
            Serial.println("FIRE_EVENT");
            break;
        case WEDS_SIM_MODE_SENSOR_FAULT:
            Serial.println("SENSOR_FAULT");
            break;
        default:
            Serial.println("UNKNOWN");
            break;
    }
}

}  // namespace

WedsSimulationState weds_simulation_state = {
    0,
    0,
    true,
    false,
    0.0f,
    0.0f,
    0.0f,
    0.0f
};

WedsSensorSample weds_read_simulated_sample() {
    updateSimulationState(weds_simulation_state);
    const WedsSensorSample sample = generateSimulatedSample(weds_simulation_state);
    printSample(sample);
    printSimulationPhase();
    return sample;
}
