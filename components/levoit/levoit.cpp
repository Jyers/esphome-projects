#include "levoit.h"
#include "levoit_message.h"
#include "esphome/components/network/util.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/util.h"
#include "esphome/components/wifi/wifi_component.h"

#include <stdio.h>
#include <string.h>
#include <cstdint>
#include <time.h>
#include <vector>
#include <functional>
#include "freertos/task.h"
#include "decoder.h"
#include "tlv.h"
#include "vital_status.h"
#include "core_status.h"
#include "core_commands.h"
#include "vital_commands.h"
#include "superior_commands.h"
#include "decoder_helpers.h"
#include "switch/levoit_switch.h"
#include "fan/levoit_fan.h"
#include "number/levoit_number.h"
#include "sensor/levoit_sensor.h"
#include "select/levoit_select.h"
#include "text_sensor/levoit_text_sensor.h"

namespace esphome
{
    namespace levoit
    {

        static const char *const TAG = "levoit";

        // ===== Component =====

        void Levoit::register_switch(SwitchType type, LevoitSwitch *sw)
        {
            if (!sw)
                return;
            switches_[st_idx_(type)] = sw;
        }
        void Levoit::register_number(NumberType type, LevoitNumber *nm)
        {
            if (!nm)
                return;
            numbers_[nt_idx_(type)] = nm;
        }
        void Levoit::register_sensor(SensorType type, LevoitSensor *se)
        {
            if (!se)
                return;
            sensors_[st_idx_(type)] = se;
        }
        void Levoit::register_select(SelectType type, LevoitSelect *sl)
        {
            if (!sl)
                return;
            selects_[sl_idx_(type)] = sl;
        }
        void Levoit::register_text_sensor(TextSensorType type, LevoitTextSensor *tsl)
        {
            if (!tsl)
                return;
            text_sensor_[static_cast<uint8_t>(type)] = tsl;
        }
        void Levoit::register_binary_sensor(BinarySensorType type, LevoitBinarySensor *bs)
        {
            if (!bs)
                return;
            binary_sensors_[bs_idx_(type)] = bs;
        }
        void Levoit::register_button(ButtonType type, LevoitButton *btn)
        {
            if (!btn)
                return;
            buttons_[static_cast<uint8_t>(type)] = btn;
        }
        void Levoit::publish_switch(SwitchType type, bool state)
        {
            auto *sw = switches_[st_idx_(type)];
            if (!sw)
                return;

            // avoid redundant publishes (helps “loop feel”)
            if (sw->state == state)
                return;
            sw->publish_state(state);
        }

        void Levoit::publish_sensor(SensorType type, float value)
        {
            auto *se = sensors_[st_idx_(type)];
            if (!se)
                return;

            // avoid redundant publishes (helps “loop feel”)

            if (se->has_state() && se->state == value)
                return;
            se->publish_state(value);
        }

