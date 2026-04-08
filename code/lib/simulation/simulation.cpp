#include "simulation.hpp"
#include "utils.hpp"

// Choose the scenario you want to test after warmup
constexpr sim_mode_t SELECTED_MODE = SIM_MODE_FIRE_EVENT;

sim_state_t sim_state = {
    0,      // tick
    0,      // mode_tick
    true,   // in_warmup
    false,  // fault_initialized
    0.0f,   // fault_temp_value
    0.0f,   // fault_hum_value
    0.0f,   // fault_press_value
    0.0f    // fault_gas_r_value
};

void updateSimulationState(sim_state_t* s){
    s->tick++;

    if (s->tick <= ANOMALY_WARMUP_SAMPLES) {
        s->in_warmup = true;
        s->mode_tick = 0;
    } else {
        s->in_warmup = false;
        s->mode_tick++;
    }
}

sensors_sample_t generateSimulatedSample(sim_state_t* s){
    sensors_sample_t sample;

    float t = (float)s->tick;
    float mt = (float)s->mode_tick;

    // -----------------------------
    // Base "normal" environment
    // -----------------------------
    float temp_base_cycle = 1.0f * sinf(0.10f * t);
    float hum_base_cycle  = -3.0f * sinf(0.10f * t);
    float gas_base_cycle  = -80.0f * sinf(0.06f * t);

    float temp_noise  = sampleNormal(0.0f, 0.35f);
    float hum_noise   = sampleNormal(0.0f, 1.0f);
    float press_noise = sampleNormal(0.0f, 0.02f);
    float gas_noise   = sampleNormal(0.0f, 120.0f);

    float temp = 30.0f + temp_base_cycle + temp_noise;
    float hum  = 76.0f + hum_base_cycle + hum_noise;
    float press = 0.60f + press_noise;
    float gas_r = 18000.0f + gas_base_cycle + gas_noise;

    // During warmup always stay in normal mode
    if (s->in_warmup) {
        sample.temp = temp;
        sample.hum = hum;
        sample.press = press;
        sample.gas_r = gas_r;
        return sample;
    }

    // -----------------------------
    // Scenario after warmup
    // -----------------------------
    switch (SELECTED_MODE) {

        case SIM_MODE_NORMAL:
            // keep normal behavior
            break;

        case SIM_MODE_DRY_PERIOD: {
            // slow temperature rise and slow humidity decrease
            float dry_temp_ramp = 0.08f * mt;   // slowly rising
            float dry_hum_drop  = 0.25f * mt;   // slowly decreasing

            // Optional clamp so it does not become absurd
            if (dry_temp_ramp > 6.0f) dry_temp_ramp = 6.0f;
            if (dry_hum_drop > 18.0f) dry_hum_drop = 18.0f;

            temp += dry_temp_ramp;
            hum  -= dry_hum_drop;
            break;
        }

        case SIM_MODE_GAS_DROP: {
            // short gas resistance drop after a few post-warmup samples
            if (s->mode_tick >= 5 && s->mode_tick <= 8) {
                gas_r -= 2500.0f;
            }
            break;
        }

        case SIM_MODE_FIRE_EVENT: {
            // multi-sensor coherent event, gradual but significant
            // starts a few samples after warmup and lasts ~12 samples
            if (s->mode_tick >= 5 && s->mode_tick <= 16) {
                float phase = (float)(s->mode_tick - 5) / 11.0f; // 0..1
                if (phase < 0.0f) phase = 0.0f;
                if (phase > 1.0f) phase = 1.0f;

                temp += 2.0f + 4.0f * phase;
                hum  -= 4.0f + 8.0f * phase;
                gas_r -= 1500.0f + 3500.0f * phase;
            }
            break;
        }

        case SIM_MODE_SENSOR_FAULT: {
            // Example: gas resistance sensor gets stuck after warmup
            if (!s->fault_initialized) {
                s->fault_temp_value = temp;
                s->fault_hum_value = hum;
                s->fault_press_value = press;
                s->fault_gas_r_value = gas_r;
                s->fault_initialized = true;
            }

            // freeze gas resistance only; others continue normally
            gas_r = s->fault_gas_r_value;

            // You can alternatively freeze temp/hum instead if you want:
            // temp = s->fault_temp_value;
            // hum = s->fault_hum_value;
            break;
        }

        default:
            break;
    }

    sample.temp = temp;
    sample.hum = hum;
    sample.press = press;
    sample.gas_r = gas_r;

    return sample;
}
