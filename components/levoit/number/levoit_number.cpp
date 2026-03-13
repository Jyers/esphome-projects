#include "levoit_number.h"
#include "../levoit.h"
#include "esphome/core/log.h"
#include "esphome/components/number/number_traits.h"

namespace esphome
{
  namespace levoit
  {

    static const char *const TAG = "levoit.number";

    void LevoitNumber::setup() {
      // pick default
      this->traits.set_mode(number::NumberMode::NUMBER_MODE_AUTO);

      switch (this->type_) {
        case NumberType::EFFICIENCY_ROOM_SIZE:
          this->set_entity_category(EntityCategory::ENTITY_CATEGORY_CONFIG);
          this->traits.set_device_class("area");
          this->traits.set_unit_of_measurement("m²");
          this->traits.set_mode(number::NumberMode::NUMBER_MODE_SLIDER);
          this->traits.set_min_value(132);
          this->traits.set_max_value(792);
          this->traits.set_step(14);  // 1 m
      
          break;

        case NumberType::TIMER:
          this->traits.set_device_class("duration");
          this->traits.set_unit_of_measurement("h");
          this->traits.set_mode(number::NumberMode::NUMBER_MODE_SLIDER);
          this->traits.set_min_value(0);
          this->traits.set_max_value(12);
          this->traits.set_step(0.5);
          break;
        case NumberType::FILTER_LIFETIME_MONTHS:
          this->traits.set_device_class("duration");
          this->traits.set_unit_of_measurement("months");
          this->traits.set_mode(number::NumberMode::NUMBER_MODE_SLIDER);
          this->traits.set_min_value(1);
          this->traits.set_max_value(12);
          this->traits.set_step(1);
          this->set_icon("mdi:air-filter");
          this->set_entity_category(EntityCategory::ENTITY_CATEGORY_CONFIG);
          
          // Setup preferences for persistent storage
          pref_ = global_preferences->make_preference<float>(fnv1_hash("levoit_filter_months"));
          
          // Restore saved value or use default
          float saved_value;
          if (pref_.load(&saved_value)) {
            ESP_LOGI(TAG, "Restored filter lifetime: %.0f months", saved_value);
            this->publish_state(saved_value);
          } else {
            ESP_LOGI(TAG, "No saved filter lifetime, using default: 6 months");
            this->publish_state(6.0);
          }
          break;

        case NumberType::HUMIDITY_TARGET:
          this->traits.set_device_class("humidity");
          this->traits.set_unit_of_measurement("%");
          this->traits.set_mode(number::NumberMode::NUMBER_MODE_SLIDER);
          this->traits.set_min_value(30);
          this->traits.set_max_value(80);
          this->traits.set_step(5);
          this->set_icon("mdi:water-percent");
          break;

        default:
          break;
      }
    }
    void LevoitNumber::dump_config() { LOG_NUMBER("", "Levoit Number", this); }

    void LevoitNumber::control(float value)
    {
      // Optimistic update for HA UI
      this->publish_state(value);
      
      // Save to preferences if filter_lifetime_months
      if (this->type_ == NumberType::FILTER_LIFETIME_MONTHS) {
        pref_.save(&value);
        ESP_LOGD(TAG, "Saved filter lifetime: %.0f months", value);
      }
      
      if (!parent_)
      {
        ESP_LOGW(TAG, "No parent set for number");
        return;
      }

      parent_->on_number_command(this->type_, value);
    }

  } // namespace levoit
} // namespace esphome