        void Levoit::publish_number(NumberType type, float value)
        {
            auto *nm = numbers_[nt_idx_(type)];
            if (!nm)
                return;

            // avoid redundant publishes (helps “loop feel”)

            if (nm->has_state() && nm->state == value)
                return;
            nm->publish_state(value);
        }
        void Levoit::publish_select(SelectType type, uint32_t value)
        {
            auto *sl = selects_[sl_idx_(type)];
            if (!sl)
                return;

            const auto &options = sl->traits.get_options();

            // bounds check: value is an index into options
            if (value >= options.size())
            {
                ESP_LOGW(TAG, "publish_select: invalid index %u for type %d (options=%u)",
                         value, (int)type, (unsigned)options.size());
                return;
            }

            const std::string &opt = options[value];

            // avoid redundant publishes
            if (sl->has_state() && sl->current_option() == opt)
                return;

            sl->publish_state(opt);
        }
        void Levoit::publish_text_sensor(TextSensorType type, const std::string &value)
        {
            auto *tsl = text_sensor_[static_cast<uint8_t>(type)];
            if (!tsl)
                return;

            // avoid redundant publishes
            if (tsl->has_state() && tsl->state == value)
                return;

            tsl->publish_state(value);
        }
        void Levoit::publish_binary_sensor(BinarySensorType type, bool state)
        {
            // Store the desired state; platform entity will publish from its loop
            binary_sensor_states_[bs_idx_(type)] = state;
        }
        void Levoit::publish_filter_stats_now()
        {
            // Publish filter life percent as float
            float filter_left = this->calculate_filter_life_left_percent();
            auto *se = this->sensors_[st_idx_(SensorType::FILTER_LIFE_LEFT)];
            if (se != nullptr)
                se->publish_state(filter_left);
            // Publish binary sensor state based on 5% threshold
            this->publish_binary_sensor(BinarySensorType::FILTER_LOW, filter_left < 5.0f);
        }
        void Levoit::on_switch_command(SwitchType type, bool state)
        {
            // Optional: restrict by model
            // if (model_ != ModelType::VITAL200S && type == SwitchType::QUICK_CLEAN) return;

            switch (type)
            {
            case SwitchType::DISPLAY:
                this->sendCommand(state ? setDisplayOn : setDisplayOff);
                break;

            case SwitchType::CHILD_LOCK:
                this->sendCommand(state ? setDisplayLockOn : setDisplayLockOff);
                break;

            case SwitchType::LIGHT_DETECT:
                this->sendCommand(state ? setLightDetectOn : setLightDetectOff);
                break;

            // You’ll need to add command types for these if not present yet:
            case SwitchType::QUICK_CLEAN:
                // TODO: sendCommand(state ? setQuickCleanOn : setQuickCleanOff);
                break;

            case SwitchType::WHITE_NOISE:
                // TODO: sendCommand(state ? setWhiteNoiseOn : setWhiteNoiseOff);
                break;

            case SwitchType::AUTO_DRY_POWER_OFF:
                this->sendCommand(state ? setAutoDryPowerOffOn : setAutoDryPowerOffOff);
                break;

            case SwitchType::AUTO_DRY_WATER_EMPTY:
                this->sendCommand(state ? setAutoDryWaterEmptyOn : setAutoDryWaterEmptyOff);
                break;

            default:
                break;
            }
        }

