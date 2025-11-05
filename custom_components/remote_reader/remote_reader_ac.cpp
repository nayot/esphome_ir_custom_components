
#include "remote_reader_ac.h"
namespace esphome {
namespace remote_reader_ac {

static const char *const TAG = "remote_reader_ac.climate";

// ================================================================
// ===            IR TIMING DEFINITIONS (Mitsubishi)             ===
// ================================================================
const uint32_t IR_FREQUENCY = 38000;
static const int32_t HEADER_PULSE_US = 3400;
static const int32_t HEADER_SPACE_US = -1700;
static const int32_t PULSE_DURATION_US = 450;
static const int32_t SPACE_ZERO_US = -420;
static const int32_t SPACE_ONE_US = -1270;
static const int32_t FINAL_PULSE_US = 450;
static const int32_t SPACE_ONE_MIN_US = 850;

// ================================================================
// ===                ENCODE / DECODE HELPERS                    ===
// ================================================================
static std::optional<std::vector<uint8_t>> decode_to_bytes(remote_base::RemoteReceiveData data) {
  if (data.size() < 32) return std::nullopt;

  std::vector<uint8_t> bytes;
  uint8_t cur = 0;
  int bit_idx = 0;

  // skip header
  for (size_t i = 2; i + 1 < data.size(); i += 2) {
    int32_t space = std::abs(data[i + 1]);
    bool bit = (space >= SPACE_ONE_MIN_US);
    cur = (cur << 1) | (bit ? 1 : 0);
    bit_idx++;
    if (bit_idx % 8 == 0) {
      bytes.push_back(cur);
      cur = 0;
    }
  }

  if (bit_idx % 8 != 0) {
    cur <<= (8 - (bit_idx % 8));
    bytes.push_back(cur);
  }

  return bytes;
}

static std::vector<int32_t> encode_bytes_to_raw(const std::vector<uint8_t> &bytes) {
  std::vector<int32_t> raw;
  raw.reserve(2 + bytes.size() * 16 + 1);
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

// ================================================================
// ===                  TRANSMIT FUNCTION                        ===
// ================================================================
void RemoteReaderACClimate::transmit_hex(const std::vector<uint8_t> &bytes) {
  if (!this->transmitter_) {
    ESP_LOGE(TAG, "No transmitter configured!");
    return;
  }
  std::ostringstream oss;
  for (uint8_t b : bytes)
    oss << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << (int)b << " ";
  ESP_LOGI(TAG, "TX bytes (%d): %s", (int)bytes.size(), oss.str().c_str());

  auto raw = encode_bytes_to_raw(bytes);
  auto call = this->transmitter_->transmit();
  auto *raw_obj = call.get_data();
  raw_obj->set_carrier_frequency(IR_FREQUENCY);
  raw_obj->set_data(raw);
  call.perform();
}

// ================================================================
// ===                 CLIMATE INTERFACE                         ===
// ================================================================
void RemoteReaderACClimate::setup() {
  this->mode = climate::CLIMATE_MODE_OFF;
  this->target_temperature = 25.0f;
  this->fan_mode = climate::CLIMATE_FAN_AUTO;
}

void RemoteReaderACClimate::dump_config() {
  ESP_LOGCONFIG(TAG, "Remote Reader AC Climate:");
  LOG_CLIMATE("", "Remote Reader AC", this);
}

climate::ClimateTraits RemoteReaderACClimate::traits() {
  climate::ClimateTraits t;
  t.set_supports_current_temperature(false);
  t.set_supported_modes({
      climate::CLIMATE_MODE_OFF,
      climate::CLIMATE_MODE_COOL,
      climate::CLIMATE_MODE_DRY,
      climate::CLIMATE_MODE_FAN_ONLY,
  });
  t.set_supported_fan_modes({
      climate::CLIMATE_FAN_AUTO,
      climate::CLIMATE_FAN_LOW,
      climate::CLIMATE_FAN_MEDIUM,
      climate::CLIMATE_FAN_HIGH,
  });
  return t;
}

void RemoteReaderACClimate::control(const climate::ClimateCall &call) {
  ESP_LOGI(TAG, "Control called (stub) — implement your send logic here.");
  this->publish_state();
}

// ================================================================
// ===                  RECEIVE FUNCTION                         ===
// ================================================================
bool RemoteReaderACClimate::on_receive(remote_base::RemoteReceiveData data) {
  auto decoded = decode_to_bytes(data);
  if (!decoded.has_value()) return false;

  const auto &bytes = decoded.value();
  if (bytes.empty()) return false;

  // Print the received bytes as HEX
  std::ostringstream oss;
  for (uint8_t b : bytes)
    oss << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
        << static_cast<int>(b) << " ";
  ESP_LOGI(TAG, "RX bytes (%d): %s", (int)bytes.size(), oss.str().c_str());

  // --- Wait 2 seconds before retransmitting ---
  ESP_LOGI(TAG, "Waiting 2 seconds before retransmit...");
  delay(2000);

  // --- Transmit the same code back out ---
  this->transmit_hex(bytes);

  // --- Optional simple mode preview (for log only) ---
  if (bytes.size() >= 3) {
    const uint8_t b1 = bytes[1];
    const uint8_t b2 = bytes[2];

    if (b1 == 0x00) {
      this->mode = climate::CLIMATE_MODE_OFF;
    } else {
      this->mode = climate::CLIMATE_MODE_COOL;
      this->target_temperature = 15.0f + ((b2 - 0x9E) / 2.0f);
    }
    this->publish_state();
  }

  return true;
}

}  // namespace remote_reader_ac
}  // namespace esphome