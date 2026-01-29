#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/sensirion_common/i2c_sensirion.h"
#include "esphome/core/application.h"
#include "esphome/core/preferences.h"

namespace esphome {
namespace sen6x {

enum ERRORCODE {
  COMMUNICATION_FAILED,
  SERIAL_NUMBER_IDENTIFICATION_FAILED,
  MEASUREMENT_INIT_FAILED,
  PRODUCT_NAME_FAILED,
  FIRMWARE_FAILED,
  UNKNOWN
};

// Shortest time interval of 3H for storing baseline values.
// Prevents wear of the flash because of too many write operations
const uint32_t SHORTEST_BASELINE_STORE_INTERVAL = 10800;
// Store anyway if the baseline difference exceeds the max storage diff value
const uint32_t MAXIMUM_STORAGE_DIFF = 50;

struct Sen6xBaselines {
  int32_t state0;
  int32_t state1;
} PACKED;  // NOLINT


struct GasTuning {
  uint16_t index_offset;
  uint16_t learning_time_offset_hours;
  uint16_t learning_time_gain_hours;
  uint16_t gating_max_duration_minutes;
  uint16_t std_initial;
  uint16_t gain_factor;
};

struct TemperatureCompensation {
  int16_t offset;
  int16_t normalized_offset_slope;
  uint16_t time_constant;
  uint16_t slot;
};

struct TemperatureAcceleration {
  uint16_t k;
  uint16_t p;
  uint16_t t1;
  uint16_t t2;
};

class SEN6XComponent : public PollingComponent, public sensirion_common::SensirionI2CDevice {
 public:
  float get_setup_priority() const override { return setup_priority::DATA; }
  void setup() override;
  void dump_config() override;
  void update() override;

  enum Sen6xType { SEN62, SEN63C, SEN65, SEN66, SEN68, SEN69C, UNKNOWN };

  void set_pm_1_0_sensor(sensor::Sensor *pm_1_0) { pm_1_0_sensor_ = pm_1_0; }
  void set_pm_2_5_sensor(sensor::Sensor *pm_2_5) { pm_2_5_sensor_ = pm_2_5; }
  void set_pm_4_0_sensor(sensor::Sensor *pm_4_0) { pm_4_0_sensor_ = pm_4_0; }
  void set_pm_10_0_sensor(sensor::Sensor *pm_10_0) { pm_10_0_sensor_ = pm_10_0; }

  void set_voc_sensor(sensor::Sensor *voc_sensor) { voc_sensor_ = voc_sensor; }
  void set_nox_sensor(sensor::Sensor *nox_sensor) { nox_sensor_ = nox_sensor; }
  void set_hcho_sensor(sensor::Sensor *hcho_sensor) { hcho_sensor_ = hcho_sensor; }
  void set_humidity_sensor(sensor::Sensor *humidity_sensor) { humidity_sensor_ = humidity_sensor; }
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }
  void set_co2_sensor(sensor::Sensor *co2) { co2_sensor_ = co2; }
  void set_store_baseline(bool store_baseline) { store_baseline_ = store_baseline; }
  void set_voc_algorithm_tuning(uint16_t index_offset, uint16_t learning_time_offset_hours,
                                uint16_t learning_time_gain_hours, uint16_t gating_max_duration_minutes,
                                uint16_t std_initial, uint16_t gain_factor) {
    GasTuning tuning_params;
    tuning_params.index_offset = index_offset;
    tuning_params.learning_time_offset_hours = learning_time_offset_hours;
    tuning_params.learning_time_gain_hours = learning_time_gain_hours;
    tuning_params.gating_max_duration_minutes = gating_max_duration_minutes;
    tuning_params.std_initial = std_initial;
    tuning_params.gain_factor = gain_factor;
    voc_tuning_params_ = tuning_params;
  }
  void set_nox_algorithm_tuning(uint16_t index_offset, uint16_t learning_time_offset_hours,
                                uint16_t learning_time_gain_hours, uint16_t gating_max_duration_minutes,
                                uint16_t gain_factor) {
    GasTuning tuning_params;
    tuning_params.index_offset = index_offset;
    tuning_params.learning_time_offset_hours = learning_time_offset_hours;
    tuning_params.learning_time_gain_hours = learning_time_gain_hours;
    tuning_params.gating_max_duration_minutes = gating_max_duration_minutes;
    tuning_params.std_initial = 50;
    tuning_params.gain_factor = gain_factor;
    nox_tuning_params_ = tuning_params;
  }
  void set_temperature_compensation(float offset, float normalized_offset_slope, uint16_t time_constant, uint16_t slot) {
    TemperatureCompensation temp_comp;
    temp_comp.offset = offset * 200;
    temp_comp.normalized_offset_slope = normalized_offset_slope * 10000;
    temp_comp.time_constant = time_constant;
    temp_comp.slot = slot;
    temperature_compensation_ = temp_comp;
  }
  void set_temperature_acceleration(float k, float p, float t1, float t2) {
    TemperatureAcceleration temp_accel;
    temp_accel.k = k * 10;
    temp_accel.p = p * 10;
    temp_accel.t1 = t1 * 10;
    temp_accel.t2 = t2 * 10;
    temperature_acceleration_ = temp_accel;
  }
  bool start_fan_cleaning();

 protected:
  bool write_tuning_parameters_(uint16_t i2c_command, const GasTuning &tuning);
  bool write_temperature_compensation_(const TemperatureCompensation &compensation);
  bool write_temperature_acceleration_(const TemperatureAcceleration &acceleration);
  ERRORCODE error_code_;
  bool initialized_{false};
  sensor::Sensor *pm_1_0_sensor_{nullptr};
  sensor::Sensor *pm_2_5_sensor_{nullptr};
  sensor::Sensor *pm_4_0_sensor_{nullptr};
  sensor::Sensor *pm_10_0_sensor_{nullptr};
  // SEN54 and SEN55 only
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
  sensor::Sensor *voc_sensor_{nullptr};
  // SEN55 only
  sensor::Sensor *nox_sensor_{nullptr};
  sensor::Sensor *hcho_sensor_{nullptr};
  sensor::Sensor *co2_sensor_{nullptr};

  std::string product_name_;
  Sen6xType sen6x_type_{UNKNOWN};
  uint8_t serial_number_[4];
  uint16_t firmware_version_;
  Sen6xBaselines voc_baselines_storage_;
  bool store_baseline_;
  uint32_t seconds_since_last_store_;
  ESPPreferenceObject pref_;
  optional<GasTuning> voc_tuning_params_;
  optional<GasTuning> nox_tuning_params_;
  optional<TemperatureCompensation> temperature_compensation_;
  optional<TemperatureAcceleration> temperature_acceleration_;
};

}  // namespace sen6x
}  // namespace esphome