        void Levoit::on_number_command(NumberType type, float value)
        {
            // Optional: restrict by model
            // if (model_ != ModelType::VITAL200S && type == NumberType::EFFICIENCY_ROOM_SIZE) return;

            switch (type)
            {
            case NumberType::TIMER:
                if (this->model_ == ModelType::SUPERIOR6000S)
                {
                    // For superior: ESP manages the timer countdown (value is in hours)
                    uint32_t secs = static_cast<uint32_t>(value * 3600);
                    this->sendCommand(setTimerMinutes);
                    if (secs > 0)
                    {
                        this->start_esp_timer(secs);
                        uint16_t mins = secs / 60;
                        this->publish_text_sensor(TextSensorType::TIMER_DURATION_INITIAL, format_duration_minutes(mins));
                        // Immediately publish timer remaining (same as initial at start)
                        this->publish_sensor(SensorType::TIMER_CURRENT, value);
                        this->publish_text_sensor(TextSensorType::TIMER_DURATION_CURRENT, format_duration_minutes(mins));
                    }
                    else
                    {
                        this->stop_esp_timer();
                        this->publish_sensor(SensorType::TIMER_CURRENT, 0.0f);
                        this->publish_text_sensor(TextSensorType::TIMER_DURATION_CURRENT, format_duration_minutes(0));
                    }
                }
                else
                {
                    this->sendCommand(setTimerMinutes);
                }
                break;

            case NumberType::EFFICIENCY_ROOM_SIZE:
                this->sendCommand(setAutoModeEfficient); // takes value from number: Room Size
                break;

            case NumberType::HUMIDITY_TARGET:
                this->sendCommand(setHumidityTarget);
                break;
            }
        }
        void Levoit::on_select_command(SelectType type, uint32_t value)
        {
            // Optional: restrict by model
            // if (model_ != ModelType::VITAL200S && type == SwitchType::QUICK_CLEAN) return;

            switch (type)
            {
            case SelectType::AUTO_MODE:
                switch (value)
                {
                case 0:
                    this->sendCommand(setAutoModeDefault);
                    break;
                case 1:
                    this->sendCommand(setAutoModeQuiet);
                    break;
                case 2:
                    this->sendCommand(setAutoModeEfficient);
                    break;
                default:
                    break;
                }
                break;

            case SelectType::SLEEP_MODE:
                switch (value)
                {
                case 0:
                    this->sendCommand(setSleepModeDefault);
                    break;
                case 1:
                    // this->sendCommand(setSleepModeCustom);
                    break;
                default:
                    break;
                }
                break;

            case SelectType::AUTO_PROFILE:
                switch (value)
                {
                case 0:
                    this->sendCommand(setAutoProfileHome);
                    break;
                case 1:
                    this->sendCommand(setAutoProfileAway);
                    break;
                default:
                    break;
                }
                break;

            case SelectType::HUMIDITY_SUBTYPE:
                switch (value)
                {
                case 0:
                    this->sendCommand(setHumiditySubtypeSmart);
                    break;
                case 1:
                    this->sendCommand(setHumiditySubtypeFan);
                    break;
                default:
                    break;
                }
                break;

            case SelectType::DRY_LEVEL:
                // Store the dry level preference for when Dry mode is selected from the fan entity
                this->dry_level_preference_ = (value <= 1) ? value : 0;
                switch (value)
                {
                case 0:
                    this->sendCommand(setDryLevelLow);
                    break;
                case 1:
                    this->sendCommand(setDryLevelHigh);
                    break;
                default:
                    break;
                }
                break;

            default:
                break;
            }
        }
        void Levoit::on_fan_command(int power, int speed_level, int mode)
        {
            ESP_LOGD(TAG, "on_fan_command: power=%d speed_level=%d mode=%d", power, speed_level, mode);
            // Optional: restrict by model
            // if (model_ != ModelType::VITAL200S && type == SwitchType::QUICK_CLEAN) return;

            if (power != -1)
            {
                this->sendCommand(power == 1 ? setDeviceON : setDeviceOFF);
            }
            if (speed_level != -1)
            {
                switch (speed_level)
                {
                case 1:
                    this->sendCommand(setDeviceFanLvl1);
                    break;
                case 2:
                    this->sendCommand(setDeviceFanLvl2);
                    break;
                case 3:
                    this->sendCommand(setDeviceFanLvl3);
                    break;
                case 4:
                    this->sendCommand(setDeviceFanLvl4);
                    break;
                case 5:
                    this->sendCommand(setDeviceFanLvl5);
                    break;
                case 6:
                    this->sendCommand(setDeviceFanLvl6);
                    break;
                case 7:
                    this->sendCommand(setDeviceFanLvl7);
                    break;
                case 8:
                    this->sendCommand(setDeviceFanLvl8);
                    break;
                case 9:
                    this->sendCommand(setDeviceFanLvl9);
                    break;
                default:
                    break;
                }
            }
            if (mode != -1)
            {
                switch (mode)
                {
                case 0:
                    this->sendCommand(setFanModeManual);
                    break;
                case 1:
                    this->sendCommand(setFanModeSleep);
                    break;
                case 2:
                    this->sendCommand(setFanModeAuto);
                    break;
                case 3:
                    this->sendCommand(setFanModeHumidity);
                    break;
                case 5:
                    this->sendCommand(setFanModePet);
                    break;
                case 6:
                    // Dry mode: use stored dry level preference
                    if (this->dry_level_preference_ == 1)
                        this->sendCommand(setDryLevelHigh);
                    else
                        this->sendCommand(setDryLevelLow);
                    break;
                default:
                    break;
                }
            }
        }

        void Levoit::dump_config() {}

