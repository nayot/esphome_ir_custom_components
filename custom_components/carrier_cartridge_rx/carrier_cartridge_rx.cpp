#include "carrier_cartridge_rx.h"
#include "esphome/core/log.h"
#include "esphome/components/climate/climate.h"
// Include remote_base.h BEFORE remote_receiver.h
#include "esphome/components/remote_base/remote_base.h"
#include "esphome/components/remote_receiver/remote_receiver.h"
#include <vector>
#include <optional>
#include <cmath> 
#include <algorithm> 
#include <string> 

namespace esphome {
namespace carrier_cartridge_rx {

static const char *const TAG = "carrier_cartridge_rx";

// ======================================================================
// ===               IR PROTOCOL TIMING DEFINITIONS                   ===
// ======================================================================
static const int32_t HEADER_PULSE_US = 9000;
static const int32_t HEADER_SPACE_US = -4500;
static const int32_t PULSE_DURATION_US = 650;
static const int32_t SPACE_ZERO_US = -500;
static const int32_t SPACE_ONE_US = -1600;
static const int32_t FINAL_PULSE_US = 650;
static const int32_t SPACE_ZERO_MAX_US = 700;
static const int32_t SPACE_ONE_MIN_US = 1300;

// ======================================================================
// ===                HEX "CODE BOOK" FOR SPECIAL CODES               ===
// ======================================================================
static const uint64_t CODE_SWING_ON  = 0xF20D01FE210120FCULL;
static const uint64_t CODE_SWING_OFF = 0xF20D01FE210100DCULL;

// ======================================================================
// ===                DECODER FUNCTION (Keep This)                    ===
// ======================================================================
static std::optional<uint64_t> decode_to_uint64(remote_base::RemoteReceiveData data) {
  if (data.size() < 171) { return std::nullopt; }
  uint64_t decoded_data = 0;
  int bit_count = 0;
  for (size_t i = 2; i < data.size() - 1 && bit_count < 64; i += 2) {
    int32_t space_duration = std::abs(data[i + 1]);
    decoded_data <<= 1;
    if (space_duration >= SPACE_ONE_MIN_US) { // Uses constant
      decoded_data |= 1;
    } else if (space_duration < SPACE_ZERO_MAX_US) { /* 0 bit */ } // Uses constant
    else {
      return std::nullopt;
     }
    bit_count++;
  }
  if (bit_count == 64) { return decoded_data; }
  return std::nullopt;
}

// Renamed helper functions
std::string rx_climate_mode_to_string(climate::ClimateMode mode) {
    switch (mode) {
        case climate::CLIMATE_MODE_OFF: return "OFF";
        case climate::CLIMATE_MODE_COOL: return "COOL";
        case climate::CLIMATE_MODE_HEAT: return "HEAT";
        case climate::CLIMATE_MODE_FAN_ONLY: return "FAN_ONLY";
        case climate::CLIMATE_MODE_DRY: return "DRY";
        case climate::CLIMATE_MODE_AUTO: return "AUTO";
        default: return "UNKNOWN";
    }
}

std::string rx_climate_fan_mode_to_string(climate::ClimateFanMode fan_mode) {
     switch (fan_mode) {
        case climate::CLIMATE_FAN_AUTO: return "AUTO";
        case climate::CLIMATE_FAN_LOW: return "LOW";
        case climate::CLIMATE_FAN_MEDIUM: return "MEDIUM";
        case climate::CLIMATE_FAN_HIGH: return "HIGH";
        default: return "UNKNOWN";
    }
}

std::string rx_climate_swing_mode_to_string(climate::ClimateSwingMode swing_mode) {
    switch (swing_mode) {
        case climate::CLIMATE_SWING_OFF: return "OFF";
        case climate::CLIMATE_SWING_VERTICAL: return "VERTICAL";
        case climate::CLIMATE_SWING_HORIZONTAL: return "HORIZONTAL";
        case climate::CLIMATE_SWING_BOTH: return "BOTH";
        default: return "UNKNOWN";
    }
}

// ======================================================================
// ===                COMPONENT FUNCTIONS                             ===
// ======================================================================

// IMPLEMENT the function here
void CarrierCartridgeRx::set_receiver_component(remote_receiver::RemoteReceiverComponent *receiver) {
  // *** FIX: Use register_listener instead of add_listener ***
  receiver->register_listener(this);
}

void CarrierCartridgeRx::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Carrier Cartridge Receiver...");
}

void CarrierCartridgeRx::dump_config() {
  ESP_LOGCONFIG(TAG, "Carrier Cartridge Receiver:");
  LOG_TEXT_SENSOR("  ", "Mode Sensor", this->mode_sensor_);
  LOG_TEXT_SENSOR("  ", "Fan Mode Sensor", this->fan_mode_sensor_);
  LOG_TEXT_SENSOR("  ", "Target Temp Sensor", this->target_temperature_sensor_);
  LOG_TEXT_SENSOR("  ", "Swing Mode Sensor", this->swing_mode_sensor_);
}

// on_receive: Decodes and publishes to sensors
bool CarrierCartridgeRx::on_receive(remote_base::RemoteReceiveData data) {
  auto decoded_hex = decode_to_uint64(data);
  if (!decoded_hex.has_value()) {
    return false; // Not our code
  }

  uint64_t hex_code = decoded_hex.value();
  ESP_LOGD(TAG, "on_receive: Decoded HEX prefix: 0x%016llX", hex_code);

  climate::ClimateMode received_mode;
  climate::ClimateFanMode received_fan_mode;
  climate::ClimateSwingMode received_swing_mode;
  float received_temp = NAN;
  bool state_decoded = false;

  // Check for special SWING codes first
  if (hex_code == CODE_SWING_ON) { // Uses constant
    ESP_LOGD(TAG, "on_receive: Matched SWING ON code");
    if (this->swing_mode_sensor_ != nullptr && this->swing_mode_sensor_->get_raw_state() != "VERTICAL") {
       this->swing_mode_sensor_->publish_state("VERTICAL");
    }
    return true; // Handled
  } else if (hex_code == CODE_SWING_OFF) { // Uses constant
    ESP_LOGD(TAG, "on_receive: Matched SWING OFF code");
    if (this->swing_mode_sensor_ != nullptr && this->swing_mode_sensor_->get_raw_state() != "OFF") {
       this->swing_mode_sensor_->publish_state("OFF");
    }
    return true; // Handled
  } else {
      // --- Parse the main state code ---
      uint64_t prefix = (hex_code >> 24) & 0xFFFFFFFFFFULL;
      if (prefix != 0xF20D01FE21ULL) {
        return false;
      }
      state_decoded = true;

      uint8_t b5 = (hex_code >> 24) & 0xFF;
      uint8_t b6 = (hex_code >> 16) & 0xFF;
      uint8_t b7 = (hex_code >> 8) & 0xFF;
      uint8_t power_nibble = (b5 >> 4) & 0x0F;
      uint8_t mode_nibble = b5 & 0x0F;

      if (power_nibble == 0x0) { received_mode = climate::CLIMATE_MODE_OFF; }
      else {
        switch (mode_nibble) {
          case 0x8: received_mode = climate::CLIMATE_MODE_COOL; break;
          case 0x1: received_mode = climate::CLIMATE_MODE_HEAT; break;
          case 0x2: received_mode = climate::CLIMATE_MODE_DRY; break;
          case 0x0: received_mode = climate::CLIMATE_MODE_FAN_ONLY; break;
          default: ESP_LOGW(TAG, "on_receive: Unknown mode nibble: 0x%X", mode_nibble); state_decoded=false;
        }
      }

      uint8_t fan_nibble = (b6 >> 4) & 0x0F;
      switch (fan_nibble) {
        case 0x0: received_fan_mode = climate::CLIMATE_FAN_AUTO; break;
        case 0x4: received_fan_mode = climate::CLIMATE_FAN_LOW; break;
        case 0x6: case 0x8: received_fan_mode = climate::CLIMATE_FAN_MEDIUM; break;
        case 0xA: case 0xC: received_fan_mode = climate::CLIMATE_FAN_HIGH; break;
        default: received_fan_mode = climate::CLIMATE_FAN_AUTO; break;
      }

      if (state_decoded && received_mode != climate::CLIMATE_MODE_OFF && received_mode != climate::CLIMATE_MODE_FAN_ONLY) {
        uint8_t temp_nibble = b6 & 0x0F;
        received_temp = 17.0f + temp_nibble;
        received_temp = clamp(received_temp, 17.0f, 30.0f);
      }

      uint8_t swing_nibble = (b7 >> 4) & 0x0F;
      if (swing_nibble == 0x2) { received_swing_mode = climate::CLIMATE_SWING_VERTICAL; }
      else { received_swing_mode = climate::CLIMATE_SWING_OFF; }
  }

  // --- Publish state to sensors if decoded successfully ---
  if (state_decoded) {
      std::string mode_str = rx_climate_mode_to_string(received_mode);
      std::string fan_mode_str = rx_climate_fan_mode_to_string(received_fan_mode);
      std::string swing_mode_str = rx_climate_swing_mode_to_string(received_swing_mode);
      std::string temp_str = "";
      if (!std::isnan(received_temp)) {
          char temp_buffer[10];
          snprintf(temp_buffer, sizeof(temp_buffer), "%.1f", received_temp);
          temp_str = temp_buffer;
      } else if (received_mode != climate::CLIMATE_MODE_OFF && received_mode != climate::CLIMATE_MODE_FAN_ONLY) {
           temp_str = "N/A"; // Or ""
      }

      bool published = false;
      if (this->mode_sensor_ != nullptr && this->mode_sensor_->get_raw_state() != mode_str) {
          this->mode_sensor_->publish_state(mode_str);
          published = true;
      }
      if (this->fan_mode_sensor_ != nullptr && this->fan_mode_sensor_->get_raw_state() != fan_mode_str) {
          this->fan_mode_sensor_->publish_state(fan_mode_str);
          published = true;
      }
      if (this->target_temperature_sensor_ != nullptr && this->target_temperature_sensor_->get_raw_state() != temp_str) {
          this->target_temperature_sensor_->publish_state(temp_str);
          published = true;
      }
      if (this->swing_mode_sensor_ != nullptr && this->swing_mode_sensor_->get_raw_state() != swing_mode_str) {
          this->swing_mode_sensor_->publish_state(swing_mode_str);
          published = true;
      }
      if (published) {
          ESP_LOGD(TAG, "Published updated state: Mode=%s, Fan=%s, Temp=%s, Swing=%s",
                   mode_str.c_str(), fan_mode_str.c_str(), temp_str.c_str(), swing_mode_str.c_str());
      } else {
          ESP_LOGD(TAG, "Received state matches current sensor state, not publishing.");
      }
  }
  return true; // We identified the code
}

} // namespace carrier_cartridge_rx
} // namespace esphome