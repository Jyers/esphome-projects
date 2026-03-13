#include "levoit_sensor.h"
#include "../levoit.h"
#include "esphome/core/entity_base.h"



namespace esphome
{
  namespace levoit
  {

    static const char *const TAG = "levoit.sensor";

    void LevoitSensor::setup() {
      // pick default
      // this->traits.set_mode(sensor::SensorMode::NUMBER_MODE_AUTO);

      switch (this->type_) {
        case SensorType::EFFICIENCY_COUNTER:
          this->set_device_class("duration");
          this->set_unit_of_measurement("s");
          this->set_entity_category(EntityCategory::ENTITY_CATEGORY_DIAGNOSTIC);
          break;
        case SensorType::TIMER_CURRENT:
          this->set_device_class("duration");
          this->set_unit_of_measurement("s");
          break;
        case SensorType::PM25:
          this->set_device_class("pm25");
          this->set_unit_of_measurement("µg/m³");
          break;
        case SensorType::AQI:
          this->set_device_class("aqi");
          this->set_icon("mdi:molecule");
          break;
        case SensorType::CURRENT_CADR:
          this->set_device_class("volume_flow_rate");
          this->set_entity_category(EntityCategory::ENTITY_CATEGORY_DIAGNOSTIC);
          this->set_unit_of_measurement("m³/h");
          this->set_icon("mdi:air-filter");
          break;
        case SensorType::FILTER_LIFE_LEFT:
          this->set_unit_of_measurement("%");
          this->set_icon("mdi:air-filter");
          this->set_accuracy_decimals(1);
          break;
        case SensorType::TEMPERATURE:
          this->set_device_class("temperature");
          this->set_unit_of_measurement("°C");
          this->set_accuracy_decimals(1);
          break;
        default:
          break;
      }
    }
    void LevoitSensor::dump_config() { LOG_SENSOR("", "Levoit Sensor", this); }



  } // namespace levoit
} // namespace esphome