        void Levoit::set_device_model(std::string model)
        {
            if (model == "VITAL200S")
                model_ = ModelType::VITAL200S;
            else if (model == "VITAL100S")
                model_ = ModelType::VITAL100S;
            else if (model == "CORE300S")
                model_ = ModelType::CORE300S;
            else if (model == "CORE400S")
                model_ = ModelType::CORE400S;
            else if (model == "SUPERIOR6000S")
                model_ = ModelType::SUPERIOR6000S;

            ESP_LOGI(TAG, "Model set to: %s (ModelType=%d)", model.c_str(), (int)model_);
        }
        void Levoit::setup()
        {
            ESP_LOGI(TAG, "Setting up Levoit %s", model_ == ModelType::VITAL200S ? "VITAL200S" : "VITAL100S");
            //https://docs.google.com/spreadsheets/d/17j6FZwvqHRFkGoH5996u5JdR7tk4_7fNuTxAK7kc4Fk/edit?gid=1612245341#gid=1612245341
            if (model_ == ModelType::VITAL200S)
                cadr = 415;
            if (model_ == ModelType::VITAL100S)
                cadr = 221;
            if (model_ == ModelType::CORE300S)
                cadr = 214; 
            if (model_ == ModelType::CORE400S)
                cadr = 415;
            if (model_ == ModelType::CORE200S)
                cadr = 167;
            if (model_ == ModelType::CORE600S)
                cadr = 641;
            if (model_ == ModelType::SUPERIOR6000S)
                cadr = 500;
            
            // Initialize preferences for tracking used_cadr and total_runtime
            pref_used_cadr_ = global_preferences->make_preference<uint32_t>(fnv1_hash("levoit_used_cadr"));
            pref_total_runtime_ = global_preferences->make_preference<uint32_t>(fnv1_hash("levoit_runtime"));
            
            // Restore saved values or initialize to 0
            if (pref_used_cadr_.load(&used_cadr_)) {
                ESP_LOGI(TAG, "Restored used_cadr: %u m³", used_cadr_);
            } else {
                used_cadr_ = 0;
                ESP_LOGI(TAG, "Initialized used_cadr to 0");
            }
            if (pref_total_runtime_.load(&total_runtime_)) {
                ESP_LOGI(TAG, "Restored total_runtime: %u hours", total_runtime_);
            } else {
                total_runtime_ = 0;
                ESP_LOGI(TAG, "Initialized total_runtime to 0");
            }
                               
            // Set LED to blink on initial connect until WiFi is connected
            this->sendCommand(CommandType::setWifiLedBlinking);
            this->sendCommand(CommandType::setFilterLedOn);
            filter_led_on_ = true;
            filter_blinking_ = true;
            
            // Track CADR on initial setup
            track_cadr_usage();
        }
        
        void Levoit::track_cadr_usage()
        {
            // Get fan state - check if fan is ON using .state member
            if (this->fan_ != nullptr && this->fan_->state) {
                // Fan is enabled - track usage
                total_runtime_++;
                
                // Get fan speed level (1-4) from .speed member
                int speed = this->fan_->speed;
                if (speed > 0 && speed <= 4) {
                    // Use helper to compute current CADR/hour, then convert to per-minute
                    uint32_t cadr_per_hour = this->calculate_current_cadr_per_hour();
                    uint32_t cadr_per_min = cadr_per_hour / 60;
                    used_cadr_ += cadr_per_min;
                    ESP_LOGD(TAG, "CADR tracked: +%u m³ (speed=%d, total=%u m³, runtime=%u min)", 
                             cadr_per_min, speed, used_cadr_, total_runtime_);
                    
                }
                
                // Calculate and publish filter life left (once per minute here)
                float filter_left = this->calculate_filter_life_left_percent();
                auto *se = this->sensors_[st_idx_(SensorType::FILTER_LIFE_LEFT)];
                if (se != nullptr)
                    se->publish_state(filter_left);
                
                // Save to preferences every minute when running
                pref_used_cadr_.save(&used_cadr_);
                pref_total_runtime_.save(&total_runtime_);
            }
        }

        uint32_t Levoit::calculate_current_cadr_per_hour() const
        {
            if (this->fan_ == nullptr || !this->fan_->state)
                return 0;
            int speed = this->fan_->speed;
            // Determine max speed based on model (Core300S has 3 speeds, Superior has 9)
            uint32_t max_speed = (this->model_ == ModelType::CORE300S) ? 3u :
                                 (this->model_ == ModelType::SUPERIOR6000S) ? 9u : 4u;
            if (speed <= 0 || (uint32_t)speed > max_speed)
                return 0;
            uint32_t result = (cadr * (uint32_t)speed) / max_speed;

            // Sleep mode derates level 1 to 63%
            esphome::StringRef preset = this->fan_->get_preset_mode();
            if (!preset.empty() && preset == "Sleep" && speed == 1)
            {
                result = (uint32_t)(result * 0.63f);
            }

            return result;
        }

