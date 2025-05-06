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

    measurement->variant.environment_metrics.has_gas_resistance = true;
    measurement->variant.environment_metrics.gas_resistance = random(0, 10000) / 100.0;

    measurement->variant.environment_metrics.has_iaq = true;
    measurement->variant.environment_metrics.iaq = random(0, 100);

    measurement->variant.environment_metrics.has_distance = true;
    measurement->variant.environment_metrics.distance = random(0, 10000) / 100.0;

    measurement->variant.environment_metrics.has_lux = true;
    measurement->variant.environment_metrics.lux = random(0, 10000) / 100.0;

    measurement->variant.environment_metrics.has_white_lux = true;
    measurement->variant.environment_metrics.white_lux = random(0, 10000) / 100.0;

    measurement->variant.environment_metrics.has_ir_lux = true;
    measurement->variant.environment_metrics.ir_lux = random(0, 10000) / 100.0;

    measurement->variant.environment_metrics.has_uv_lux = true;
    measurement->variant.environment_metrics.uv_lux = random(0, 10000) / 100.0;

    measurement->variant.environment_metrics.has_wind_direction = true;
    measurement->variant.environment_metrics.wind_direction = random(0, 360);

    measurement->variant.environment_metrics.has_wind_speed = true;
    measurement->variant.environment_metrics.wind_speed = random(0, 100) / 10.0;

    measurement->variant.environment_metrics.has_weight = true;
    measurement->variant.environment_metrics.weight = random(0, 10000) / 100.0;

    measurement->variant.environment_metrics.has_wind_gust = true;
    measurement->variant.environment_metrics.wind_gust = random(0, 100) / 10.0;

    measurement->variant.environment_metrics.has_wind_lull = true;
    measurement->variant.environment_metrics.wind_lull = random(0, 100) / 10.0;

    measurement->variant.environment_metrics.has_radiation = true;
    measurement->variant.environment_metrics.radiation = random(0, 10000) / 100.0;

    measurement->variant.environment_metrics.has_rainfall_1h = true;
    measurement->variant.environment_metrics.rainfall_1h = random(0, 100) / 10.0;

    measurement->variant.environment_metrics.has_rainfall_24h = true;
    measurement->variant.environment_metrics.rainfall_24h = random(0, 100) / 10.0;

    measurement->variant.environment_metrics.has_soil_moisture = true;
    measurement->variant.environment_metrics.soil_moisture = random(0, 100);

    measurement->variant.environment_metrics.has_soil_temperature = true;
    measurement->variant.environment_metrics.soil_temperature = random(0, 5000) / 100.0;
    
    LOG_INFO("Random sensor values - Temp: %.2f°C, Humidity: %.2f%%", 
            measurement->variant.environment_metrics.temperature,
            measurement->variant.environment_metrics.relative_humidity);

    LOG_INFO("Random sensor values - Pressure: %.2f hPa, Gas Resistance: %.2f MOhm",
            measurement->variant.environment_metrics.barometric_pressure,
            measurement->variant.environment_metrics.gas_resistance);

    LOG_INFO("Random sensor values - IAQ: %d, Distance: %.2f m", 
            measurement->variant.environment_metrics.iaq,
            measurement->variant.environment_metrics.distance);

    LOG_INFO("Random sensor values - Lux: %.2f, White Lux: %.2f, IR Lux: %.2f, UV Lux: %.2f", 
            measurement->variant.environment_metrics.lux,
            measurement->variant.environment_metrics.white_lux,
            measurement->variant.environment_metrics.ir_lux,
            measurement->variant.environment_metrics.uv_lux);

    LOG_INFO("Random sensor values - Wind Direction: %d°, Wind Speed: %.2f m/s, Wind Gust: %.2f m/s, Wind Lull: %.2f m/s", 
            measurement->variant.environment_metrics.wind_direction,
            measurement->variant.environment_metrics.wind_speed,
            measurement->variant.environment_metrics.wind_gust,
            measurement->variant.environment_metrics.wind_lull);

    LOG_INFO("Random sensor values - Weight: %.2f kg, Radiation: %.2f µSv/h", 
            measurement->variant.environment_metrics.weight,
            measurement->variant.environment_metrics.radiation);

    LOG_INFO("Random sensor values - Rainfall 1h: %.2f mm, Rainfall 24h: %.2f mm", 
            measurement->variant.environment_metrics.rainfall_1h,
            measurement->variant.environment_metrics.rainfall_24h);

    LOG_INFO("Random sensor values - Soil Moisture: %d%%, Soil Temperature: %.2f°C", 
            measurement->variant.environment_metrics.soil_moisture,
            measurement->variant.environment_metrics.soil_temperature);
            
    return true;
}

#endif