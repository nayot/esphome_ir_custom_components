#include "mitsubishi_ac.h"

namespace esphome {
namespace mitsubishi_ac {

static const char *const TAG = "mitsubishi_ac.climate";

// ======================================================================
// ===               IR PROTOCOL TIMING DEFINITIONS                   ===
// ======================================================================
const uint32_t IR_FREQUENCY = 38000;  // 38 kHz
static const int32_t HEADER_PULSE_US = 3400;
static const int32_t HEADER_SPACE_US = -1700;
static const int32_t PULSE_DURATION_US = 450;
static const int32_t SPACE_ZERO_US = -420;
static const int32_t SPACE_ONE_US = -1270;
static const int32_t FINAL_PULSE_US = 450;
static const int32_t SPACE_ONE_MIN_US = 850;

// ======================================================================
// ===                   TEMPERATURE ENCODE/DECODE                    ===
// ======================================================================
struct TempCode { uint8_t temp; uint8_t code; };

// From your full 16–31 °C capture (COOL, FAN AUTO)
static const TempCode COOL_TEMPS[] = {
    {16, 0xF0}, {17, 0x70}, {18, 0xB0}, {19, 0x30},
    {20, 0xD0}, {21, 0x50}, {22, 0x90}, {23, 0x10},
    {24, 0xE0}, {25, 0x60}, {26, 0xA0}, {27, 0x20},
    {28, 0xC0}, {29, 0x40}, {30, 0x80}, {31, 0x00},
};

static uint8_t encode_temp_byte(float t) {
  int temp = static_cast<int>(t + 0.5f);
  if (temp < 16) temp = 16;
  if (temp > 31) temp = 31;
  for (auto &p : COOL_TEMPS)
    if (p.temp == temp) return p.code;
  return COOL_TEMPS[0].code;
}

static float decode_temp_byte(uint8_t code) {
  for (auto &p : COOL_TEMPS)
    if (p.code == code) return p.temp;

  // Fallback: nearest
  uint8_t best_code = COOL_TEMPS[0].code;
  int best_diff = 999;
  float best_temp = COOL_TEMPS[0].temp;
  for (auto &p : COOL_TEMPS) {
    int d = abs((int)p.code - (int)code);
    if (d < best_diff) {
      best_diff = d;
      best_temp = p.temp;
      best_code = p.code;
    }
  }
  return best_temp;
}

// ======================================================================
// ===                     FAN / MODE HELPERS                         ===
// ======================================================================

// Mode byte (B6)
static uint8_t encode_mode_byte(climate::ClimateMode mode) {
  switch (mode) {
    case climate::CLIMATE_MODE_COOL:     return 0xC0;
    case climate::CLIMATE_MODE_DRY:      return 0x40;
    case climate::CLIMATE_MODE_FAN_ONLY: return 0xE0;
    default:                             return 0xE0;
  }
}

static climate::ClimateMode decode_mode_byte(uint8_t b) {
  if (b == 0xC0) return climate::CLIMATE_MODE_COOL;
  if (b == 0x40) return climate::CLIMATE_MODE_DRY;
  if (b == 0xE0) return climate::CLIMATE_MODE_FAN_ONLY;
  return climate::CLIMATE_MODE_OFF;
}

// Fan byte (B8) – mode-aware encode
static uint8_t encode_fan_byte(climate::ClimateMode mode,
                               climate::ClimateFanMode fan) {
  // COOL (from your COOL table)
  if (mode == climate::CLIMATE_MODE_COOL) {
    switch (fan) {
      case climate::CLIMATE_FAN_LOW:    return 0x40;
      case climate::CLIMATE_FAN_MEDIUM: return 0xDC;
      case climate::CLIMATE_FAN_HIGH:   return 0xBC;
      case climate::CLIMATE_FAN_AUTO:
      default:                          return 0x00;  // AUTO
    }
  }

  // DRY / FAN_ONLY (from DRY/FAN_ONLY captures: 1C / 5C / DC / BC)
  if (mode == climate::CLIMATE_MODE_DRY ||
      mode == climate::CLIMATE_MODE_FAN_ONLY) {
    switch (fan) {
      case climate::CLIMATE_FAN_LOW:    return 0x5C;
      case climate::CLIMATE_FAN_MEDIUM: return 0xDC;
      case climate::CLIMATE_FAN_HIGH:   return 0xBC;
      case climate::CLIMATE_FAN_AUTO:
      default:                          return 0x1C;  // AUTO
    }
  }

  // Fallback
  return 0x00;
}

// Fan decode – understand both COOL and DRY/FAN_ONLY variants
static climate::ClimateFanMode decode_fan_byte(uint8_t b) {
  // AUTO codes
  if (b == 0x00 || b == 0x1C)
    return climate::CLIMATE_FAN_AUTO;

  // LOW can be 0x40 (COOL) or 0x5C (DRY/FAN_ONLY)
  if (b == 0x40 || b == 0x5C)
    return climate::CLIMATE_FAN_LOW;

  if (b == 0xDC)
    return climate::CLIMATE_FAN_MEDIUM;

  if (b == 0xBC)
    return climate::CLIMATE_FAN_HIGH;

  // Unknown → safest is AUTO
  return climate::CLIMATE_FAN_AUTO;
}

// ======================================================================
// ===                     FRAME GENERATION                           ===
// ======================================================================
static std::array<uint8_t, 14> build_frame_(climate::ClimateMode mode,
                                            float temp_c,
                                            climate::ClimateFanMode fan) {
  std::array<uint8_t, 14> f = {
      0xC4, 0xD3, 0x64, 0x80, 0x00,
      0x25,  // ON
      0xC0,  // mode
      0x60,  // temp
      0x00,  // fan
      0x00, 0x00, 0x00, 0x00,
      0x00};

  // Mode (B6)
  f[6] = encode_mode_byte(mode);

  // Temp (B7)
  if (mode == climate::CLIMATE_MODE_COOL)
    f[7] = encode_temp_byte(temp_c);
  else
    f[7] = 0xE0;  // DRY / FAN_ONLY always had E0 in your captures

  // Fan (B8)
  f[8] = encode_fan_byte(mode, fan);

  // Checksum: sum of bytes 0–12
  uint8_t sum = 0;
  for (int i = 0; i < 13; i++) sum += f[i];
  f[13] = sum;

  return f;
}

// OFF frame – from your OFF capture
static std::array<uint8_t, 14> build_off_frame_() {
  std::array<uint8_t, 14> f = {
      0xC4, 0xD3, 0x64, 0x80, 0x00,
      0x05, 0xE0, 0xE0, 0xBC,
      0x00, 0x00, 0x00, 0x00,
      0x00};
  uint8_t sum = 0;
  for (int i = 0; i < 13; i++) sum += f[i];
  f[13] = sum;
  return f;
}

// ======================================================================
// ===                  RAW ENCODE / DECODE HELPERS                   ===
// ======================================================================
static std::optional<std::pair<std::array<uint8_t, 14>, int>>
decode_to_bytes(remote_base::RemoteReceiveData data) {
  std::array<uint8_t, 14> bytes{};
  int bit_index = 0;

  // Find header (approx 3400 / 1700 pair)
  size_t start_index = 0;
  for (size_t i = 0; i + 1 < data.size(); i++) {
    if (std::abs(data[i]) > 3000 && std::abs(data[i + 1]) > 1500) {
      start_index = i + 2;
      break;
    }
  }

  const int THRESHOLD_US = 850;  // between 420 and 1270
  for (size_t i = start_index; i + 1 < data.size(); i += 2) {
    int32_t space = std::abs(data[i + 1]);
    bool bit = (space >= THRESHOLD_US);
    size_t byte_index = bit_index / 8;
    if (byte_index >= bytes.size()) break;
    bytes[byte_index] <<= 1;
    bytes[byte_index] |= bit ? 1 : 0;
    bit_index++;
  }

  if (bit_index < 8) return std::nullopt;
  int used = (bit_index + 7) / 8;
  int rem = bit_index % 8;
  if (rem != 0) bytes[bit_index / 8] <<= (8 - rem);

  ESP_LOGD(TAG, "Decoded %d bits (~%d bytes)", bit_index, used);
  return std::make_pair(bytes, used);
}

static std::vector<int32_t> encode_bytes_to_raw(const uint8_t *d, size_t n) {
  std::vector<int32_t> r;
  r.reserve(2 + n * 16 + 1);
  r.push_back(HEADER_PULSE_US);
  r.push_back(HEADER_SPACE_US);
  for (size_t i = 0; i < n; i++) {
    uint8_t b = d[i];
    for (int j = 7; j >= 0; j--) {
      r.push_back(PULSE_DURATION_US);
      r.push_back((b >> j) & 1 ? SPACE_ONE_US : SPACE_ZERO_US);
    }
  }
  r.push_back(FINAL_PULSE_US);
  return r;
}

// ======================================================================
// ===                     TRANSMIT FUNCTIONS                         ===
// ======================================================================
void MitsubishiACClimate::transmit_hex_variable(const uint8_t *d, size_t n) {
  if (!this->transmitter_) return;

  std::ostringstream oss;
  for (size_t i = 0; i < n; i++)
    oss << std::hex << std::uppercase << std::setw(2)
        << std::setfill('0') << (int)d[i] << " ";
  ESP_LOGI(TAG, "TX %dB: %s", (int)n, oss.str().c_str());

  auto raw = encode_bytes_to_raw(d, n);
  auto call = this->transmitter_->transmit();
  call.get_data()->set_carrier_frequency(IR_FREQUENCY);
  call.get_data()->set_data(raw);
  call.perform();
}

// ======================================================================
// ===                     CLIMATE FUNCTIONS                          ===
// ======================================================================
void MitsubishiACClimate::setup() {
  this->mode = climate::CLIMATE_MODE_OFF;
  this->target_temperature = 25;
  this->fan_mode = climate::CLIMATE_FAN_AUTO;
}

void MitsubishiACClimate::dump_config() {
  ESP_LOGCONFIG(TAG, "Mitsubishi/SAIJO A/C Protocol (16–31 °C, mode-aware fan)");
  LOG_CLIMATE("", "mitsubishi_ac", this);
}

climate::ClimateTraits MitsubishiACClimate::traits() {
  climate::ClimateTraits t;
  t.set_supports_current_temperature(false);
  t.set_supported_modes({
      climate::CLIMATE_MODE_OFF,
      climate::CLIMATE_MODE_COOL,
      climate::CLIMATE_MODE_DRY,
      climate::CLIMATE_MODE_FAN_ONLY});
  t.set_supported_fan_modes({
      climate::CLIMATE_FAN_AUTO,
      climate::CLIMATE_FAN_LOW,
      climate::CLIMATE_FAN_MEDIUM,
      climate::CLIMATE_FAN_HIGH});
  t.set_visual_min_temperature(16);
  t.set_visual_max_temperature(31);
  t.set_visual_temperature_step(1);
  return t;
}

void MitsubishiACClimate::control(const climate::ClimateCall &call) {
  if (call.get_mode().has_value()) this->mode = *call.get_mode();
  if (call.get_target_temperature().has_value())
    this->target_temperature = *call.get_target_temperature();
  if (call.get_fan_mode().has_value())
    this->fan_mode = *call.get_fan_mode();

  if (this->mode == climate::CLIMATE_MODE_OFF) {
    auto off = build_off_frame_();
    this->transmit_hex_variable(off.data(), off.size());
  } else {
    auto frame = build_frame_(this->mode,
                              this->target_temperature,
                              this->fan_mode.has_value()
                                  ? *this->fan_mode
                                  : climate::CLIMATE_FAN_AUTO);
    this->transmit_hex_variable(frame.data(), frame.size());
  }

  this->publish_state();
}

// ======================================================================
// ===                     RECEIVE FUNCTIONS                          ===
// ======================================================================
bool MitsubishiACClimate::on_receive(remote_base::RemoteReceiveData data) {
  auto d = decode_to_bytes(data);
  if (!d.has_value()) return false;
  auto [b, n] = d.value();
  if (n < 14) return false;

  // Filter: only accept frames that match known header
  if (!(b[0] == 0xC4 && b[1] == 0xD3 && b[2] == 0x64 && b[3] == 0x80 && b[4] == 0x00)) {
    ESP_LOGD(TAG, "Ignoring frame (unknown header)");
    return false;
  }

  std::ostringstream oss;
  for (int i = 0; i < n; i++)
    oss << std::hex << std::uppercase << std::setw(2)
        << std::setfill('0') << (int)b[i];
  ESP_LOGI(TAG, "RX %dB: %s", n, oss.str().c_str());

  // OFF vs ON (B5)
  if (b[5] == 0x05) {
    this->mode = climate::CLIMATE_MODE_OFF;
    this->publish_state();
    return true;
  }

  // Mode / Temp / Fan
  this->mode = decode_mode_byte(b[6]);

  if (this->mode == climate::CLIMATE_MODE_COOL)
    this->target_temperature = decode_temp_byte(b[7]);

  this->fan_mode = decode_fan_byte(b[8]);

  ESP_LOGI(TAG, "Decoded → Mode=%d  Temp=%.0f°C  Fan=%d",
           (int)this->mode,
           this->target_temperature,
           this->fan_mode.has_value() ? (int)*this->fan_mode : -1);

  this->publish_state();
  return true;
}

}  // namespace mitsubishi_ac
}  // namespace esphome