        float Levoit::calculate_filter_life_left_percent() const
        {
            auto *filter_lifetime_num = this->get_number(NumberType::FILTER_LIFETIME_MONTHS);
            if (filter_lifetime_num == nullptr || !filter_lifetime_num->has_state())
                return 100.0f; // 100%

            float filter_lifetime_months = filter_lifetime_num->state;
            uint32_t total_filter_capacity = cadr * 24 * 30 * filter_lifetime_months;
            if (total_filter_capacity == 0)
                return 100.0f;

            float life_left_percent = 100.0f - ((float)used_cadr_ / (float)total_filter_capacity * 100.0f);
            if (life_left_percent < 0.0f)
                life_left_percent = 0.0f;
            if (life_left_percent > 100.0f)
                life_left_percent = 100.0f;

            return life_left_percent;
        }

        /// @brief The main loop, that is triggeed by the esphome framework automatically
        void Levoit::loop()
        {
            static uint32_t last_check, last_check_sec = 0;
            static uint32_t last_check_min = 30000;
            static uint32_t last_cadr_check = 0;
            static uint32_t last_filter_check = 0;
            uint32_t now = millis();
            
            // Every minute: track CADR usage and runtime
            if (now - last_check_min >= 60000)
            {
                last_check_min = now;
                track_cadr_usage();
            }
            
            if (now - last_check_sec >= 1000)
            {
                // every second
                last_check_sec = now;
                // Handle filter LED blinking
                if (filter_blinking_)
                {
                    // toggle filter led
                    if (filter_led_on_)
                    {
                        this->sendCommand(CommandType::setFilterLedOff);
                        filter_led_on_ = false;
                    }
                    else
                    {
                        this->sendCommand(CommandType::setFilterLedOn);
                        filter_led_on_ = true;
                    }
                }
                // Check WiFi status and update LED accordingly
                auto *wifi = wifi::global_wifi_component;
                if (wifi != nullptr)
                {
                    bool is_connected = wifi->is_connected();
                    bool is_disabled = wifi->is_disabled();
                    // Determine WiFi state: connecting = not connected and not disabled
                    bool is_connecting = !is_connected && !is_disabled;

                    if (is_connected && !wifi_led_solid_)
                    {
                        ESP_LOGD(TAG, "WiFi connected - setting LED solid");
                        this->sendCommand(CommandType::setWifiLedOn);
                        wifi_led_solid_ = true;
                        this->sendCommand(CommandType::setFilterLedOff);
                        filter_led_on_ = false;
                        filter_blinking_ = false;
                    }
                    else if (is_connecting && wifi_led_solid_)
                    {
                        ESP_LOGD(TAG, "WiFi connecting - setting LED blinking");
                        this->sendCommand(CommandType::setWifiLedBlinking);
                        wifi_led_solid_ = false;
                    }
                    else if (is_disabled && wifi_led_solid_)
                    {
                        ESP_LOGD(TAG, "WiFi disabled - setting LED off");
                        this->sendCommand(CommandType::setWifiLedOff);
                        wifi_led_solid_ = false;
                    }
                }
            }

            // Every 5 seconds: compute and publish current CADR/hour
            if (now - last_cadr_check >= 5000)
            {
                last_cadr_check = now;
                uint32_t current_cadr_hour = this->calculate_current_cadr_per_hour();
                this->publish_sensor(SensorType::CURRENT_CADR, current_cadr_hour);
            }

            // Every 10 seconds: publish filter life left
            if (now - last_filter_check >= 10000)
            {
                last_filter_check = now;
                float filter_left = this->calculate_filter_life_left_percent();
                auto *se = this->sensors_[st_idx_(SensorType::FILTER_LIFE_LEFT)];
                if (se != nullptr)
                    se->publish_state(filter_left);
                // Also update FILTER_LOW binary sensor
                this->publish_binary_sensor(BinarySensorType::FILTER_LOW, filter_left < 5.0f);
            }

            if (this->model_ == ModelType::SUPERIOR6000S)
            {
                // ESP-managed timer for superior devices
                if (this->esp_timer_active_ && now - esp_timer_last_update_ >= 60000)
                {
                    esp_timer_last_update_ = now;
                    uint32_t elapsed_secs = (now - esp_timer_start_millis_) / 1000;
                    uint32_t remaining = (elapsed_secs >= esp_timer_duration_secs_) ? 0 : (esp_timer_duration_secs_ - elapsed_secs);
                    uint16_t remaining_min = remaining / 60;
                    float remaining_hours = remaining / 3600.0f;

                    if (remaining > 0)
                    {
                        ESP_LOGD(TAG, "ESP timer update: %u sec remaining", remaining);
                        this->send_timer_update(remaining);
                        this->publish_sensor(SensorType::TIMER_CURRENT, remaining_hours);
                        this->publish_text_sensor(TextSensorType::TIMER_DURATION_CURRENT, format_duration_minutes(remaining_min));
                    }
                    else
                    {
                        // Timer expired - send 5 zero commands
                        if (esp_timer_zero_count_ < 5)
                        {
                            ESP_LOGD(TAG, "ESP timer expired, sending zero command %u/5", esp_timer_zero_count_ + 1);
                            this->send_timer_update(0);
                            esp_timer_zero_count_++;
                        }
                        else
                        {
                            ESP_LOGI(TAG, "ESP timer finished");
                            this->stop_esp_timer();
                            this->publish_number(NumberType::TIMER, 0.0f);
                            this->publish_sensor(SensorType::TIMER_CURRENT, 0.0f);
                            this->publish_text_sensor(TextSensorType::TIMER_DURATION_CURRENT, format_duration_minutes(0));
                        }
                    }
                }
            }
            else if (this->timer_active_ && now - last_check >= 10000)
            { // 10 seconds
                // timer active? (core/vital MCU-managed timer)
                last_check = now;
                ESP_LOGD(TAG, "Request status - timer");
                this->sendCommand(requestTimerStatus);
            }
            // 1) Read incoming bytes into buffer_
            while (available())
            {
                uint8_t b;
                read_byte(&b);

                if (buf_len_ < sizeof(buffer_))
                {
                    buffer_[buf_len_++] = b;
                }
                else
                {
                    ESP_LOGW(TAG, "RX buffer overflow -> reset");
                    buf_len_ = 0;
                    return;
                }
            }

            // 2) Parse as many frames as possible from the buffer
            while (true)
            {
                if (buf_len_ < 1)
                    break;

                // Find 0xA5
                size_t start = 0;
                while (start < buf_len_ && buffer_[start] != 0xA5)
                    start++;

                if (start == buf_len_)
                { // no start
                    buf_len_ = 0;
                    break;
                }

                // Drop junk before start
                if (start > 0)
                {
                    memmove(buffer_, buffer_ + start, buf_len_ - start);
                    buf_len_ -= start;
                }

                // Need at least 6 bytes (up to checksum byte)
                if (buf_len_ < 6)
                    break;

                const uint8_t size_after_checksum = buffer_[3];
                const size_t expected_len = 6 + size_after_checksum;

                // sanity guard (avoid insane lengths)
                if (expected_len > sizeof(buffer_))
                {
                    ESP_LOGW(TAG, "RX length too big (%u) -> resync", (unsigned)expected_len);
                    memmove(buffer_, buffer_ + 1, buf_len_ - 1);
                    buf_len_ -= 1;
                    continue;
                }

                if (buf_len_ < expected_len)
                    break; // wait more bytes

                // Validate checksum over the WHOLE frame length, excluding byte[5]
                const uint8_t expected_chk = levoit_checksum(buffer_, expected_len);
                const uint8_t got_chk = buffer_[5];

                if (got_chk != expected_chk)
                {
                    ESP_LOGW(TAG, "Bad checksum: got=0x%02X expected=0x%02X; resync", got_chk, expected_chk);
                    // resync: drop start byte and try again
                    memmove(buffer_, buffer_ + 1, buf_len_ - 1);
                    buf_len_ -= 1;
                    continue;
                }

                // Good frame -> you can now split header/payload
                // bytes 0..9 are your "header fields" (if size allows)
                const uint8_t *frame = buffer_;
                const size_t frame_len = expected_len;

                // after-checksum block begins at byte 6
                const uint8_t *after_checksum = frame + 6;
                const size_t after_checksum_len = frame_len - 6;

                // if you want "payload after your 10-byte header"
                const uint8_t *payload = nullptr;
                size_t payload_len = 0;
                if (frame_len >= 10)
                {
                    payload = frame + 10;
                    payload_len = frame_len - 10;
                }

                // Debug log frame
                char hexbuf[1024];
                size_t pos = 0;
                for (size_t i = 0; i < frame_len && pos < sizeof(hexbuf) - 6; i++)
                {
                    pos += snprintf(hexbuf + pos, sizeof(hexbuf) - pos, "0x%02X ", frame[i]);
                }

                ESP_LOGV(TAG, "<<< RX packet (%u bytes): %s", (unsigned)frame_len, hexbuf);

                // Example: log key header bytes if present
                if (frame_len >= 10)
                {
                    ESP_LOGD(TAG,
                             "<<< HEADER: ptype=0x%02X 0x%02X | type=0x%02X ",
                             frame[7], frame[8], frame[1]);
                }
                if (frame_len > 10)
                {
                    const uint8_t *payload = frame + 10;
                    size_t payload_len = frame_len - 10;

                    char phex[512];
                    size_t pos = 0;

                    for (size_t i = 0; i < payload_len && pos < sizeof(phex) - 6; i++)
                    {
                        pos += snprintf(phex + pos, sizeof(phex) - pos, "0x%02X ", payload[i]);
                    }

                    ESP_LOGD(TAG, "<<< PAYLOAD Size: %u bytes", (unsigned)payload_len);
                    ESP_LOGV(TAG, "<<< PAYLOAD: %s", phex);
                }
                else
                {
                    ESP_LOGD(TAG, "<<< PAYLOAD Size: 0 bytes");
                }

                // Process
                process_message((uint8_t *)frame, (int)frame_len);

                // Remove processed packet
                memmove(buffer_, buffer_ + frame_len, buf_len_ - frame_len);
                buf_len_ -= frame_len;
            }

            delay(1);
        }

