#include "carrier_ac.h"
// Includes are now in the .h file, but <vector> is needed here
#include <vector> 

namespace esphome {
namespace carrier_ac {

static const char *const TAG = "carrier_ac.climate";

// ======================================================================
// ===               IR PROTOCOL TIMING DEFINITIONS                   ===
// ======================================================================
// (From your original file's raw codes)
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
// Use 'ULL' (Unsigned Long Long) for 64-bit constants.
// 
// TODO: You MUST define all the hex codes for the commands you are
// using in the 'control' function below.
//
// Example:
static const uint64_t CODE_OFF                 = 0x2049000000090700ULL; 
static const uint64_t CODE_COOL_22_AUTO = 0x2847000000a90700ULL;
static const uint64_t CODE_COOL_23_AUTO = 0x2848000000990700ULL;
static const uint64_t CODE_COOL_24_AUTO = 0x2849000000890700ULL;
static const uint64_t CODE_COOL_25_AUTO = 0x284a000000790700ULL;
static const uint64_t CODE_COOL_26_AUTO = 0x284b000000690700ULL;
static const uint64_t CODE_COOL_27_AUTO = 0x284c000000590700ULL;
static const uint64_t CODE_COOL_22_LOW = 0x2877000000790007ULL;
static const uint64_t CODE_COOL_23_LOW = 0x2878000000690007ULL;
static const uint64_t CODE_COOL_24_LOW = 0x2879000000590007ULL;
static const uint64_t CODE_COOL_25_LOW = 0x287a000000490007ULL;
static const uint64_t CODE_COOL_26_LOW = 0x287b000000390007ULL;
static const uint64_t CODE_COOL_27_LOW = 0x287c000000290007ULL;
static const uint64_t CODE_COOL_22_MEDIUM = 0x2869000000690106ULL;
static const uint64_t CODE_COOL_23_MEDIUM = 0x2868000000790106ULL;
static const uint64_t CODE_COOL_24_MEDIUM = 0x2869000000690106ULL;
static const uint64_t CODE_COOL_25_MEDIUM = 0x286a000000590106ULL;
static const uint64_t CODE_COOL_26_MEDIUM = 0x286b000000490106ULL;
static const uint64_t CODE_COOL_27_MEDIUM = 0x286c000000390106ULL;
static const uint64_t CODE_COOL_22_HIGH = 0x2857000000990205ULL;
static const uint64_t CODE_COOL_23_HIGH = 0x2858000000890205ULL;
static const uint64_t CODE_COOL_24_HIGH = 0x2859000000790205ULL;
static const uint64_t CODE_COOL_25_HIGH = 0x285a000000690205ULL;
static const uint64_t CODE_COOL_26_HIGH = 0x285b000000590205ULL;
static const uint64_t CODE_COOL_27_HIGH = 0x285c000000490205ULL;
static const uint64_t CODE_FAN_ONLY_LOW = 0x2839000000990007ULL;
static const uint64_t CODE_FAN_ONLY_MEDIUM = 0x2829000000a90106ULL;
static const uint64_t CODE_FAN_ONLY_HIGH = 0x2819000000b90205ULL;
static const uint64_t CODE_FAN_ONLY_AUTO= 0x2819000000b90205ULL;

// ======================================================================
// ===                NEW ENCODER / DECODER FUNCTIONS                 ===
// ======================================================================

/**
 * @brief Decodes raw IR timings into a 64-bit integer.
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

// --- setup() and dump_config() from your original file ---
void CarrierACClimate::setup() {
  //// If you use a sensor, restore this logic
  // if (this->sensor_ != nullptr) {
  //   this->sensor_->add_on_state_callback([this](float state) {
  //     this->current_temperature = state;
  //     this->publish_state();
  //   });
  //   this->current_temperature = this->sensor_->state;
  // } else {
  //   this->current_temperature = NAN;
  // }
  // --- Set default state on startup ---
  this->mode = climate::CLIMATE_MODE_OFF;
  this->target_temperature = 25.0f; // Or any other default you prefer
  this->fan_mode = climate::CLIMATE_FAN_AUTO;
  //// If you use a receiver, you must register the listener
  // if (this->receiver_ != nullptr) {
  //   this->receiver_->add_listener(this);
  // }
}

void CarrierACClimate::dump_config() {
  ESP_LOGCONFIG(TAG, "Carrier AC Climate:");
  LOG_CLIMATE("", "Carrier AC", this);
}


climate::ClimateTraits CarrierACClimate::traits() {
  auto traits = climate::ClimateTraits();
  traits.set_supports_current_temperature(false);
  
  traits.set_supported_modes({
      climate::CLIMATE_MODE_OFF,
      climate::CLIMATE_MODE_COOL,
      climate::CLIMATE_MODE_FAN_ONLY,
  });
  
  traits.set_supported_fan_modes({
      climate::CLIMATE_FAN_AUTO,
      climate::CLIMATE_FAN_LOW,
      climate::CLIMATE_FAN_MEDIUM,
      climate::CLIMATE_FAN_HIGH,
  });
  // traits.set_supported_swing_modes({
  //     // climate::CLIMATE_SWING_VERTICAL,
  // });
  traits.set_visual_min_temperature(22.0f);
  traits.set_visual_max_temperature(27.0f);
  traits.set_visual_temperature_step(1.0f);
  return traits;
}


// ======================================================================
// ===                TRANSMITTER FUNCTIONS (MODIFIED)                ===
// ======================================================================

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


/**
 * @brief This is our NEW hex helper function.
 * It converts hex to raw, then calls your working transmit_raw_code_ function.
 */
void CarrierACClimate::transmit_hex(uint64_t hex_data) {
  std::vector<int32_t> raw_vector = encode_hex_to_raw(hex_data);
  // Call your original, working function with the new data
  this->transmit_raw_code_(raw_vector.data(), raw_vector.size());
}

/**
 * @brief Transmit a command to the AC.
 * This is now updated to call transmit_hex().
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

  // Call new helper function
  if (this->mode == climate::CLIMATE_MODE_OFF) {
    this->transmit_hex(CODE_OFF);
  } 
  // --- AUTO FAN ---
  else if (this->mode == climate::CLIMATE_MODE_COOL && this->target_temperature == 22.0f && this->fan_mode == climate::CLIMATE_FAN_AUTO) {
    this->transmit_hex(CODE_COOL_22_AUTO);
  } else if (this->mode == climate::CLIMATE_MODE_COOL && this->target_temperature == 23.0f && this->fan_mode == climate::CLIMATE_FAN_AUTO) {
    this->transmit_hex(CODE_COOL_23_AUTO);
  } else if (this->mode == climate::CLIMATE_MODE_COOL && this->target_temperature == 24.0f && this->fan_mode == climate::CLIMATE_FAN_AUTO) {
    this->transmit_hex(CODE_COOL_24_AUTO);
  } else if (this->mode == climate::CLIMATE_MODE_COOL && this->target_temperature == 25.0f && this->fan_mode == climate::CLIMATE_FAN_AUTO) {
    this->transmit_hex(CODE_COOL_25_AUTO);
  } else if (this->mode == climate::CLIMATE_MODE_COOL && this->target_temperature == 26.0f && this->fan_mode == climate::CLIMATE_FAN_AUTO) {
    this->transmit_hex(CODE_COOL_26_AUTO);
  } else if (this->mode == climate::CLIMATE_MODE_COOL && this->target_temperature == 27.0f && this->fan_mode == climate::CLIMATE_FAN_AUTO) {
    this->transmit_hex(CODE_COOL_27_AUTO);
  }
  // --- LOW FAN ---
  else if (this->mode == climate::CLIMATE_MODE_COOL && this->target_temperature == 22.0f && this->fan_mode == climate::CLIMATE_FAN_LOW) {
    this->transmit_hex(CODE_COOL_22_LOW);
  } else if (this->mode == climate::CLIMATE_MODE_COOL && this->target_temperature == 23.0f && this->fan_mode == climate::CLIMATE_FAN_LOW) {
    this->transmit_hex(CODE_COOL_23_LOW);
  } else if (this->mode == climate::CLIMATE_MODE_COOL && this->target_temperature == 24.0f && this->fan_mode == climate::CLIMATE_FAN_LOW) {
    this->transmit_hex(CODE_COOL_24_LOW);
  } else if (this->mode == climate::CLIMATE_MODE_COOL && this->target_temperature == 25.0f && this->fan_mode == climate::CLIMATE_FAN_LOW) {
    this->transmit_hex(CODE_COOL_25_LOW);
  } else if (this->mode == climate::CLIMATE_MODE_COOL && this->target_temperature == 26.0f && this->fan_mode == climate::CLIMATE_FAN_LOW) {
    this->transmit_hex(CODE_COOL_26_LOW);
  } else if (this->mode == climate::CLIMATE_MODE_COOL && this->target_temperature == 27.0f && this->fan_mode == climate::CLIMATE_FAN_LOW) {
    this->transmit_hex(CODE_COOL_27_LOW);
  }
  // --- MEDIUM FAN ---
  else if (this->mode == climate::CLIMATE_MODE_COOL && this->target_temperature == 22.0f && this->fan_mode == climate::CLIMATE_FAN_MEDIUM) {
    this->transmit_hex(CODE_COOL_22_MEDIUM);
  } else if (this->mode == climate::CLIMATE_MODE_COOL && this->target_temperature == 23.0f && this->fan_mode == climate::CLIMATE_FAN_MEDIUM) {
    this->transmit_hex(CODE_COOL_23_MEDIUM);
  } else if (this->mode == climate::CLIMATE_MODE_COOL && this->target_temperature == 24.0f && this->fan_mode == climate::CLIMATE_FAN_MEDIUM) {
    this->transmit_hex(CODE_COOL_24_MEDIUM);
  } else if (this->mode == climate::CLIMATE_MODE_COOL && this->target_temperature == 25.0f && this->fan_mode == climate::CLIMATE_FAN_MEDIUM) {
    this->transmit_hex(CODE_COOL_25_MEDIUM);
  } else if (this->mode == climate::CLIMATE_MODE_COOL && this->target_temperature == 26.0f && this->fan_mode == climate::CLIMATE_FAN_MEDIUM) {
    this->transmit_hex(CODE_COOL_26_MEDIUM);
  } else if (this->mode == climate::CLIMATE_MODE_COOL && this->target_temperature == 27.0f && this->fan_mode == climate::CLIMATE_FAN_MEDIUM) {
    this->transmit_hex(CODE_COOL_27_MEDIUM);
  }
  // --- HIGH FAN ---
  else if (this->mode == climate::CLIMATE_MODE_COOL && this->target_temperature == 22.0f && this->fan_mode == climate::CLIMATE_FAN_HIGH) {
    this->transmit_hex(CODE_COOL_22_HIGH);
  } else if (this->mode == climate::CLIMATE_MODE_COOL && this->target_temperature == 23.0f && this->fan_mode == climate::CLIMATE_FAN_HIGH) {
    this->transmit_hex(CODE_COOL_23_HIGH);
  } else if (this->mode == climate::CLIMATE_MODE_COOL && this->target_temperature == 24.0f && this->fan_mode == climate::CLIMATE_FAN_HIGH) {
    this->transmit_hex(CODE_COOL_24_HIGH);
  } else if (this->mode == climate::CLIMATE_MODE_COOL && this->target_temperature == 25.0f && this->fan_mode == climate::CLIMATE_FAN_HIGH) {
    this->transmit_hex(CODE_COOL_25_HIGH);
  } else if (this->mode == climate::CLIMATE_MODE_COOL && this->target_temperature == 26.0f && this->fan_mode == climate::CLIMATE_FAN_HIGH) {
    this->transmit_hex(CODE_COOL_26_HIGH);
  } else if (this->mode == climate::CLIMATE_MODE_COOL && this->target_temperature == 27.0f && this->fan_mode == climate::CLIMATE_FAN_HIGH) {
    this->transmit_hex(CODE_COOL_27_HIGH);
  }
  // --- FAN ONLY ---
  else if (this->mode == climate::CLIMATE_MODE_FAN_ONLY && this->fan_mode == climate::CLIMATE_FAN_LOW) {
    this->transmit_hex(CODE_FAN_ONLY_LOW);
  } else if (this->mode == climate::CLIMATE_MODE_FAN_ONLY && this->fan_mode == climate::CLIMATE_FAN_MEDIUM) {
    this->transmit_hex(CODE_FAN_ONLY_MEDIUM);
  } else if (this->mode == climate::CLIMATE_MODE_FAN_ONLY && this->fan_mode == climate::CLIMATE_FAN_HIGH) {
    this->transmit_hex(CODE_FAN_ONLY_HIGH);
  } else if (this->mode == climate::CLIMATE_MODE_FAN_ONLY && this->fan_mode == climate::CLIMATE_FAN_AUTO) {
    this->transmit_hex(CODE_FAN_ONLY_AUTO);
  }
  
  else {
    ESP_LOGW(TAG, "No matching hex code found to transmit for current state.");
  }

  // Publish the new state
  this->publish_state();
}


// ======================================================================
// ===                 RECEIVER FUNCTIONS (MODIFIED)                  ===
// ======================================================================

/**
 * @brief Receive and decode an IR command.
 * This is now updated to decode hex and use the smart logic.
 */
bool CarrierACClimate::on_receive(remote_base::RemoteReceiveData data) {
  // Try to decode the raw data into our 64-bit hex format
  auto decoded_hex = decode_to_uint64(data);

  if (!decoded_hex.has_value()) {
    return false; // Not a valid Carrier 64-bit code
  }

  uint64_t hex_code = decoded_hex.value();
  ESP_LOGD(TAG, "Received IR code. HEX: 0x%016llX", hex_code);

  // Get B0 (Byte 0, MSB) and B1 (Byte 1)
  uint8_t b0 = (hex_code >> 56) & 0xFF;
  uint8_t b1 = (hex_code >> 48) & 0xFF;

  // --- Rule 1: Check Power (B0) ---
  if (b0 == 0x20) {
    ESP_LOGD(TAG, "Matched OFF code (B0=0x20)");
    this->mode = climate::CLIMATE_MODE_OFF;
    this->publish_state();
    return true;
  }

  if (b0 != 0x28) {
    ESP_LOGW(TAG, "Received code, but not ON or OFF (B0=0x%02X)", b0);
    return false; // Not an ON code, ignore
  }

  // --- At this point, we know B0 == 0x28 (Power is ON) ---
  ESP_LOGD(TAG, "Matched ON code (B0=0x28). B1=0x%02X", b1);

  // --- Rule 2: Get Mode & Fan from B1 High Nibble ---
  uint8_t b1_high_nibble = (b1 >> 4) & 0x0F;
  
  switch (b1_high_nibble) {
    case 0x1:
      this->mode = climate::CLIMATE_MODE_FAN_ONLY;
      this->fan_mode = climate::CLIMATE_FAN_HIGH;
      break;
    case 0x2:
      this->mode = climate::CLIMATE_MODE_FAN_ONLY;
      this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
      break;
    case 0x3:
      this->mode = climate::CLIMATE_MODE_FAN_ONLY;
      this->fan_mode = climate::CLIMATE_FAN_LOW;
      break;
    case 0x4:
      this->mode = climate::CLIMATE_MODE_COOL;
      this->fan_mode = climate::CLIMATE_FAN_AUTO;
      break;
    case 0x5:
      this->mode = climate::CLIMATE_MODE_COOL;
      this->fan_mode = climate::CLIMATE_FAN_HIGH;
      break;
    case 0x6:
      this->mode = climate::CLIMATE_MODE_COOL;
      this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
      break;
    case 0x7:
      this->mode = climate::CLIMATE_MODE_COOL;
      this->fan_mode = climate::CLIMATE_FAN_LOW;
      break;
    case 0xB:
      this->mode = climate::CLIMATE_MODE_DRY;
      this->fan_mode = climate::CLIMATE_FAN_AUTO; // DRY mode fan is usually fixed
      break;
    default:
      ESP_LOGW(TAG, "Unknown B1 High Nibble: 0x%X", b1_high_nibble);
      return false; // Unrecognized mode
  }

  // --- Rule 3: Get Temperature from B1 Low Nibble ---
  // This only applies if we are NOT in FAN_ONLY mode
  if (this->mode == climate::CLIMATE_MODE_COOL || this->mode == climate::CLIMATE_MODE_DRY) {
    uint8_t b1_low_nibble = b1 & 0x0F;
    this->target_temperature = 15.0f + b1_low_nibble;
    ESP_LOGD(TAG, "Decoded: Mode: %d, Fan: %d, Temp: %.1f", this->mode, this->fan_mode, this->target_temperature);
  } else {
    // FAN_ONLY mode, temperature is irrelevant
    ESP_LOGD(TAG, "Decoded: Mode: %d, Fan: %d", this->mode, this->fan_mode);
  }

  // Publish the new state
  this->publish_state();
  return true;
}


}  // namespace carrier_ac
}  // namespace esphome