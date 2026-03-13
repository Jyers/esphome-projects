#pragma once
#include "esphome/core/preferences.h"

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/application.h"
#include "types.h"

namespace esphome {
namespace levoit {

// forward declaration 
class LevoitSwitch;  
class LevoitNumber;  
class LevoitFan;
class LevoitSensor;
class LevoitSelect;
class LevoitTextSensor;
class LevoitBinarySensor;  
class LevoitButton;





class Levoit : public Component, public uart::UARTDevice {
 public:
  const char* get_version() const { return "1.1.0 esphome"; }

  // called from python codegen
  void register_switch(SwitchType type, LevoitSwitch *sw);
  void register_number(NumberType type, LevoitNumber *nm);
  void register_sensor(SensorType type, LevoitSensor *se);
  void register_select(SelectType type, LevoitSelect *sl);
  void register_text_sensor(TextSensorType type, LevoitTextSensor *tsl);
  
  // called by switch, number,... when HA toggles it
  void on_switch_command(SwitchType type, bool state);
  void on_number_command(NumberType type, uint32_t value);  
  void on_select_command(SelectType type, uint32_t value);

  // called by decoder when device reports status
  void publish_switch(SwitchType type, bool state);
  void publish_number(NumberType type, uint32_t value);
  void publish_sensor(SensorType type, uint32_t value);
  void publish_select(SelectType type, uint32_t value);
  void publish_text_sensor(TextSensorType type, const std::string &value);
  // binary sensor + button
  void register_binary_sensor(BinarySensorType type, class LevoitBinarySensor *bs);
  void register_button(ButtonType type, class LevoitButton *btn);
  void publish_binary_sensor(BinarySensorType type, bool state);
  // helper to compute and publish filter stats immediately
  void publish_filter_stats_now();

      
        

 



  // called by fan when HA toggles it
  void on_fan_command(int power, int speed_level, int mode);

  void setup() override;
  void loop() override;
  void dump_config() override;
  void set_device_model(std::string model);
  void process_message(uint8_t *msg, int len);
  void track_cadr_usage();  // Track CADR and runtime based on fan state
  uint32_t calculate_current_cadr_per_hour() const;  // Compute current CADR/h based on speed and model
  float calculate_filter_life_left_percent() const;  // Remaining filter life percentage (0-100 with decimals)
  void sendCommand(CommandType commandType);   // if CommandType exists in your project
  void ackMessage(uint8_t ptype0, uint8_t ptype1);
  void ackFilterReset(uint8_t ptype0, uint8_t ptype1);
  void set_fan(LevoitFan *fan) { this->fan_ = fan; }
  LevoitFan *get_fan() const { return this->fan_; }
  LevoitNumber *get_number(NumberType type) const { return numbers_[nt_idx_(type)]; }
  LevoitSensor *sensors_ptr(SensorType type) const { return sensors_[st_idx_(type)]; }
  class LevoitBinarySensor *get_binary_sensor(BinarySensorType type) const { return binary_sensors_[bs_idx_(type)]; }
  bool get_binary_sensor_state(BinarySensorType type) const { return binary_sensor_states_[bs_idx_(type)]; }
  void start_timer(){this->timer_active_ = true;};
  void stop_timer(){this->timer_active_ = false;}; 
  bool is_timer_active() const { return this->timer_active_; };
  
  // ESP-managed timer (for superior devices)
  void start_esp_timer(uint32_t duration_secs);
  void stop_esp_timer();
  void send_timer_update(uint32_t remaining_secs);
  
  // Dry mode preference (for superior devices)
  uint8_t get_dry_level_preference() const { return dry_level_preference_; }
  void set_dry_level_preference(uint8_t level) { dry_level_preference_ = level; }
  
  // Getters for internally tracked values
  uint32_t get_used_cadr() const { return used_cadr_; }
  uint32_t get_total_runtime() const { return total_runtime_; }
  ModelType get_model() const { return model_; }
  
  // Setters for internally tracked values (auto-saves to preferences)
  void set_used_cadr(uint32_t value) {
    used_cadr_ = value;
    pref_used_cadr_.save(&used_cadr_);
  }
  void set_total_runtime(uint32_t value) {
    total_runtime_ = value;
    pref_total_runtime_.save(&total_runtime_);
  }
  
 
  protected:
  LevoitSwitch *switches_[16] {nullptr};  // enough for your types; or use exact count
  LevoitNumber *numbers_[16] {nullptr};  // enough for your types; or use exact count
  LevoitSensor *sensors_[16] {nullptr};  // enough for your types; or use exact count
  LevoitSelect *selects_[16] {nullptr};  // enough for your types; or use exact count
  LevoitTextSensor *text_sensor_[16] {nullptr};  // enough for your types; or use exact count
  class LevoitBinarySensor *binary_sensors_[8] {nullptr};
  bool binary_sensor_states_[8] {false};
  class LevoitButton *buttons_[4] {nullptr};
  LevoitFan *fan_{nullptr};
  ModelType model_{ModelType::VITAL100S};

  uint16_t cadr = 235;
  uint8_t buffer_[512];
  size_t buf_len_{0};

  bool timer_active_{true};
  bool wifi_led_solid_{false};
  bool filter_led_on_{false};
  bool filter_blinking_{false};
  
  // ESP-managed timer state (for superior devices)
  bool esp_timer_active_{false};
  uint32_t esp_timer_start_millis_{0};
  uint32_t esp_timer_duration_secs_{0};
  uint32_t esp_timer_last_update_{0};
  uint8_t esp_timer_zero_count_{0};
  
  // Dry mode preference (for superior devices)
  // 0 = Low, 1 = High
  uint8_t dry_level_preference_{0};
  
  // Internal tracked values (persisted in preferences)
  uint32_t used_cadr_{0};
  uint32_t total_runtime_{0};
  ESPPreferenceObject pref_used_cadr_;
  ESPPreferenceObject pref_total_runtime_;
  // helper to convert enum to index
  static constexpr uint8_t st_idx_(SwitchType t) { return (uint8_t)t; }
  static constexpr uint8_t nt_idx_(NumberType t) { return (uint8_t)t; }
  static constexpr uint8_t st_idx_(SensorType t) { return (uint8_t)t; }
  static constexpr uint8_t sl_idx_(SelectType t) { return (uint8_t)t; }
  static constexpr uint8_t tsl_idx_(TextSensorType t) { return (uint8_t)t; }
  static constexpr uint8_t bs_idx_(BinarySensorType t) { return (uint8_t)t; }
};

}  // namespace levoit
}  // namespace esphome