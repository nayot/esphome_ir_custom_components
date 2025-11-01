
#include "saijo_ac.h"

namespace esphome {
namespace saijo_ac {

static const char *const TAG = "saijo_ac.climate";

// ======================================================================
// ===               IR PROTOCOL TIMING DEFINITIONS                   ===
// ======================================================================
const uint32_t IR_FREQUENCY = 38000;  // 38kHz
static const int32_t HEADER_PULSE_US = 9000;
static const int32_t HEADER_SPACE_US = -4500;
static const int32_t PULSE_DURATION_US = 650;
static const int32_t SPACE_ZERO_US = -500;
static const int32_t SPACE_ONE_US = -1600;
static const int32_t FINAL_PULSE_US = 650;
static const int32_t SPACE_ONE_MIN_US = 1300;

// ======================================================================
// ===                9-BYTE CODEBOOK DEFINITIONS                     ===
// ======================================================================
static const std::array<uint8_t, 9> CODE_COOL_22_AUTO = { 0xA0, 0x90, 0xAC, 0x04, 0x25, 0x0C, 0x25, 0x00, 0xA8 };
static const std::array<uint8_t, 9> CODE_COOL_23_AUTO = { 0xA0, 0x90, 0xAE, 0x04, 0x24, 0x0C, 0x24, 0x00, 0x21 };
static const std::array<uint8_t, 9> CODE_COOL_24_AUTO = { 0xA0, 0x90, 0xB0, 0x04, 0x24, 0x0C, 0x24, 0x00, 0x27 };
static const std::array<uint8_t, 9> CODE_COOL_25_AUTO = { 0xA0, 0x90, 0xB2, 0x04, 0x24, 0x0C, 0x24, 0x00, 0x21 };
static const std::array<uint8_t, 9> CODE_COOL_26_AUTO = { 0xA0, 0x90, 0xB4, 0x04, 0x24, 0x0C, 0x24, 0x00, 0x2F };
static const std::array<uint8_t, 9> CODE_COOL_27_AUTO = { 0xA0, 0x90, 0xB6, 0x04, 0x23, 0x0C, 0x23, 0x00, 0xE2 };
static const std::array<uint8_t, 9> CODE_COOL_22_LOW = { 0xA0, 0x90, 0xAC, 0x24, 0x23, 0x0C, 0x23, 0x00, 0x14 };
static const std::array<uint8_t, 9> CODE_COOL_23_LOW = { 0xA0, 0x90, 0xAE, 0x24, 0x23, 0x0C, 0x23, 0x00, 0x4A };
static const std::array<uint8_t, 9> CODE_COOL_24_LOW = { 0xA0, 0x90, 0xB0, 0x24, 0x22, 0x0C, 0x22, 0x00, 0x0B };
static const std::array<uint8_t, 9> CODE_COOL_25_LOW = { 0xA0, 0x90, 0xB2, 0x24, 0x22, 0x0C, 0x22, 0x00, 0x0D };
static const std::array<uint8_t, 9> CODE_COOL_26_LOW = { 0xA0, 0x90, 0xB4, 0x24, 0x22, 0x0C, 0x22, 0x00, 0x0B };
static const std::array<uint8_t, 9> CODE_COOL_27_LOW = { 0xA0, 0x90, 0xB6, 0x24, 0x22, 0x0C, 0x22, 0x00, 0x0D };
static const std::array<uint8_t, 9> CODE_COOL_22_MEDIUM = { 0xA0, 0x90, 0xAC, 0x64, 0x21, 0x0C, 0x21, 0x00, 0xDD };
static const std::array<uint8_t, 9> CODE_COOL_23_MEDIUM = { 0xA0, 0x90, 0xAE, 0x64, 0x21, 0x0C, 0x21, 0x00, 0xDF };
static const std::array<uint8_t, 9> CODE_COOL_24_MEDIUM = { 0xA0, 0x90, 0xB0, 0x64, 0x21, 0x0C, 0x21, 0x00, 0xDD };
static const std::array<uint8_t, 9> CODE_COOL_25_MEDIUM = { 0xA0, 0x90, 0xB2, 0x64, 0x21, 0x0C, 0x21, 0x00, 0xC7 };
static const std::array<uint8_t, 9> CODE_COOL_26_MEDIUM = { 0xA0, 0x90, 0xB4, 0x64, 0x21, 0x0C, 0x21, 0x00, 0xC5 };
static const std::array<uint8_t, 9> CODE_COOL_27_MEDIUM = { 0xA0, 0x90, 0xB6, 0x64, 0x20, 0x0C, 0x20, 0x00, 0x08 };
static const std::array<uint8_t, 9> CODE_COOL_22_HIGH = { 0xA0, 0x90, 0xAC, 0x84, 0x20, 0x0C, 0x20, 0x00, 0xB2 };
static const std::array<uint8_t, 9> CODE_COOL_23_HIGH = { 0xA0, 0x90, 0xAE, 0x84, 0x20, 0x0C, 0x20, 0x00, 0xF0 };
static const std::array<uint8_t, 9> CODE_COOL_24_HIGH = { 0xA0, 0x90, 0xB0, 0x84, 0x1F, 0x0C, 0x1F, 0x00, 0xF0 };
static const std::array<uint8_t, 9> CODE_COOL_25_HIGH = { 0xA0, 0x90, 0xB2, 0x84, 0x1F, 0x0C, 0x1F, 0x00, 0xB6 };
static const std::array<uint8_t, 9> CODE_COOL_26_HIGH = { 0xA0, 0x90, 0xB4, 0x84, 0x1F, 0x0C, 0x1F, 0x00, 0xB4 };
static const std::array<uint8_t, 9> CODE_COOL_27_HIGH = { 0xA0, 0x90, 0xB6, 0x84, 0x1F, 0x0C, 0x1F, 0x00, 0xEA };
static const std::array<uint8_t, 9> CODE_FAN_ONLY_AUTO = { 0xA0, 0x90, 0xB6, 0x04, 0x5F, 0x0C, 0x1F, 0x00, 0x2B };
static const std::array<uint8_t, 9> CODE_FAN_ONLY_LOW = { 0xA0, 0x90, 0xB6, 0x24, 0x5E, 0x0C, 0x1E, 0x00, 0xC5 };
static const std::array<uint8_t, 9> CODE_FAN_ONLY_MEDIUM = { 0xA0, 0x90, 0xB6, 0x64, 0x5E, 0x0C, 0x1E, 0x00, 0x8D };
static const std::array<uint8_t, 9> CODE_FAN_ONLY_HIGH = { 0xA0, 0x90, 0xB6, 0x84, 0x5E, 0x0C, 0x1E, 0x00, 0x65 };
static const std::array<uint8_t, 9> CODE_OFF = { 0xA0, 0x00, 0xB6, 0x84, 0x1E, 0x0C, 0x1E, 0x00, 0x45 };
static const std::array<uint8_t, 9> CODE_DRY_22_AUTO = { 0xA0, 0x90, 0xAC, 0x04, 0x1D, 0x0C, 0x5D, 0x00, 0x79 };
static const std::array<uint8_t, 9> CODE_DRY_23_AUTO = { 0xA0, 0x90, 0xAE, 0x04, 0x1D, 0x0C, 0x5D, 0x00, 0x3F };
static const std::array<uint8_t, 9> CODE_DRY_24_AUTO = { 0xA0, 0x90, 0xB0, 0x04, 0x1D, 0x0C, 0x5D, 0x00, 0x3D };
static const std::array<uint8_t, 9> CODE_DRY_25_AUTO = { 0xA0, 0x90, 0xB2, 0x04, 0x1D, 0x0C, 0x5D, 0x00, 0x73 };
static const std::array<uint8_t, 9> CODE_DRY_26_AUTO = { 0xA0, 0x90, 0xB4, 0x03, 0x24, 0x0B, 0x64, 0x00, 0xEF };
static const std::array<uint8_t, 9> CODE_DRY_27_AUTO = { 0xA0, 0x90, 0xB6, 0x03, 0x23, 0x0B, 0x63, 0x00, 0xAA };

// ======================================================================
// ===                  RAW ENCODE / DECODE HELPERS                   ===
// ======================================================================
static std::optional<std::array<uint8_t,9>> decode_to_bytes(remote_base::RemoteReceiveData data) {
  if (data.size() < 147) return std::nullopt;
  std::array<uint8_t,9> bytes{};
  int bit_index = 0;

  for (size_t i = 2; i + 1 < data.size() && bit_index < 72; i += 2) {
    int32_t space_duration = std::abs(data[i + 1]);
    bool bit = (space_duration >= SPACE_ONE_MIN_US);
    size_t byte_index = bit_index / 8;
    bytes[byte_index] <<= 1;
    bytes[byte_index] |= bit ? 1 : 0;
    bit_index++;
  }
  int remaining = 8 - (bit_index % 8);
  if (remaining != 8) bytes[bit_index / 8] <<= remaining;
  return bytes;
}

static std::vector<int32_t> encode_bytes_to_raw(const std::array<uint8_t,9> &bytes) {
  std::vector<int32_t> raw;
  raw.reserve(147);
  raw.push_back(HEADER_PULSE_US);
  raw.push_back(HEADER_SPACE_US);
  for (auto b : bytes) {
    for (int i = 7; i >= 0; i--) {
      raw.push_back(PULSE_DURATION_US);
      raw.push_back(((b >> i) & 1) ? SPACE_ONE_US : SPACE_ZERO_US);
    }
  }
  raw.push_back(FINAL_PULSE_US);
  return raw;
}

// ======================================================================
// ===                     TRANSMIT FUNCTIONS                         ===
// ======================================================================
void SaijoACClimate::transmit_hex_9b(const std::array<uint8_t,9> &bytes) {
  if (!this->transmitter_) {
    ESP_LOGE(TAG, "Transmitter not configured!");
    return;
  }
  std::vector<int32_t> raw = encode_bytes_to_raw(bytes);
  auto call = this->transmitter_->transmit();
  auto *raw_obj = call.get_data();
  raw_obj->set_carrier_frequency(IR_FREQUENCY);
  raw_obj->set_data(raw);
  call.perform();
}

// ======================================================================
// ===                     CLIMATE FUNCTIONS                          ===
// ======================================================================
void SaijoACClimate::setup() {
  this->mode = climate::CLIMATE_MODE_OFF;
  this->target_temperature = 25.0f;
  this->fan_mode = climate::CLIMATE_FAN_AUTO;
}

void SaijoACClimate::dump_config() {
  ESP_LOGCONFIG(TAG, "Saijo AC Climate:");
  LOG_CLIMATE("", "Saijo AC", this);
}

climate::ClimateTraits SaijoACClimate::traits() {
  climate::ClimateTraits traits;
  traits.set_supports_current_temperature(false);
  traits.set_supported_modes({
    climate::CLIMATE_MODE_OFF,
    climate::CLIMATE_MODE_COOL,
    climate::CLIMATE_MODE_DRY,
    climate::CLIMATE_MODE_FAN_ONLY,
  });
  traits.set_supported_fan_modes({
    climate::CLIMATE_FAN_AUTO,
    climate::CLIMATE_FAN_LOW,
    climate::CLIMATE_FAN_MEDIUM,
    climate::CLIMATE_FAN_HIGH,
  });
  traits.set_visual_min_temperature(22.0f);
  traits.set_visual_max_temperature(27.0f);
  traits.set_visual_temperature_step(1.0f);
  return traits;
}

void SaijoACClimate::control(const climate::ClimateCall &call) {
  if (call.get_mode().has_value()) this->mode = *call.get_mode();
  if (call.get_target_temperature().has_value()) this->target_temperature = *call.get_target_temperature();
  if (call.get_fan_mode().has_value()) this->fan_mode = *call.get_fan_mode();

  if (this->mode == climate::CLIMATE_MODE_OFF) {
    this->transmit_hex_9b(CODE_OFF);
  }

  // === COOL MODE ===
  else if (this->mode == climate::CLIMATE_MODE_COOL) {
    float t = this->target_temperature;
    if (!this->fan_mode.has_value()) {
      ESP_LOGW(TAG, "Fan mode not set, skipping transmission.");
      return;
    }

    switch (static_cast<int>(*this->fan_mode)) {  // ✅ dereferenced
      case static_cast<int>(climate::CLIMATE_FAN_AUTO):
        if (t == 22) this->transmit_hex_9b(CODE_COOL_22_AUTO);
        else if (t == 23) this->transmit_hex_9b(CODE_COOL_23_AUTO);
        else if (t == 24) this->transmit_hex_9b(CODE_COOL_24_AUTO);
        else if (t == 25) this->transmit_hex_9b(CODE_COOL_25_AUTO);
        else if (t == 26) this->transmit_hex_9b(CODE_COOL_26_AUTO);
        else if (t == 27) this->transmit_hex_9b(CODE_COOL_27_AUTO);
        break;
      default:
        ESP_LOGW(TAG, "Fan speed mode not mapped yet for COOL.");
        break;
    }
  }

  // === DRY MODE ===
  else if (this->mode == climate::CLIMATE_MODE_DRY) {
    float t = this->target_temperature;

    // DRY mode typically uses AUTO fan only
    if (t == 22) this->transmit_hex_9b(CODE_DRY_22_AUTO);
    else if (t == 23) this->transmit_hex_9b(CODE_DRY_23_AUTO);
    else if (t == 24) this->transmit_hex_9b(CODE_DRY_24_AUTO);
    else if (t == 25) this->transmit_hex_9b(CODE_DRY_25_AUTO);
    else if (t == 26) this->transmit_hex_9b(CODE_DRY_26_AUTO);
    else if (t == 27) this->transmit_hex_9b(CODE_DRY_27_AUTO);
    else {
      ESP_LOGW(TAG, "Unsupported temperature %.1f for DRY mode", t);
    }
  }

  // === FAN-ONLY MODE ===
  else if (this->mode == climate::CLIMATE_MODE_FAN_ONLY) {
    if (!this->fan_mode.has_value()) {
      ESP_LOGW(TAG, "Fan mode not set, skipping transmission.");
      return;
    }

    switch (static_cast<int>(*this->fan_mode)) {  // ✅ dereferenced
      case static_cast<int>(climate::CLIMATE_FAN_LOW):
        this->transmit_hex_9b(CODE_FAN_ONLY_LOW);
        break;
      case static_cast<int>(climate::CLIMATE_FAN_MEDIUM):
        this->transmit_hex_9b(CODE_FAN_ONLY_MEDIUM);
        break;
      case static_cast<int>(climate::CLIMATE_FAN_HIGH):
        this->transmit_hex_9b(CODE_FAN_ONLY_HIGH);
        break;
      default:
        this->transmit_hex_9b(CODE_FAN_ONLY_AUTO);
        break;
    }
  }

  else {
    ESP_LOGW(TAG, "No matching 9-byte code found for state.");
  }

  this->publish_state();
}

// ======================================================================
// ===                     RECEIVE FUNCTIONS                          ===
// ======================================================================

bool SaijoACClimate::on_receive(remote_base::RemoteReceiveData data) {
  // Decode 9 bytes instead of 8
  auto decoded = decode_to_bytes(data);
  if (!decoded.has_value()) return false;

  const auto &bytes = decoded.value();
  if (bytes.size() < 9) return false;

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

  // --- OFF detection (observed OFF: A0 00 B2 01 09 09 09 00 ??, accept any with b1==00)
  if (b1 == 0x00) {
    this->mode = climate::CLIMATE_MODE_OFF;
    this->target_temperature = 15.0f + ((b2 - 0x9E) / 2.0f);
    this->fan_mode = climate::CLIMATE_FAN_AUTO;
    this->publish_state();
    ESP_LOGI(TAG, "RX OFF: temp=%.1f", this->target_temperature);
    return true;
  }

  // --- Not ON frame? Ignore ---
  if (b1 != 0x90) return false;

  // --- Common temperature field ---
  this->target_temperature = 15.0f + ((b2 - 0x9E) / 2.0f);

  // --- Robust mode resolution ---
  const bool b4_plus40 = (b4 & 0x40) != 0;
  const bool b6_plus40 = (b6 & 0x40) != 0;

  if ( b4_plus40 && !b6_plus40)      this->mode = climate::CLIMATE_MODE_FAN_ONLY;
  else if (!b4_plus40 &&  b6_plus40) this->mode = climate::CLIMATE_MODE_DRY;
  else                                this->mode = climate::CLIMATE_MODE_COOL;  // default

  // --- Fan speed decode (use B3 upper nibble) ---
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

  // --- Optional: log 9th byte diagnostics ---
  ESP_LOGD(TAG, "Extra B8=0x%02X (reserved)", b8);

  ESP_LOGI(TAG,
           "RX ON: mode=%d, temp=%.1f, fan=%d, swing=%d (b4=0x%02X, b6=0x%02X, b3=0x%02X, b5=0x%02X, b8=0x%02X)",
           this->mode, this->target_temperature, this->fan_mode, this->swing_level_,
           b4, b6, b3, b5, b8);

  this->publish_state();
  return true;
}
}  // namespace saijo_ac
}  // namespace esphome