        /// @brief Processes all messages that are coming from the air purifier secondary chip on UART1
        /// @param msg
        /// @param len
        void Levoit::process_message(std::uint8_t *msg, int len)
        {
            if (len < 10)
                return;
            if (msg[0] != 0xA5)
                return;

            const uint8_t msg_type = msg[1];
            const uint8_t seq = msg[2];
            const uint8_t size_af = msg[3];
            const uint8_t cmd_sz = msg[6];
            const uint8_t ptype0 = msg[7];
            const uint8_t ptype1 = msg[8];

            // Frame sanity
            ESP_LOGD(TAG,
                     "RX: type=0x%02X seq=%u size_after_chk=%u cmdSz=%u ptype=%02X%02X model=%d",
                     msg_type, seq, size_af, cmd_sz, ptype0, ptype1, (int)model_);

            // Payload starts after the 10-byte header
            const uint8_t *payload = nullptr;
            size_t payload_len = 0;

            if (len > 10)
            {
                payload = msg + 10;
                payload_len = len - 10;
            }

            ESP_LOGD(TAG, ">>> Calling dispatch_decoder with model=%d ptype=%02X%02X", (int)model_, ptype0, ptype1);
            dispatch_decoder(this, model_, msg_type, seq, cmd_sz, ptype0, ptype1, payload, payload_len);
        }

