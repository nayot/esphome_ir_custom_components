
#include "saijo_ac.h"
#include <cmath>
#include <vector>

namespace esphome {
namespace saijo_ac {

static const char *const TAG = "saijo_ac.climate";

// ======================================================================
// ===               IR PROTOCOL TIMING DEFINITIONS                   ===
// ======================================================================
static const uint32_t IR_FREQUENCY      = 38000;  // 38 kHz

// Header
static const int32_t HEADER_PULSE_US = 9000;
static const int32_t HEADER_SPACE_US = -4500;

// Bit timing
static const int32_t PULSE_DURATION_US = 650;
static const int32_t SPACE_ZERO_US     = -500;
static const int32_t SPACE_ONE_US      = -1600;

// End of frame
static const int32_t FINAL_PULSE_US    = 650;

// Decode tolerances
static const int32_t SPACE_ZERO_MAX_US = 700;
static const int32_t SPACE_ONE_MIN_US  = 1300;

// ======================================================================
// ===                     ENCODER / DECODER                           ===
// ======================================================================

// Decode the received NEC-like timings to a 64-bit value (MSB first).
static std::optional<uint64_t> decode_to_uint64(remote_base::RemoteReceiveData data) {
  // Very rough shape check: header + 64 bits + trailing pulse ~ 2 + 64*2 + 1 = 131 entries
  if (data.size() < 131) return std::nullopt;

  uint64_t value = 0;
  int bit_count = 0;

  // Skip header pair (0,1), then for each bit: PULSE (even idx), SPACE (odd idx)
  for (size_t i = 2; i + 1 < data.size() && bit_count < 64; i += 2) {
    const int32_t space = std::abs(data[i + 1]);
    value <<= 1;
    if (space >= SPACE_ONE_MIN_US) {
      value |= 1ULL;
    } else if (space <= SPACE_ZERO_MAX_US) {
      // 0 bit, nothing to set
    } else {
      // ambiguous
      return std::nullopt;
    }
    bit_count++;
  }

  if (bit_count != 64) return std::nullopt;
  return value;
}

// Encode 64-bit payload to timing vector with the above constants.
static std::vector<int32_t> encode_hex_to_raw(uint64_t hex_data) {
  std::vector<int32_t> raw;
  raw.reserve(131);

  raw.push_back(HEADER_PULSE_US);
  raw.push_back(HEADER_SPACE_US);

  for (int i = 63; i >= 0; i--) {
    raw.push_back(PULSE_DURATION_US);
    raw.push_back(((hex_data >> i) & 1ULL) ? SPACE_ONE_US : SPACE_ZERO_US);
  }

  raw.push_back(FINAL_PULSE_US);
  return raw;
}

// Saijo mappings
static inline uint8_t temp_to_b2_code_(int temp_c) {
  // Clamp to your observed range 22..27 to avoid odd codes
  if (temp_c < 22) temp_c = 22;
  if (temp_c > 27) temp_c = 27;
  return static_cast<uint8_t>(0x9E + 2 * (temp_c - 15));
}

static inline uint8_t fan_to_code_(climate::ClimateFanMode fan) {
  switch (fan) {
    case climate::CLIMATE_FAN_LOW:    return 0x2;
    case climate::CLIMATE_FAN_MEDIUM: return 0x6;
    case climate::CLIMATE_FAN_HIGH:   return 0x8;
    default:                          return 0x0;  // AUTO
  }
}

static inline uint8_t swing_to_code_high_(int swing_level) {
  // 0 = swing ON; 1..5 => high nibble 2,4,6,8,A respectively
  if (swing_level <= 0) return 0x0;
  if (swing_level > 5) swing_level = 5;
  return static_cast<uint8_t>((swing_level - 1) * 2 + 0x2);
}

// Build ON frames per reverse-engineered rules.
static uint64_t build_on_frame_(uint8_t seq,
                                climate::ClimateMode mode,
                                climate::ClimateFanMode fan,
                                int temp_c,
                                int swing_level) {
  const uint8_t b0 = 0xA0;
  const uint8_t b1 = 0x90;  // ON
  const uint8_t b2 = temp_to_b2_code_(temp_c);

  const uint8_t fan_code  = fan_to_code_(fan);
  const uint8_t mode_code = (mode == climate::CLIMATE_MODE_COOL) ? 0x2 : 0x1;
  const uint8_t b3 = static_cast<uint8_t>((fan_code << 4) | mode_code);

  // B5: high nibble = swing code; low nibble subflag A for COOL, 9 for DRY/FAN
  const uint8_t swing_high = swing_to_code_high_(swing_level);
  const uint8_t subflag    = (mode == climate::CLIMATE_MODE_COOL) ? 0xA : 0x9;
  const uint8_t b5 = static_cast<uint8_t>((swing_high << 4) | subflag);

  // B4/B6 pair with ±0x40 placement for DRY/FAN; COOL mirrors.
  const uint8_t base = static_cast<uint8_t>(seq & 0x0F);
  uint8_t b4 = base;
  uint8_t b6 = base;
  if (mode == climate::CLIMATE_MODE_DRY) {
    b6 = static_cast<uint8_t>(base + 0x40);
  } else if (mode == climate::CLIMATE_MODE_FAN_ONLY) {
    b4 = static_cast<uint8_t>(base + 0x40);
  }
  const uint8_t b7 = 0x00;

  uint64_t frame = 0;
  const uint8_t bytes[8] = {b0, b1, b2, b3, b4, b5, b6, b7};
  for (int i = 0; i < 8; i++) frame = (frame << 8) | bytes[i];
  return frame;
}

// OFF frame observed after DRY (and generally acceptable): A0 00 B2 01 09 09 09 00
static uint64_t build_off_frame_(int temp_c) {
  const uint8_t b0 = 0xA0;
  const uint8_t b1 = 0x00;  // OFF
  const uint8_t b2 = temp_to_b2_code_(temp_c);
  const uint8_t b3 = 0x01;  // fan AUTO (0), mode group=DRY/FAN (1)
  const uint8_t b4 = 0x09;
  const uint8_t b5 = 0x09;
  const uint8_t b6 = 0x09;
  const uint8_t b7 = 0x00;

  uint64_t frame = 0;
  const uint8_t bytes[8] = {b0, b1, b2, b3, b4, b5, b6, b7};
  for (int i = 0; i < 8; i++) frame = (frame << 8) | bytes[i];
  return frame;
}

// ======================================================================
// ===                          TX HELPERS                             ===
// ======================================================================
void SaijoACClimate::transmit_raw_code_(const int32_t *data, size_t len) {
  if (!this->transmitter_ || data == nullptr || len == 0) {
    ESP_LOGE(TAG, "TX error: transmitter/data invalid");
    return;
  }
  std::vector<int32_t> vec(data, data + len);
  auto call = this->transmitter_->transmit();
  auto *raw = call.get_data();
  raw->set_carrier_frequency(IR_FREQUENCY);
  raw->set_data(vec);
  call.perform();
}

void SaijoACClimate::transmit_hex(uint64_t hex_data) {
  auto raw = encode_hex_to_raw(hex_data);
  this->transmit_raw_code_(raw.data(), raw.size());
}

// ======================================================================
// ===                     ESPHOME VIRTUALS                            ===
// ======================================================================

SaijoACClimate::~SaijoACClimate() = default;

climate::ClimateTraits SaijoACClimate::traits() {
  climate::ClimateTraits t;
  t.set_supports_current_temperature(false);

  t.set_supported_modes({
      climate::CLIMATE_MODE_OFF,
      climate::CLIMATE_MODE_COOL,
      climate::CLIMATE_MODE_FAN_ONLY,
      climate::CLIMATE_MODE_DRY,
  });

  t.set_supported_fan_modes({
      climate::CLIMATE_FAN_AUTO,
      climate::CLIMATE_FAN_LOW,
      climate::CLIMATE_FAN_MEDIUM,
      climate::CLIMATE_FAN_HIGH,
  });

  // Updated visual range and step
  t.set_visual_min_temperature(15.0f);
  t.set_visual_max_temperature(30.0f);
  t.set_visual_temperature_step(0.5f);

  return t;
}

void SaijoACClimate::setup() {
  this->mode = climate::CLIMATE_MODE_OFF;
  this->target_temperature = 25.0f;
  this->fan_mode = climate::CLIMATE_FAN_AUTO;
  this->seq_ = 0x05;
  this->swing_level_ = 0;  // swing ON by default
}

void SaijoACClimate::dump_config() {
  ESP_LOGCONFIG(TAG, "Saijo Denki A/C (custom component)");
  LOG_CLIMATE("  ", "SaijoACClimate", this);
  ESP_LOGCONFIG(TAG, "  Seq start: 0x%X, Swing level: %d", this->seq_, this->swing_level_);
}

void SaijoACClimate::control(const climate::ClimateCall &call) {
  if (call.get_mode().has_value()) this->mode = *call.get_mode();
  if (call.get_target_temperature().has_value()) this->target_temperature = *call.get_target_temperature();
  if (call.get_fan_mode().has_value()) this->fan_mode = *call.get_fan_mode();

  // Build and send frame
  uint64_t frame = 0;
  if (this->mode == climate::CLIMATE_MODE_OFF) {
    frame = build_off_frame_(static_cast<int>(this->target_temperature));
  } else {
    frame = build_on_frame_(this->seq_, this->mode, *this->fan_mode,
                            static_cast<int>(this->target_temperature),
                            this->swing_level_);
    this->seq_ = static_cast<uint8_t>((this->seq_ + 1) & 0x0F);  // advance sequence
  }

  ESP_LOGI(TAG, "TX frame: 0x%016llX", frame);
  this->transmit_hex(frame);
  this->publish_state();
}

// Decode received frame back into HA state
bool SaijoACClimate::on_receive(remote_base::RemoteReceiveData data) {
  auto decoded = decode_to_uint64(data);
  if (!decoded.has_value()) return false;

  const uint64_t code = *decoded;
  const uint8_t b0 = (code >> 56) & 0xFF;
  const uint8_t b1 = (code >> 48) & 0xFF;
  const uint8_t b2 = (code >> 40) & 0xFF;
  const uint8_t b3 = (code >> 32) & 0xFF;
  const uint8_t b4 = (code >> 24) & 0xFF;
  const uint8_t b5 = (code >> 16) & 0xFF;
  const uint8_t b6 = (code >> 8)  & 0xFF;
  const uint8_t b7 = (code >> 0)  & 0xFF;

  ESP_LOGI(TAG, "RX bytes: %02X %02X %02X %02X %02X %02X %02X %02X",
           b0,b1,b2,b3,b4,b5,b6,b7);

  // OFF detection (observed OFF: A0 00 B2 01 09 09 09 00, but accept any with b1==00)
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

  // --- Robust mode resolution: ONLY use +0x40 placement on B4/B6 ---
  const bool b4_plus40 = (b4 & 0x40) != 0;
  const bool b6_plus40 = (b6 & 0x40) != 0;

  if ( b4_plus40 && !b6_plus40)      this->mode = climate::CLIMATE_MODE_FAN_ONLY;
  else if (!b4_plus40 &&  b6_plus40) this->mode = climate::CLIMATE_MODE_DRY;
  else                                this->mode = climate::CLIMATE_MODE_COOL;  // neither set

  // Fan speed: if b3 is unreliable on your handset, default to AUTO
  const uint8_t fan_code = (b3 >> 4) & 0x0F;
  switch (fan_code) {
    case 0x2: this->fan_mode = climate::CLIMATE_FAN_LOW;    break;
    case 0x6: this->fan_mode = climate::CLIMATE_FAN_MEDIUM; break;
    case 0x8: this->fan_mode = climate::CLIMATE_FAN_HIGH;   break;
    default:  this->fan_mode = climate::CLIMATE_FAN_AUTO;   break;
  }

  // Swing decode (vertical) from B5 high nibble
  const uint8_t swing_high = (b5 >> 4) & 0x0F;
  if (swing_high == 0x0) {
    this->swing_level_ = 0;  // swing on
  } else if (swing_high >= 0x2 && swing_high <= 0xA && (swing_high % 2 == 0)) {
    this->swing_level_ = static_cast<int>((swing_high - 0x2) / 2 + 1);
  }

  ESP_LOGI(TAG, "RX ON: mode=%d, temp=%.1f, fan=%d, swing=%d (b4=0x%02X, b6=0x%02X, b3=0x%02X, b5=0x%02X)",
           this->mode, this->target_temperature, this->fan_mode, this->swing_level_,
           b4, b6, b3, b5);

  this->publish_state();
  return true;
}

}  // namespace saijo_ac
}  // namespace esphome