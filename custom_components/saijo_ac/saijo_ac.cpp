#include "saijo_ac.h"
// Includes are now in the .h file, but <vector> is needed here
#include <vector> 

namespace esphome {
namespace saijo_ac {

static const char *const TAG = "saijo_ac.climate";

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
// ===        NEW 9-BYTE ENCODER / DECODER (147 elements total)       ===
// ======================================================================

/**
 * @brief Decode raw IR timings into 9 bytes (72 bits)
 */
static std::optional<std::vector<uint8_t>> decode_to_bytes(remote_base::RemoteReceiveData data) {
  if (data.size() < 147) return std::nullopt;

  std::vector<uint8_t> bytes(9, 0);
  int bit_index = 0;

  for (size_t i = 2; i < data.size() - 1 && bit_index < 72; i += 2) {
    int32_t space_duration = std::abs(data[i + 1]);
    bool bit = (space_duration >= SPACE_ONE_MIN_US);

    size_t byte_index = bit_index / 8;
    bytes[byte_index] <<= 1;
    bytes[byte_index] |= bit ? 1 : 0;

    bit_index++;
  }

  // Final left shift for incomplete byte
  int remaining = 8 - (bit_index % 8);
  if (remaining != 8) bytes[bit_index / 8] <<= remaining;

  if (bit_index == 72) return bytes;
  return std::nullopt;
}

/**
 * @brief Encode 9 bytes (72 bits) into raw IR pulse/space durations.
 */
static std::vector<int32_t> encode_bytes_to_raw(const std::vector<uint8_t> &bytes) {
  std::vector<int32_t> raw;
  raw.reserve(147);

  raw.push_back(HEADER_PULSE_US);
  raw.push_back(HEADER_SPACE_US);

  for (uint8_t b : bytes) {
    for (int i = 7; i >= 0; i--) {
      raw.push_back(PULSE_DURATION_US);
      raw.push_back(((b >> i) & 1) ? SPACE_ONE_US : SPACE_ZERO_US);
    }
  }

  raw.push_back(FINAL_PULSE_US);
  return raw;
}

/**
 * @brief Transmit 9-byte sequence.
 */
void SaijoACClimate::transmit_hex_9b(const std::vector<uint8_t> &bytes) {
  std::vector<int32_t> raw_vector = encode_bytes_to_raw(bytes);
  this->transmit_raw_code_(raw_vector.data(), raw_vector.size());
}

// ======================================================================
// ===                CLIMATE COMPONENT FUNCTIONS                     ===
// ======================================================================

// --- setup() and dump_config() from your original file ---
void SaijoACClimate::setup() {
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

void SaijoACClimate::dump_config() {
  ESP_LOGCONFIG(TAG, "saijo AC Climate:");
  LOG_CLIMATE("", "saijo AC", this);
}


climate::ClimateTraits SaijoACClimate::traits() {
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

void SaijoACClimate::transmit_raw_code_(const int32_t *data, size_t len) {
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

 void SaijoACClimate::transmit_hex(uint64_t hex_data) {
  // Convert 8-byte hex_data to 9-byte vector with 0x00 padding at end
  std::vector<uint8_t> bytes(9, 0);
  for (int i = 0; i < 8; i++) {
    bytes[i] = (hex_data >> ((8 - 1 - i) * 8)) & 0xFF;
  }
  this->transmit_hex_9b(bytes);
}

/**
 * @brief Transmit a command to the AC.
 * This is now updated to call transmit_hex().
 */
void SaijoACClimate::control(const climate::ClimateCall &call) {
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

bool SaijoACClimate::on_receive(remote_base::RemoteReceiveData data) {
  // Decode 9 bytes instead of 8
  auto decoded = decode_to_bytes(data);
  if (!decoded.has_value()) return false;

  const auto &bytes = decoded.value();
  if (bytes.size() < 9) return false;

  // Directly index the 9 bytes
  const uint8_t b0 = bytes[0];
  const uint8_t b1 = bytes[1];
  const uint8_t b2 = bytes[2];
  const uint8_t b3 = bytes[3];
  const uint8_t b4 = bytes[4];
  const uint8_t b5 = bytes[5];
  const uint8_t b6 = bytes[6];
  const uint8_t b7 = bytes[7];
  const uint8_t b8 = bytes[8];  // new 9th byte

  ESP_LOGI(TAG, "RX bytes: %02X %02X %02X %02X %02X %02X %02X %02X %02X",
           b0,b1,b2,b3,b4,b5,b6,b7,b8);

  // --- OFF detection (observed OFF: A0 00 B2 01 09 09 09 00, accept any with b1==00)
  if (b1 == 0x00) {
    this->mode = climate::CLIMATE_MODE_OFF;
    this->target_temperature = 15.0f + ((b2 - 0x9E) / 2.0f);
    this->fan_mode = climate::CLIMATE_FAN_AUTO;
    this->publish_state();
    ESP_LOGI(TAG, "RX OFF: temp=%.1f", this->target_temperature);
    return true;
  }

  if (b1 != 0x90) return false;  // not an ON frame we recognize

  // Common fields
  this->target_temperature = 15.0f + ((b2 - 0x9E) / 2.0f);

  // --- Robust mode resolution ---
  const bool b4_plus40 = (b4 & 0x40) != 0;
  const bool b6_plus40 = (b6 & 0x40) != 0;

  if ( b4_plus40 && !b6_plus40)      this->mode = climate::CLIMATE_MODE_FAN_ONLY;
  else if (!b4_plus40 &&  b6_plus40) this->mode = climate::CLIMATE_MODE_DRY;
  else                                this->mode = climate::CLIMATE_MODE_COOL;

  // --- Fan speed ---
  const uint8_t fan_code = (b3 >> 4) & 0x0F;
  switch (fan_code) {
    case 0x2: this->fan_mode = climate::CLIMATE_FAN_LOW;    break;
    case 0x6: this->fan_mode = climate::CLIMATE_FAN_MEDIUM; break;
    case 0x8: this->fan_mode = climate::CLIMATE_FAN_HIGH;   break;
    default:  this->fan_mode = climate::CLIMATE_FAN_AUTO;   break;
  }

  // --- Swing decode (vertical) from B5 high nibble ---
  const uint8_t swing_high = (b5 >> 4) & 0x0F;
  if (swing_high == 0x0) {
    this->swing_level_ = 0;  // swing on
  } else if (swing_high >= 0x2 && swing_high <= 0xA && (swing_high % 2 == 0)) {
    this->swing_level_ = static_cast<int>((swing_high - 0x2) / 2 + 1);
  }

  ESP_LOGI(TAG,
           "RX ON: mode=%d, temp=%.1f, fan=%d, swing=%d (b4=0x%02X, b6=0x%02X, b3=0x%02X, b5=0x%02X, b8=0x%02X)",
           this->mode, this->target_temperature, this->fan_mode, this->swing_level_,
           b4, b6, b3, b5, b8);

  this->publish_state();
  return true;
}

}  // namespace saijo_ac
}  // namespace esphome