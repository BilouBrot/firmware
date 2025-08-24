#include "configuration.h"

#if !MESHTASTIC_EXCLUDE_ENVIRONMENTAL_SENSOR

#include "../mesh/generated/meshtastic/telemetry.pb.h"
#include "RandomSensor.h"
#include "TelemetrySensor.h"
#include <Arduino.h>

RandomSensor::RandomSensor() : TelemetrySensor(meshtastic_TelemetrySensorType_CUSTOM_SENSOR, "RandomSensor") {}

int32_t RandomSensor::runOnce()
{
    LOG_INFO("Init sensor: %s", sensorName);
    status = true; // Always succeed initialization
    return initI2CSensor();
}

void RandomSensor::setup() {
    // Initialize the random seed
    randomSeed(millis());
}

bool RandomSensor::getMetrics(meshtastic_Telemetry *measurement)
{
    // Add a random temperature value between 0 and 50
    measurement->variant.environment_metrics.has_temperature = true;
    measurement->variant.environment_metrics.temperature = random(0, 5000) / 100.0;
    
    // Optionally add other random metrics
    measurement->variant.environment_metrics.has_relative_humidity = true;
    measurement->variant.environment_metrics.relative_humidity = random(0, 10000) / 100.0;

    measurement->variant.environment_metrics.has_barometric_pressure = true;
    measurement->variant.environment_metrics.barometric_pressure = random(95000, 105000) / 100.0;
            
    return true;
}

#endif