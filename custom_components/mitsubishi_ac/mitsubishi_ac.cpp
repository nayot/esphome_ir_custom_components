#include "mitsubishi_ac.h"
#include <cstring>

namespace esphome {
namespace mitsubishi_ac {

static const char *const TAG = "mitsubishi_ac";

// ===============================================================
// IR TIMING
// ===============================================================
const uint32_t IR_FREQUENCY = 38000;
static const int32_t HEADER_PULSE_US = 3400;
static const int32_t HEADER_SPACE_US = -1700;
static const int32_t PULSE_DURATION_US = 450;
static const int32_t SPACE_ZERO_US = -420;
static const int32_t SPACE_ONE_US = -1270;
static const int32_t FINAL_PULSE_US = 450;
static const int32_t SPACE_ONE_MIN_US = 850;

// ===============================================================
// CODEBOOK  (14-byte frames)
// ===============================================================
static const uint8_t CODE_COOL_22_AUTO[14] = { 0xC4, 0xD3, 0x64, 0x80, 0x00, 0x25, 0xC0, 0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0xA3 };
static const uint8_t CODE_COOL_23_AUTO[14] = { 0xC4, 0xD3, 0x64, 0x80, 0x00, 0x25, 0xC0, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x23 };
static const uint8_t CODE_COOL_24_AUTO[14] = { 0xC4, 0xD3, 0x64, 0x80, 0x00, 0x25, 0xC0, 0xE0, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC3 };
static const uint8_t CODE_COOL_25_AUTO[14] = { 0xC4, 0xD3, 0x64, 0x80, 0x00, 0x25, 0xC0, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x43 };
static const uint8_t CODE_COOL_26_AUTO[14] = { 0xC4, 0xD3, 0x64, 0x80, 0x00, 0x25, 0xC0, 0xA0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x83 };
static const uint8_t CODE_COOL_27_AUTO[14] = { 0xC4, 0xD3, 0x64, 0x80, 0x00, 0x25, 0xC0, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03 };
static const uint8_t CODE_COOL_22_LOW[14] = { 0xC4, 0xD3, 0x64, 0x80, 0x00, 0x25, 0xC0, 0x90, 0x40, 0x00, 0x00, 0x00, 0x00, 0xE3 };
static const uint8_t CODE_COOL_23_LOW[14] = { 0xC4, 0xD3, 0x64, 0x80, 0x00, 0x25, 0xC0, 0x10, 0x40, 0x00, 0x00, 0x00, 0x00, 0x63 };
static const uint8_t CODE_COOL_24_LOW[14] = { 0xC4, 0xD3, 0x64, 0x80, 0x00, 0x25, 0xC0, 0xE0, 0x40, 0x00, 0x00, 0x00, 0x00, 0xA3 };
static const uint8_t CODE_COOL_25_LOW[14] = { 0xC4, 0xD3, 0x64, 0x80, 0x00, 0x25, 0xC0, 0x60, 0x40, 0x00, 0x00, 0x00, 0x00, 0x23 };
static const uint8_t CODE_COOL_26_LOW[14] = { 0xC4, 0xD3, 0x64, 0x80, 0x00, 0x25, 0xC0, 0xA0, 0x40, 0x00, 0x00, 0x00, 0x00, 0xC3 };
static const uint8_t CODE_COOL_27_LOW[14] = { 0xC4, 0xD3, 0x64, 0x80, 0x00, 0x25, 0xC0, 0x20, 0x40, 0x00, 0x00, 0x00, 0x00, 0x43 };
static const uint8_t CODE_COOL_22_MEDIUM[14] = { 0xC4, 0xD3, 0x64, 0x80, 0x00, 0x25, 0xC0, 0x90, 0xDC, 0x00, 0x00, 0x00, 0x00, 0x00 };
static const uint8_t CODE_COOL_23_MEDIUM[14] = { 0xC4, 0xD3, 0x64, 0x80, 0x00, 0x25, 0xC0, 0x10, 0xDC, 0x00, 0x00, 0x00, 0x00, 0xFF };
static const uint8_t CODE_COOL_24_MEDIUM[14] = { 0xC4, 0xD3, 0x64, 0x80, 0x00, 0x25, 0xC0, 0xE0, 0xDC, 0x00, 0x00, 0x00, 0x00, 0x7F };
static const uint8_t CODE_COOL_25_MEDIUM[14] = { 0xC4, 0xD3, 0x64, 0x80, 0x00, 0x25, 0xC0, 0x60, 0xDC, 0x00, 0x00, 0x00, 0x00, 0xBF };
static const uint8_t CODE_COOL_26_MEDIUM[14] = { 0xC4, 0xD3, 0x64, 0x80, 0x00, 0x25, 0xC0, 0xA0, 0xDC, 0x00, 0x00, 0x00, 0x00, 0x3F };
static const uint8_t CODE_COOL_27_MEDIUM[14] = { 0xC4, 0xD3, 0x64, 0x80, 0x00, 0x25, 0xC0, 0x20, 0xDC, 0xDC, 0x00, 0x00, 0x00, 0xDF };
static const uint8_t CODE_COOL_22_HIGH[14] = { 0xC4, 0xD3, 0x64, 0x80, 0x00, 0x25, 0xC0, 0x90, 0xBC, 0xDC, 0x00, 0x00, 0x00, 0x40 };
static const uint8_t CODE_COOL_23_HIGH[14] = { 0xC4, 0xD3, 0x64, 0x80, 0x00, 0x25, 0xC0, 0x10, 0xBC, 0xDC, 0x00, 0x00, 0x00, 0x80 };
static const uint8_t CODE_COOL_24_HIGH[14] = { 0xC4, 0xD3, 0x64, 0x80, 0x00, 0x25, 0xC0, 0xE0, 0xBC, 0xDC, 0x00, 0x00, 0x00, 0x00 };
static const uint8_t CODE_COOL_25_HIGH[14] = { 0xC4, 0xD3, 0x64, 0x80, 0x00, 0x25, 0xC0, 0x60, 0xBC, 0xDC, 0x00, 0x00, 0x00, 0xFF };
static const uint8_t CODE_COOL_26_HIGH[14] = { 0xC4, 0xD3, 0x64, 0x80, 0x00, 0x25, 0xC0, 0xA0, 0xBC, 0xDC, 0x00, 0x00, 0x00, 0x7F };
static const uint8_t CODE_COOL_27_HIGH[14] = { 0xC4, 0xD3, 0x64, 0x80, 0x00, 0x25, 0xC0, 0x20, 0xBC, 0x00, 0x00, 0x00, 0x00, 0xBF };
static const uint8_t CODE_FAN_ONLY_AUTO[14] = { 0xC4, 0xD3, 0x64, 0x80, 0x00, 0x25, 0xE0, 0xE0, 0x1C, 0x00, 0x00, 0x00, 0x00, 0xFF };
static const uint8_t CODE_FAN_ONLY_LOW[14] = { 0xC4, 0xD3, 0x64, 0x80, 0x00, 0x25, 0xE0, 0xE0, 0x5C, 0x00, 0x00, 0x00, 0x00, 0x80 };
static const uint8_t CODE_FAN_ONLY_MEDIUM[14] = { 0xC4, 0xD3, 0x64, 0x80, 0x00, 0x25, 0xE0, 0xE0, 0xDC, 0x00, 0x00, 0x00, 0x00, 0x40 };
static const uint8_t CODE_FAN_ONLY_HIGH[14] = { 0xC4, 0xD3, 0x64, 0x80, 0x00, 0x25, 0xE0, 0xE0, 0xBC, 0x00, 0x00, 0x00, 0x00, 0x20 };
static const uint8_t CODE_OFF[14] = { 0xC4, 0xD3, 0x64, 0x80, 0x00, 0x05, 0xE0, 0xE0, 0xBC, 0x00, 0x00, 0x00, 0x00, 0x00 };
static const uint8_t CODE_DRY_AUTO[14] = { 0xC4, 0xD3, 0x64, 0x80, 0x00, 0x25, 0x40, 0xE0, 0x1C, 0x00, 0x00, 0x00, 0x00, 0x5F };
static const uint8_t CODE_DRY_LOW[14] = { 0xC4, 0xD3, 0x64, 0x80, 0x00, 0x25, 0x40, 0xE0, 0x5C, 0x00, 0x00, 0x00, 0x00, 0x3F };
static const uint8_t CODE_DRY_MEDIUM[14] = { 0xC4, 0xD3, 0x64, 0x80, 0x00, 0x25, 0x40, 0xE0, 0xDC, 0x00, 0x00, 0x00, 0x00, 0xBF };
static const uint8_t CODE_DRY_HIGH[14] = { 0xC4, 0xD3, 0x64, 0x80, 0x00, 0x25, 0x40, 0xE0, 0xBC, 0x00, 0x00, 0x00, 0x00, 0xFF };

// ===============================================================
// RAW ENCODE HELPER
// ===============================================================
static std::vector<int32_t> encode_bytes_to_raw(const uint8_t *data, size_t len) {
  std::vector<int32_t> raw;
  raw.reserve(2 + len * 16 + 1);
  raw.push_back(HEADER_PULSE_US);
  raw.push_back(HEADER_SPACE_US);
  for (size_t i = 0; i < len; i++) {
    uint8_t b = data[i];
    for (int j = 7; j >= 0; j--) {
      raw.push_back(PULSE_DURATION_US);
      raw.push_back(((b >> j) & 1) ? SPACE_ONE_US : SPACE_ZERO_US);
    }
  }
  raw.push_back(FINAL_PULSE_US);
  return raw;
}

// ===============================================================
// TRANSMIT
// ===============================================================
void MitsubishiACClimate::transmit_hex_variable(const uint8_t *data, size_t len) {
  if (!this->transmitter_) return;
  auto call = this->transmitter_->transmit();
  auto *raw = call.get_data();
  raw->set_carrier_frequency(IR_FREQUENCY);
  raw->set_data(encode_bytes_to_raw(data, len));
  call.perform();
}

// ===============================================================
// CLIMATE INTERFACE
// ===============================================================
void MitsubishiACClimate::setup() {
  this->mode = climate::CLIMATE_MODE_OFF;
  this->target_temperature = 25;
  this->fan_mode = climate::CLIMATE_FAN_AUTO;
}

void MitsubishiACClimate::dump_config() {
  ESP_LOGCONFIG(TAG, "Mitsubishi A/C (Codebook TX, 22–27 °C)");
}


climate::ClimateTraits MitsubishiACClimate::traits() {
  climate::ClimateTraits t;
  t.set_supports_current_temperature(false);
  t.set_supported_modes({
    climate::CLIMATE_MODE_OFF,
    climate::CLIMATE_MODE_COOL,
    climate::CLIMATE_MODE_DRY,
    climate::CLIMATE_MODE_FAN_ONLY
  });
  t.set_supported_fan_modes({
    climate::CLIMATE_FAN_AUTO,
    climate::CLIMATE_FAN_LOW,
    climate::CLIMATE_FAN_MEDIUM,
    climate::CLIMATE_FAN_HIGH
  });
  t.set_visual_min_temperature(22);
  t.set_visual_max_temperature(27);
  t.set_visual_temperature_step(1);
  return t;
}


void MitsubishiACClimate::control(const climate::ClimateCall &call) {
  if (call.get_mode().has_value())
    this->mode = *call.get_mode();
  if (call.get_target_temperature().has_value())
    this->target_temperature = *call.get_target_temperature();
  if (call.get_fan_mode().has_value())
    this->fan_mode = *call.get_fan_mode();

  const uint8_t *code = CODE_OFF;  // default
  int t = (int)this->target_temperature;

  // ===============================================================
  // COOL MODE
  // ===============================================================
  if (this->mode == climate::CLIMATE_MODE_COOL) {
    if (this->fan_mode == climate::CLIMATE_FAN_AUTO) {
      switch (t) {
        case 22: code = CODE_COOL_22_AUTO; break;
        case 23: code = CODE_COOL_23_AUTO; break;
        case 24: code = CODE_COOL_24_AUTO; break;
        case 25: code = CODE_COOL_25_AUTO; break;
        case 26: code = CODE_COOL_26_AUTO; break;
        case 27: code = CODE_COOL_27_AUTO; break;
        default: code = CODE_COOL_25_AUTO; break;
      }
    } else if (this->fan_mode == climate::CLIMATE_FAN_LOW) {
      switch (t) {
        case 22: code = CODE_COOL_22_LOW; break;
        case 23: code = CODE_COOL_23_LOW; break;
        case 24: code = CODE_COOL_24_LOW; break;
        case 25: code = CODE_COOL_25_LOW; break;
        case 26: code = CODE_COOL_26_LOW; break;
        case 27: code = CODE_COOL_27_LOW; break;
        default: code = CODE_COOL_25_LOW; break;
      }
    } else if (this->fan_mode == climate::CLIMATE_FAN_MEDIUM) {
      switch (t) {
        case 22: code = CODE_COOL_22_MEDIUM; break;
        case 23: code = CODE_COOL_23_MEDIUM; break;
        case 24: code = CODE_COOL_24_MEDIUM; break;
        case 25: code = CODE_COOL_25_MEDIUM; break;
        case 26: code = CODE_COOL_26_MEDIUM; break;
        case 27: code = CODE_COOL_27_MEDIUM; break;
        default: code = CODE_COOL_25_MEDIUM; break;
      }
    } else if (this->fan_mode == climate::CLIMATE_FAN_HIGH) {
      switch (t) {
        case 22: code = CODE_COOL_22_HIGH; break;
        case 23: code = CODE_COOL_23_HIGH; break;
        case 24: code = CODE_COOL_24_HIGH; break;
        case 25: code = CODE_COOL_25_HIGH; break;
        case 26: code = CODE_COOL_26_HIGH; break;
        case 27: code = CODE_COOL_27_HIGH; break;
        default: code = CODE_COOL_25_HIGH; break;
      }
    } else {
      code = CODE_COOL_25_AUTO;
    }

  // ===============================================================
  // DRY MODE
  // ===============================================================
  } else if (this->mode == climate::CLIMATE_MODE_DRY) {
    if (this->fan_mode == climate::CLIMATE_FAN_LOW)
      code = CODE_DRY_LOW;
    else if (this->fan_mode == climate::CLIMATE_FAN_MEDIUM)
      code = CODE_DRY_MEDIUM;
    else if (this->fan_mode == climate::CLIMATE_FAN_HIGH)
      code = CODE_DRY_HIGH;
    else
      code = CODE_DRY_AUTO;

  // ===============================================================
  // FAN ONLY MODE
  // ===============================================================
  } else if (this->mode == climate::CLIMATE_MODE_FAN_ONLY) {
    if (this->fan_mode == climate::CLIMATE_FAN_LOW)
      code = CODE_FAN_ONLY_LOW;
    else if (this->fan_mode == climate::CLIMATE_FAN_MEDIUM)
      code = CODE_FAN_ONLY_MEDIUM;
    else if (this->fan_mode == climate::CLIMATE_FAN_HIGH)
      code = CODE_FAN_ONLY_HIGH;
    else
      code = CODE_FAN_ONLY_AUTO;

  // ===============================================================
  // OFF
  // ===============================================================
  } else {
    code = CODE_OFF;
  }

  this->transmit_hex_variable(code, 14);
  this->publish_state();
}
// ===============================================================
// RECEIVE (unchanged – still works fine)
// ===============================================================

bool MitsubishiACClimate::on_receive(remote_base::RemoteReceiveData data) {
  if (data.size() < 64) return false;

  uint8_t b[14] = {0};
  int bit_index = 0;
  size_t start = 0;

  // --- find header ---
  for (size_t i = 0; i + 1 < data.size(); i++) {
    if (abs(data[i]) > 3000 && abs(data[i + 1]) > 1500) {
      start = i + 2;
      break;
    }
  }

  // --- decode bits ---
  for (size_t i = start; i + 1 < data.size(); i += 2) {
    int32_t sp = abs(data[i + 1]);
    bool bit = (sp >= SPACE_ONE_MIN_US);
    size_t byte_idx = bit_index / 8;
    if (byte_idx >= 14) break;
    b[byte_idx] = (b[byte_idx] << 1) | (bit ? 1 : 0);
    bit_index++;
  }

  // --- verify header signature ---
  if (!(b[0] == 0xC4 && b[1] == 0xD3))
    return false;

  // --- OFF ---
  if (b[5] == 0x05) {
    this->mode = climate::CLIMATE_MODE_OFF;
    this->publish_state();
    return true;
  }

  // --- Mode detection ---
  if (b[6] == 0xC0)
    this->mode = climate::CLIMATE_MODE_COOL;
  else if (b[6] == 0x40)
    this->mode = climate::CLIMATE_MODE_DRY;
  else if (b[6] == 0xE0)
    this->mode = climate::CLIMATE_MODE_FAN_ONLY;

  // --- Temperature detection (for COOL mode only) ---
  if (this->mode == climate::CLIMATE_MODE_COOL) {
    switch (b[7]) {
      case 0x90: this->target_temperature = 22; break;
      case 0x10: this->target_temperature = 23; break;
      case 0xE0: this->target_temperature = 24; break;
      case 0x60: this->target_temperature = 25; break;
      case 0xA0: this->target_temperature = 26; break;
      case 0x20: this->target_temperature = 27; break;
      default: this->target_temperature = 25; break;
    }
  }

  // --- Fan speed detection ---
  uint8_t fan_byte = b[8];
  if ((fan_byte & 0x40) == 0x40)
    this->fan_mode = climate::CLIMATE_FAN_LOW;
  else if ((fan_byte & 0xDC) == 0xDC)
    this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
  else if ((fan_byte & 0xBC) == 0xBC)
    this->fan_mode = climate::CLIMATE_FAN_HIGH;
  else
    this->fan_mode = climate::CLIMATE_FAN_AUTO;

  this->publish_state();
  return true;
}

}  // namespace mitsubishi_ac
}  // namespace esphome