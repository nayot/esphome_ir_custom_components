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
static const uint64_t CODE_SWING_ON  = 0xF20D01FE210120FCULL; // Rule 6
static const uint64_t CODE_SWING_OFF = 0xF20D01FE210223FCULL; // Rule 7

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

// *** FIX 1: Use register_listener (based on your reference file) ***
void CarrierCartridgeRx::set_receiver_component(remote_receiver::RemoteReceiverComponent *receiver) {
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

// ======================================================================
// ===                on_receive LOGIC (Protocol v2) ===
// ======================================================================
bool CarrierCartridgeRx::on_receive(remote_base::RemoteReceiveData data) {
  auto decoded_hex = decode_to_uint64(data);
  if (!decoded_hex.has_value()) {
    return false; // Not our code
  }

  uint64_t hex_code = decoded_hex.value();
  ESP_LOGD(TAG, "on_receive: Decoded HEX: 0x%016llX", hex_code);

  // *** FIX 2: Initialize variables to a default state ***
  climate::ClimateMode received_mode = climate::CLIMATE_MODE_OFF;
  climate::ClimateFanMode received_fan_mode = climate::CLIMATE_FAN_AUTO;
  float received_temp = NAN;
  bool state_decoded = false; // Flag to indicate if we decoded a main state
  bool swing_decoded = false; // Flag for special swing codes

  // --- Rule 6 & 7: Check for special SWING codes first ---
  if (hex_code == CODE_SWING_ON) {
    ESP_LOGD(TAG, "on_receive: Matched SWING ON code");
    if (this->swing_mode_sensor_ != nullptr && this->swing_mode_sensor_->get_raw_state() != "VERTICAL") {
       this->swing_mode_sensor_->publish_state("VERTICAL");
    }
    swing_decoded = true;
  } else if (hex_code == CODE_SWING_OFF) {
    ESP_LOGD(TAG, "on_receive: Matched SWING OFF code");
    if (this->swing_mode_sensor_ != nullptr && this->swing_mode_sensor_->get_raw_state() != "OFF") {
       this->swing_mode_sensor_->publish_state("OFF");
    }
    swing_decoded = true;
  }

  // --- Rule 1: Check for main code prefix ---
  uint32_t prefix = (hex_code >> 32) & 0xFFFFFFFFULL;
  if (prefix == 0xF20D03FCULL) {
    ESP_LOGD(TAG, "on_receive: Matched main state prefix 0xF20D03FC");
    state_decoded = true;

    uint8_t b5 = (hex_code >> 16) & 0xFF;
    uint8_t b6 = (hex_code >> 8) & 0xFF;

    // --- Rule 2: Parse Mode (B6 Low Nibble) ---
    uint8_t mode_nibble = b6 & 0x0F;
    switch (mode_nibble) {
        case 0x0: received_mode = climate::CLIMATE_MODE_AUTO; break;
        case 0x1: received_mode = climate::CLIMATE_MODE_COOL; break;
        case 0x2: received_mode = climate::CLIMATE_MODE_DRY; break;
        case 0x4: received_mode = climate::CLIMATE_MODE_FAN_ONLY; break;
        case 0x7: received_mode = climate::CLIMATE_MODE_OFF; break;
        default:
          ESP_LOGW(TAG, "on_receive: Unknown mode nibble: 0x%X", mode_nibble);
          state_decoded = false; 
    }

    // --- Rule 3: Parse Fan (B6 High Nibble) ---
    uint8_t fan_nibble = (b6 >> 4) & 0x0F;
    switch (fan_nibble) {
        case 0x0: received_fan_mode = climate::CLIMATE_FAN_AUTO; break;
        case 0x4: received_fan_mode = climate::CLIMATE_FAN_LOW; break; // Level 1
        case 0x6: received_fan_mode = climate::CLIMATE_FAN_MEDIUM; break; // Level 2
        case 0x8: received_fan_mode = climate::CLIMATE_FAN_MEDIUM; break; // Level 3
        case 0xA: received_fan_mode = climate::CLIMATE_FAN_HIGH; break; // Level 4
        case 0xC: received_fan_mode = climate::CLIMATE_FAN_HIGH; break; // Level 5
        default:
          received_fan_mode = climate::CLIMATE_FAN_AUTO; 
    }

    // --- Rule 4: Parse Temperature (B5 High Nibble) ---
    // This check now safely uses the initialized 'received_mode'
    if (state_decoded && received_mode != climate::CLIMATE_MODE_OFF && received_mode != climate::CLIMATE_MODE_FAN_ONLY) {
      uint8_t temp_nibble = (b5 >> 4) & 0x0F;
      received_temp = temp_nibble + 17.0f; // T = value + 17
      received_temp = clamp(received_temp, 17.0f, 30.0f); 
    }
  }


  // --- Publish state to sensors if a main state was decoded ---
  if (state_decoded) {
      std::string mode_str = rx_climate_mode_to_string(received_mode);
      std::string fan_mode_str = rx_climate_fan_mode_to_string(received_fan_mode);
      std::string temp_str = "";
      
      if (!std::isnan(received_temp)) {
          char temp_buffer[10];
          snprintf(temp_buffer, sizeof(temp_buffer), "%.1f", received_temp);
          // *** FIX 3: Added missing semicolon ***
          temp_str = temp_buffer;
      } else if (received_mode != climate::CLIMATE_MODE_OFF && received_mode != climate::CLIMATE_MODE_FAN_ONLY) {
           temp_str = "N/A"; 
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
      
      if (published) {
          ESP_LOGD(TAG, "Published updated state: Mode=%s, Fan=%s, Temp=%s",
                   mode_str.c_str(), fan_mode_str.c_str(), temp_str.c_str());
      } else {
          ESP_LOGD(TAG, "Received state matches current sensor state, not publishing.");
      }
  }

  // Return true if we identified either a main state OR a swing code
  return state_decoded || swing_decoded; 
}

} // namespace carrier_cartridge_rx
} // namespace esphome