
#include "saijo_ac.h"
#include <cmath>
#include <vector>

namespace esphome {
namespace saijo_ac {

static const char *const TAG = "saijo_ac.climate";

// ======================================================================
// ===                    RAW CODE BOOK SECTION                        ===
// ======================================================================
// Fill these vectors with actual captured values from logs later.
// Each code corresponds to a specific remote command.

static const std::vector<int32_t> RAW_COOL_25C = {
    // Example placeholder data – replace with real capture
    9000, -4500, 650, -500, 650, -1600, 650, -500, 650, -1600,
    650, -500, 650, -1600, 650, -500, 650, -1600, 650, -500,
    650, -1600, 650
};

static const std::vector<int32_t> RAW_DRY = {
    // TODO: Capture with remote and paste here
};

static const std::vector<int32_t> RAW_FAN = {
    // TODO: Capture with remote and paste here
};

static const std::vector<int32_t> RAW_OFF = {
    // TODO: Capture with remote and paste here
};

// ======================================================================
// ===                     TX HELPER FUNCTION                          ===
// ======================================================================

void SaijoACClimate::transmit_raw_code_(const int32_t *data, size_t len) {
  if (!this->transmitter_ || data == nullptr || len == 0) {
    ESP_LOGE(TAG, "TX error: transmitter/data invalid");
    return;
  }
  std::vector<int32_t> vec(data, data + len);
  auto call = this->transmitter_->transmit();
  auto *raw = call.get_data();
  raw->set_carrier_frequency(38000);  // 38 kHz
  raw->set_data(vec);
  call.perform();
  ESP_LOGI(TAG, "RAW code transmitted (%zu entries)", len);
}

// ======================================================================
// ===                    ESPHOME REQUIRED METHODS                     ===
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
  this->swing_level_ = 0;
}

void SaijoACClimate::dump_config() {
  ESP_LOGCONFIG(TAG, "Saijo Denki A/C (RAW control mode)");
  LOG_CLIMATE("  ", "SaijoACClimate", this);
  ESP_LOGCONFIG(TAG, "  Seq start: 0x%X, Swing level: %d", this->seq_, this->swing_level_);
}

// ======================================================================
// ===                    CLIMATE CONTROL HANDLER                      ===
// ======================================================================

void SaijoACClimate::control(const climate::ClimateCall &call) {
  if (call.get_mode().has_value()) this->mode = *call.get_mode();
  if (call.get_target_temperature().has_value()) this->target_temperature = *call.get_target_temperature();
  if (call.get_fan_mode().has_value()) this->fan_mode = *call.get_fan_mode();

  const std::vector<int32_t> *raw_code = nullptr;

  if (this->mode == climate::CLIMATE_MODE_OFF) {
    raw_code = &RAW_OFF;
  } else if (this->mode == climate::CLIMATE_MODE_COOL) {
    raw_code = &RAW_COOL_25C;
  } else if (this->mode == climate::CLIMATE_MODE_DRY) {
    raw_code = &RAW_DRY;
  } else if (this->mode == climate::CLIMATE_MODE_FAN_ONLY) {
    raw_code = &RAW_FAN;
  }

  if (raw_code == nullptr || raw_code->empty()) {
    ESP_LOGW(TAG, "No raw code defined for this mode yet!");
    return;
  }

  ESP_LOGI(TAG, "Sending RAW code for mode=%d, len=%zu", this->mode, raw_code->size());
  this->transmit_raw_code_(raw_code->data(), raw_code->size());
  this->publish_state();
}

// ======================================================================
// ===                 RAW CAPTURE / RECEIVE LOGGING                   ===
// ======================================================================

bool SaijoACClimate::on_receive(remote_base::RemoteReceiveData data) {
  // Log raw timings for manual code capture
  ESP_LOGI(TAG, "=== RAW CAPTURE START (len=%zu) ===", data.size());
  std::string raw_str;
  for (size_t i = 0; i < data.size(); i++) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d%s", data[i], (i < data.size() - 1) ? "," : "");
    raw_str += buf;
    if ((i + 1) % 10 == 0) raw_str += "\n";
  }
  ESP_LOGI(TAG, "%s", raw_str.c_str());
  ESP_LOGI(TAG, "=== RAW CAPTURE END ===");

  // Optional: keep old decoding logic to update HA state automatically
  if (data.size() < 131) return false;

  uint64_t value = 0;
  int bit_count = 0;
  for (size_t i = 2; i + 1 < data.size() && bit_count < 64; i += 2) {
    const int32_t space = std::abs(data[i + 1]);
    value <<= 1;
    if (space > 1300) value |= 1ULL;
    bit_count++;
  }

  if (bit_count != 64) return false;

  const uint64_t code = value;
  const uint8_t b1 = (code >> 48) & 0xFF;
  const uint8_t b2 = (code >> 40) & 0xFF;
  const uint8_t b4 = (code >> 24) & 0xFF;
  const uint8_t b6 = (code >> 8)  & 0xFF;

  if (b1 == 0x00) {
    this->mode = climate::CLIMATE_MODE_OFF;
  } else {
    const bool b4_plus40 = (b4 & 0x40) != 0;
    const bool b6_plus40 = (b6 & 0x40) != 0;
    if (b4_plus40 && !b6_plus40)
      this->mode = climate::CLIMATE_MODE_FAN_ONLY;
    else if (!b4_plus40 && b6_plus40)
      this->mode = climate::CLIMATE_MODE_DRY;
    else
      this->mode = climate::CLIMATE_MODE_COOL;
  }

  this->publish_state();
  return true;
}

}  // namespace saijo_ac
}  // namespace esphome