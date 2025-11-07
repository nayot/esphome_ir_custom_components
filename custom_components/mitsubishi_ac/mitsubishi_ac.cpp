
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
// ===                14-BYTE CODEBOOK PLACEHOLDER                    ===
// ======================================================================
static const std::array<uint8_t, 14> CODE_OFF = {
    0xC4, 0xD3, 0x64, 0x80, 0x00, 0x05,
    0xC0, 0x60, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x7D};

static const std::array<uint8_t, 14> CODE_COOL_22_AUTO = {
    0xC4, 0xD3, 0x64, 0x80, 0x00, 0x25,
    0xC0, 0x60, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x43};

// ======================================================================
// ===                  RAW ENCODE / DECODE HELPERS                   ===
// ======================================================================
static std::optional<std::pair<std::array<uint8_t, 14>, int>>
decode_to_bytes(remote_base::RemoteReceiveData data) {
  if (data.size() < 32) return std::nullopt;

  std::array<uint8_t, 14> bytes{};
  int bit_index = 0;
  const int MAX_BITS = 14 * 8;

  // Skip header
  for (size_t i = 2; i + 1 < data.size() && bit_index < MAX_BITS; i += 2) {
    int32_t space_duration = std::abs(data[i + 1]);
    bool bit = (space_duration >= SPACE_ONE_MIN_US);
    size_t byte_index = bit_index / 8;
    bytes[byte_index] <<= 1;
    bytes[byte_index] |= bit ? 1 : 0;
    bit_index++;
  }

  if (bit_index == 0) return std::nullopt;
  int rem = bit_index % 8;
  if (rem != 0) bytes[bit_index / 8] <<= (8 - rem);

  int used_bytes = (bit_index + 7) / 8;
  ESP_LOGD(TAG, "Decoded %d bits (~%d bytes)", bit_index, used_bytes);
  return std::make_pair(bytes, used_bytes);
}

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

// ======================================================================
// ===                     TRANSMIT FUNCTIONS                         ===
// ======================================================================
void MitsubishiACClimate::transmit_hex_variable(const uint8_t *data, size_t len) {
  if (!this->transmitter_) {
    ESP_LOGE(TAG, "Transmitter not configured!");
    return;
  }
  if (len == 0 || data == nullptr) {
    ESP_LOGW(TAG, "Empty data, skipping transmit");
    return;
  }

  std::ostringstream oss;
  for (size_t i = 0; i < len; i++)
    oss << std::hex << std::uppercase << std::setw(2)
        << std::setfill('0') << (int)data[i] << " ";
  ESP_LOGI(TAG, "TX (%d bytes): %s", (int)len, oss.str().c_str());

  std::vector<int32_t> raw = encode_bytes_to_raw(data, len);
  auto call = this->transmitter_->transmit();
  auto *raw_obj = call.get_data();
  raw_obj->set_carrier_frequency(IR_FREQUENCY);
  raw_obj->set_data(raw);
  call.perform();
}

// ======================================================================
// ===                     CLIMATE FUNCTIONS                          ===
// ======================================================================
void MitsubishiACClimate::setup() {
  this->mode = climate::CLIMATE_MODE_OFF;
  this->target_temperature = 25.0f;
  this->fan_mode = climate::CLIMATE_FAN_AUTO;
}

void MitsubishiACClimate::dump_config() {
  ESP_LOGCONFIG(TAG, "Mitsubishi AC Climate (14-byte code version):");
  LOG_CLIMATE("", "mitsubishi AC", this);
}

climate::ClimateTraits MitsubishiACClimate::traits() {
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

void MitsubishiACClimate::control(const climate::ClimateCall &call) {
  if (call.get_mode().has_value()) this->mode = *call.get_mode();
  if (call.get_target_temperature().has_value()) this->target_temperature = *call.get_target_temperature();
  if (call.get_fan_mode().has_value()) this->fan_mode = *call.get_fan_mode();

  if (this->mode == climate::CLIMATE_MODE_OFF) {
    this->transmit_hex_variable(CODE_OFF.data(), CODE_OFF.size());
  } else if (this->mode == climate::CLIMATE_MODE_COOL) {
    this->transmit_hex_variable(CODE_COOL_22_AUTO.data(), CODE_COOL_22_AUTO.size());
  } else {
    ESP_LOGI(TAG, "Other modes not implemented yet");
  }

  this->publish_state();
}

// ======================================================================
// ===                     RECEIVE FUNCTIONS                          ===
// ======================================================================
bool MitsubishiACClimate::on_receive(remote_base::RemoteReceiveData data) {
  auto decoded_opt = decode_to_bytes(data);
  if (!decoded_opt.has_value()) return false;

  auto [bytes, used_bytes] = decoded_opt.value();

  std::ostringstream oss;
  for (int i = 0; i < used_bytes; i++)
    oss << std::hex << std::uppercase << std::setw(2)
        << std::setfill('0') << (int)bytes[i] << " ";
  ESP_LOGI(TAG, "RX (%d bytes): %s", used_bytes, oss.str().c_str());

  // Example interpretation (you can refine)
  if (used_bytes > 5 && bytes[5] == 0x05) {
    this->mode = climate::CLIMATE_MODE_OFF;
  } else if (used_bytes > 5 && bytes[5] == 0x25) {
    this->mode = climate::CLIMATE_MODE_COOL;
  }

  this->publish_state();
  return true;
}

}  // namespace mitsubishi_ac
}  // namespace esphome