        /// @brief Sends the commands depending on its type. Delegates to model-specific command builders.
        /// @param commandType
        void Levoit::sendCommand(CommandType commandType)
        {
            ESP_LOGD(TAG, "Command triggered: %s", command_type_to_string(commandType));
            std::vector<uint8_t> message;

            if (this->model_ == ModelType::CORE300S || this->model_ == ModelType::CORE400S)
            {
                message = build_core_command(this, commandType);
            }
            else if (this->model_ == ModelType::VITAL100S || this->model_ == ModelType::VITAL200S)
            {
                message = build_vital_command(this, commandType);
            }
            else if (this->model_ == ModelType::SUPERIOR6000S)
            {
                message = build_superior_command(this, commandType);
            }

            if (message.size() > 0)
            {
                ESP_LOGI(TAG, ">>> Sending command %s", command_type_to_string(commandType));
                ESP_LOGD(TAG, ">>> TX: %s", format_hex_pretty(message).c_str());
                this->write_array(message.data(), message.size());
                this->flush();
                // update the message counter
                if (messageUpCounter == 255)
                    messageUpCounter = 16;
                else
                    messageUpCounter++;
            }
        }
        /**
         * @brief Sends an acknowledgement message for the received ptype0/1, based on model type
         */
        void Levoit::ackMessage(uint8_t ptype0, uint8_t ptype1)
        {

            uint8_t pv = 0x01;
            if (this->model_ == ModelType::VITAL100S || this->model_ == ModelType::VITAL200S
                || this->model_ == ModelType::SUPERIOR6000S)
            {
                pv = 0x02;
                ESP_LOGI("TAG", ">>> Sending VITAL/SUPERIOR ack for: 0x%02X 0x%02X", ptype0, ptype1);
            }

            std::vector<uint8_t> message = {0xA5, 0x12, 0xFF, 0x04, 0xFF, 0x00, pv, ptype0, ptype1, 0x00};

            levoit_finalize_message(message, messageUpCounter);

            if (message.size() > 0)
            {
                ESP_LOGI(TAG, ">>> Sending ack  for: 0x%02X 0x%02X", ptype0, ptype1);

                this->write_array(message.data(), message.size());
                this->flush();
                // update the message counter
                if (messageUpCounter == 255)
                    messageUpCounter = 16;
                else
                    messageUpCounter++;
            }
        }

