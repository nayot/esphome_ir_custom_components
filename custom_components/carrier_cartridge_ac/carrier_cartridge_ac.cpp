#include "carrier_cartridge_ac.h"
// Includes are now in the .h file, but <vector> is needed here
#include <vector> 

namespace esphome {
namespace carrier_cartridge_ac { 

static const char *const TAG = "carrier_cartridge_ac.climate"; 

// ======================================================================
// ===               IR PROTOCOL TIMING DEFINITIONS                   ===
// ======================================================================
// (Unchanged, as per your request)
const uint32_t IR_FREQUENCY = 38000; // 38kHz
static const int32_t HEADER_PULSE_US = 9000;
static const int32_t HEADER_SPACE_US = -4500;
static const int32_t PULSE_DURATION_US = 650;
static const int32_t SPACE_ZERO_US = -500;
static const int32_t SPACE_ONE_US = -1600;
static const int32_t FINAL_PULSE_US = 650;

// Tolerances for receiving
static const int32_t SPACE_ZERO_MAX_US = 700;
static const int32_t SPACE_ONE_MIN_US = 1300;


// ======================================================================
// ===                PASTE YOUR HEX "CODE BOOK" HERE                 ===
// ======================================================================
// We only need the special codes now.
static const uint64_t CODE_SWING_ON  = 0xF20D01FE210120FCULL;
static const uint64_t CODE_SWING_OFF = 0xF20D01FE210223FCULL;

// This is the static part of our main command (B0-B3)
static const uint64_t CODE_PREFIX = 0xF20D03FC00000000ULL;

// ======================================================================
// ===                NEW ENCODER / DECODER FUNCTIONS                 ===
// ======================================================================

/**
 * @brief Decodes raw IR timings into a 64-bit integer.
 * (Unchanged)
 */
static std::optional<uint64_t> decode_to_uint64(remote_base::RemoteReceiveData data) {
  if (data.size() < 131) {
    return std::nullopt;
  }

  uint64_t decoded_data = 0;
  int bit_count = 0;

  for (size_t i = 2; i < data.size() - 1 && bit_count < 64; i += 2) {
    int32_t space_duration = std::abs(data[i + 1]);
    decoded_data <<= 1;

    if (space_duration >= SPACE_ONE_MIN_US) {
      decoded_data |= 1;
    } else if (space_duration < SPACE_ZERO_MAX_US) {
      // It's a '0' bit.
    } else {
      ESP_LOGW(TAG, "Ambiguous space timing at index %d: %d", (int)i + 1, space_duration);
      return std::nullopt;
    }
    bit_count++;
  }

  if (bit_count == 64) {
    return decoded_data;
  }
  return std::nullopt;
}

/**
 * @brief Encodes a 64-bit hex code into a raw IR timing vector.
 * (Unchanged)
 */
static std::vector<int32_t> encode_hex_to_raw(uint64_t hex_data) {
  std::vector<int32_t> raw_data;
  raw_data.reserve(131);

  raw_data.push_back(HEADER_PULSE_US);
  raw_data.push_back(HEADER_SPACE_US);

  for (int i = 63; i >= 0; i--) {
    raw_data.push_back(PULSE_DURATION_US);
    if ((hex_data >> i) & 1) {
      raw_data.push_back(SPACE_ONE_US);
    } else {
      raw_data.push_back(SPACE_ZERO_US);
    }
  }
  
  raw_data.push_back(FINAL_PULSE_US);
  return raw_data;
}


// ======================================================================
// ===                CLIMATE COMPONENT FUNCTIONS                     ===
// ======================================================================

// (Unchanged from original)
void CarrierACClimate::setup() {
  this->mode = climate::CLIMATE_MODE_OFF;
  this->target_temperature = 25.0f;
  this->fan_mode = climate::CLIMATE_FAN_AUTO;
  this->swing_mode = climate::CLIMATE_SWING_OFF; // Default swing to OFF
  // if (this->receiver_ != nullptr) {
  //   this->receiver_->add_listener(this);
  // }
}

// (Unchanged from original)
void CarrierACClimate::dump_config() {
  ESP_LOGCONFIG(TAG, "Carrier Cartridge AC Climate:"); // Updated log message
  LOG_CLIMATE("", "Carrier Cartridge AC", this); // Updated log message
}

/**
 * @brief MODIFIED traits to support new protocol
 */
climate::ClimateTraits CarrierACClimate::traits() {
  auto traits = climate::ClimateTraits();
  traits.set_supports_current_temperature(false); // Set to true if you add a sensor
  
  traits.set_supported_modes({
      climate::CLIMATE_MODE_OFF,
      climate::CLIMATE_MODE_COOL,
      climate::CLIMATE_MODE_FAN_ONLY,
      climate::CLIMATE_MODE_DRY,
      climate::CLIMATE_MODE_AUTO,
  });
  
  traits.set_supported_fan_modes({
      climate::CLIMATE_FAN_AUTO,
      climate::CLIMATE_FAN_LOW,     // Corresponds to Level 1
      climate::CLIMATE_FAN_MEDIUM,  // Corresponds to Level 3
      climate::CLIMATE_FAN_HIGH,    // Corresponds to Level 5
  });

  traits.set_supported_swing_modes({
      climate::CLIMATE_SWING_OFF,
      climate::CLIMATE_SWING_VERTICAL, // "SWING ON"
  });

  traits.set_visual_min_temperature(17.0f); // From your formula (0 -> 17)
  traits.set_visual_max_temperature(30.0f); // (13 -> 30) 0xD. Max is 32 (0xF)
  traits.set_visual_temperature_step(1.0f);
  return traits;
}


// ======================================================================
// ===                TRANSMITTER FUNCTIONS (MODIFIED)                ===
// ======================================================================

// (Unchanged from original)
void CarrierACClimate::transmit_raw_code_(const int32_t *data, size_t len) {
  if (this->transmitter_ == nullptr) {
    ESP_LOGE(TAG, "Transmitter not set up!");
    return;
  }
  if (len == 0 || data == nullptr) {
      ESP_LOGE(TAG, "Invalid raw code data provided!");
      return;
  }

  std::vector<int32_t> data_vec(data, data + len);
  auto call = this->transmitter_->transmit();
  auto *raw_obj = call.get_data();
  raw_obj->set_carrier_frequency(IR_FREQUENCY);
  raw_obj->set_data(data_vec);
  call.perform();
}

// (Unchanged from original)
void CarrierACClimate::transmit_hex(uint64_t hex_data) {
  std::vector<int32_t> raw_vector = encode_hex_to_raw(hex_data);
  this->transmit_raw_code_(raw_vector.data(), raw_vector.size());
}

/**
 * @brief Transmit a command to the AC.
 * This is COMPLETELY REWRITTEN to be a "command builder"
 * based on your new protocol rules.
 */
void CarrierACClimate::control(const climate::ClimateCall &call) {
  // Update internal state from the call
  if (call.get_mode().has_value()) {
    this->mode = *call.get_mode();
  }
  if (call.get_target_temperature().has_value()) {
    this->target_temperature = *call.get_target_temperature();
  }
  if (call.get_fan_mode().has_value()) {
    this->fan_mode = *call.get_fan_mode();
  }
  if (call.get_swing_mode().has_value()) {
    this->swing_mode = *call.get_swing_mode();
    // Rule 6 & 7: Swing is a special, separate command.
    // We send it and then publish state. We don't build a main command.
    if (this->swing_mode == climate::CLIMATE_SWING_VERTICAL) {
      ESP_LOGD(TAG, "Transmitting SWING ON");
      this->transmit_hex(CODE_SWING_ON);
    } else { // CLIMATE_SWING_OFF
      ESP_LOGD(TAG, "Transmitting SWING OFF");
      this->transmit_hex(CODE_SWING_OFF);
    }
    this->publish_state();
    return; // Don't send a main command
  }

  // --- Build the main command ---
  // Rule 1: Start with the prefix
  uint64_t command = CODE_PREFIX;
  uint8_t b5 = 0x00;
  uint8_t b6 = 0x00;

  // Rule 2 & 7: Set Mode (B6 Low Nibble)
  switch (this->mode) { // <--- FIX: this->mode is NOT optional
    case climate::CLIMATE_MODE_AUTO:
      b6 |= 0x00; // 0
      break;
    case climate::CLIMATE_MODE_COOL:
      b6 |= 0x01; // 1
      break;
    case climate::CLIMATE_MODE_DRY:
      b6 |= 0x02; // 2
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      b6 |= 0x04; // 4
      break;
    case climate::CLIMATE_MODE_OFF:
      b6 |= 0x07; // 7
      break;
    default:
      ESP_LOGW(TAG, "Unsupported mode for transmit: %d", this->mode); // <--- FIX: No asterisk
      return;
  }

  // Rule 3: Set Fan (B6 High Nibble)
  // We map 3 ESPHome levels to 5 AC levels (L1, L3, L5)
  switch (*this->fan_mode) { // <--- FIX: this->fan_mode IS optional
    case climate::CLIMATE_FAN_AUTO:
      b6 |= 0x00; // 0
      break;
    case climate::CLIMATE_FAN_LOW:
      b6 |= 0x40; // 4 (Level 1)
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      b6 |= 0x80; // 8 (Level 3)
      break;
    case climate::CLIMATE_FAN_HIGH:
      b6 |= 0xC0; // C (Level 5)
      break;
    default:
      ESP_LOGW(TAG, "Unsupported fan mode for transmit: %d", *this->fan_mode);
      return;
  }

  // Rule 4: Set Temperature (B5 High Nibble)
  // Only set temp if not in FAN_ONLY or OFF mode
  if (this->mode != climate::CLIMATE_MODE_FAN_ONLY && this->mode != climate::CLIMATE_MODE_OFF) { // <--- FIX: No asterisk
    // Clamp temperature to supported range
    float temp = clamp(this->target_temperature, 17.0f, 32.0f);
    uint8_t temp_val = (uint8_t)round(temp) - 17; // (T - 17)
    b5 |= (temp_val << 4); // Set high nibble
  }

  // Rule 5: B7 is 00 (already handled by CODE_PREFIX)

  // Assemble the command
  command |= (uint64_t)b5 << 16; // B5 is 16 bits from the right
  command |= (uint64_t)b6 << 8;  // B6 is 8 bits from the right

  ESP_LOGD(TAG, "Transmitting command: 0x%016llX", command);
  this->transmit_hex(command);

  // Publish the new state
  this->publish_state();
}
// ======================================================================
// ===                 RECEIVER FUNCTIONS (MODIFIED)                  ===
// ======================================================================

/**
 * @brief Receive and decode an IR command.
 * This is COMPLETELY REWRITTEN to be a "command parser"
 * based on your new protocol rules.
 */
bool CarrierACClimate::on_receive(remote_base::RemoteReceiveData data) {
  // Try to decode the raw data into our 64-bit hex format
  auto decoded_hex = decode_to_uint64(data);

  if (!decoded_hex.has_value()) {
    return false; // Not a valid 64-bit code for this protocol
  }

  uint64_t hex_code = decoded_hex.value();
  ESP_LOGD(TAG, "Received IR code. HEX: 0x%016llX", hex_code);

  // --- Rule 6 & 7: Check for special SWING codes first ---
  if (hex_code == CODE_SWING_ON) {
    ESP_LOGD(TAG, "Matched SWING ON code");
    this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
    this->publish_state();
    return true;
  }
  if (hex_code == CODE_SWING_OFF) {
    ESP_LOGD(TAG, "Matched SWING OFF code");
    this->swing_mode = climate::CLIMATE_SWING_OFF;
    this->publish_state();
    return true;
  }

  // --- Rule 1: Check for main command prefix ---
  if ((hex_code & 0xFFFFFFFF00000000ULL) != CODE_PREFIX) {
    ESP_LOGD(TAG, "Code prefix does not match. Ignoring.");
    return false;
  }

  // --- Parse main command ---
  uint8_t b6 = (hex_code >> 8) & 0xFF;
  uint8_t b5 = (hex_code >> 16) & 0xFF;

  // --- Rule 2: Parse Mode (B6 Low Nibble) ---
  uint8_t mode_nibble = b6 & 0x0F;
  switch (mode_nibble) {
    case 0x0:
      this->mode = climate::CLIMATE_MODE_AUTO;
      break;
    case 0x1:
      this->mode = climate::CLIMATE_MODE_COOL;
      break;
    case 0x2:
      this->mode = climate::CLIMATE_MODE_DRY;
      break;
    case 0x4:
      this->mode = climate::CLIMATE_MODE_FAN_ONLY;
      break;
    case 0x7:
      this->mode = climate::CLIMATE_MODE_OFF;
      break;
    default:
      ESP_LOGW(TAG, "Received unknown mode nibble: 0x%X", mode_nibble);
      return false; // Unrecognized mode
  }

  // --- Rule 3: Parse Fan (B6 High Nibble) ---
  // Map 5 AC levels back to 3 ESPHome levels
  uint8_t fan_nibble = (b6 >> 4) & 0x0F;
  switch (fan_nibble) {
    case 0x0:
      this->fan_mode = climate::CLIMATE_FAN_AUTO;
      break;
    case 0x4: // Level 1
      this->fan_mode = climate::CLIMATE_FAN_LOW;
      break;
    case 0x6: // Level 2
    case 0x8: // Level 3
      this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
      break;
    case 0xA: // Level 4
    case 0xC: // Level 5
      this->fan_mode = climate::CLIMATE_FAN_HIGH;
      break;
    default:
      ESP_LOGW(TAG, "Received unknown fan nibble: 0x%X. Defaulting to AUTO.", fan_nibble);
      this->fan_mode = climate::CLIMATE_FAN_AUTO;
      break;
  }

  // --- Rule 4: Parse Temperature (B5 High Nibble) ---
  // Only apply if not OFF or FAN_ONLY
  if (this->mode != climate::CLIMATE_MODE_OFF && this->mode != climate::CLIMATE_MODE_FAN_ONLY) {
    uint8_t temp_nibble = (b5 >> 4) & 0x0F;
    this->target_temperature = (float)temp_nibble + 17.0f;
    ESP_LOGD(TAG, "Decoded: Mode: %d, Fan: %d, Temp: %.1f", this->mode, this->fan_mode, this->target_temperature);
  } else {
    // FAN_ONLY or OFF mode
    ESP_LOGD(TAG, "Decoded: Mode: %d, Fan: %d", this->mode, this->fan_mode);
  }

  // Publish the new state
  this->publish_state();
  return true;
}


}  // namespace carrier_cartridge_ac
}  // namespace esphome