        /**
         * @brief Sends a filter-reset acknowledgement (0x52) for the received ptype0/1.
         *        Used when the MCU reports a filter reset initiated from the physical display.
         */
        void Levoit::ackFilterReset(uint8_t ptype0, uint8_t ptype1)
        {
            uint8_t pv = 0x02; // Vital/Superior protocol version

            std::vector<uint8_t> message = {0xA5, 0x52, 0xFF, 0x04, 0x00, 0x00, pv, ptype0, ptype1, 0x16};

            levoit_finalize_message(message, messageUpCounter);

            if (message.size() > 0)
            {
                ESP_LOGI(TAG, ">>> Sending filter reset ack for: 0x%02X 0x%02X", ptype0, ptype1);

                this->write_array(message.data(), message.size());
                this->flush();
                // update the message counter
                if (messageUpCounter == 255)
                    messageUpCounter = 16;
                else
                    messageUpCounter++;
            }
        }

        // --- ESP-managed timer methods (for superior devices) ---
        void Levoit::start_esp_timer(uint32_t duration_secs)
        {
            esp_timer_active_ = true;
            esp_timer_start_millis_ = millis();
            esp_timer_duration_secs_ = duration_secs;
            esp_timer_last_update_ = esp_timer_start_millis_;
            esp_timer_zero_count_ = 0;
            ESP_LOGI(TAG, "ESP timer started: %u seconds", duration_secs);
        }

        void Levoit::stop_esp_timer()
        {
            esp_timer_active_ = false;
            esp_timer_zero_count_ = 0;
            ESP_LOGI(TAG, "ESP timer stopped");
        }

        void Levoit::send_timer_update(uint32_t remaining_secs)
        {
            // Build and send timer command with specific remaining seconds
            // Uses feature_id=0x19 with payload: 01 04 <LE32 seconds>
            std::vector<uint8_t> msg_type = {0x02, 0x19, 0x50};
            uint8_t b0 = (remaining_secs >> 0) & 0xFF;
            uint8_t b1 = (remaining_secs >> 8) & 0xFF;
            uint8_t b2 = (remaining_secs >> 16) & 0xFF;
            uint8_t b3 = (remaining_secs >> 24) & 0xFF;
            std::vector<uint8_t> payload = {0x01, 0x04, b0, b1, b2, b3};

            auto message = build_levoit_message(msg_type, payload, messageUpCounter);
            if (message.size() > 0)
            {
                ESP_LOGD(TAG, ">>> TX timer update: %u sec remaining", remaining_secs);
                this->write_array(message.data(), message.size());
                this->flush();
                if (messageUpCounter == 255)
                    messageUpCounter = 16;
                else
                    messageUpCounter++;
            }
        }

    } // namespace levoit
} // namespace esphome
