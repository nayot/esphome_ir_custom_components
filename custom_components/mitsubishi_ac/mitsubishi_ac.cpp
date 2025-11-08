#include "mitsubishi_ac.h"
#include <cstring>
#include <cstdio>

namespace esphome {
namespace mitsubishi_ac {

static const char *const TAG = "mitsubishi_ac";

// ===============================================================
// IR PROTOCOL CONSTANTS
// ===============================================================
const uint32_t IR_FREQUENCY = 38000;
static const int32_t HEADER_PULSE_US = 3400;
static const int32_t HEADER_SPACE_US = -1700;
static const int32_t PULSE_DURATION_US = 450;
static const int32_t SPACE_ZERO_US = -420;
static const int32_t SPACE_ONE_US  = -1270;
static const int32_t FINAL_PULSE_US = 450;
static const int32_t SPACE_ONE_MIN_US = 850;

// ===============================================================
// TEMPERATURE TABLE (store in flash)
// ===============================================================
struct TempCode { uint8_t temp; uint8_t code; };
static const TempCode COOL_TEMPS[] PROGMEM = {
  {16,0xF0},{17,0x70},{18,0xB0},{19,0x30},
  {20,0xD0},{21,0x50},{22,0x90},{23,0x10},
  {24,0xE0},{25,0x60},{26,0xA0},{27,0x20},
  {28,0xC0},{29,0x40},{30,0x80},{31,0x00}
};

static uint8_t encode_temp_byte(float t) {
  int temp = (int)(t + 0.5f);
  if (temp < 16) temp = 16;
  if (temp > 31) temp = 31;
  for (uint8_t i = 0; i < 16; i++) {
    if (pgm_read_byte(&COOL_TEMPS[i].temp) == temp)
      return pgm_read_byte(&COOL_TEMPS[i].code);
  }
  return 0xF0;
}

static float decode_temp_byte(uint8_t code) {
  for (uint8_t i = 0; i < 16; i++) {
    if (pgm_read_byte(&COOL_TEMPS[i].code) == code)
      return pgm_read_byte(&COOL_TEMPS[i].temp);
  }
  return 25;
}

// ===============================================================
// FAN / MODE HELPERS
// ===============================================================
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

static uint8_t encode_fan_byte(climate::ClimateMode mode, climate::ClimateFanMode fan) {
  if (mode == climate::CLIMATE_MODE_COOL) {
    if (fan == climate::CLIMATE_FAN_LOW) return 0x40;
    if (fan == climate::CLIMATE_FAN_MEDIUM) return 0xDC;
    if (fan == climate::CLIMATE_FAN_HIGH) return 0xBC;
    return 0x00;
  } else {
    if (fan == climate::CLIMATE_FAN_LOW) return 0x5C;
    if (fan == climate::CLIMATE_FAN_MEDIUM) return 0xDC;
    if (fan == climate::CLIMATE_FAN_HIGH) return 0xBC;
    return 0x1C;
  }
}

static climate::ClimateFanMode decode_fan_byte(uint8_t b) {
  if (b == 0x00 || b == 0x1C) return climate::CLIMATE_FAN_AUTO;
  if (b == 0x40 || b == 0x5C) return climate::CLIMATE_FAN_LOW;
  if (b == 0xDC) return climate::CLIMATE_FAN_MEDIUM;
  if (b == 0xBC) return climate::CLIMATE_FAN_HIGH;
  return climate::CLIMATE_FAN_AUTO;
}

// ===============================================================
// FRAME BUILDERS
// ===============================================================
static void build_frame(uint8_t *f, climate::ClimateMode mode, float t, climate::ClimateFanMode fan) {
  memcpy_P(f, (const uint8_t[]){
    0xC4,0xD3,0x64,0x80,0x00,0x25,0xC0,0x60,0x00,0x00,0x00,0x00,0x00,0x00
  },14);
  f[6] = encode_mode_byte(mode);
  f[7] = (mode == climate::CLIMATE_MODE_COOL) ? encode_temp_byte(t) : 0xE0;
  f[8] = encode_fan_byte(mode, fan);
  uint8_t sum = 0;
  for (int i=0;i<13;i++) sum += f[i];
  f[13]=sum;
}

static void build_off_frame(uint8_t *f) {
  memcpy_P(f,(const uint8_t[]){
    0xC4,0xD3,0x64,0x80,0x00,0x05,0xE0,0xE0,0xBC,0,0,0,0,0
  },14);
  uint8_t sum=0; for(int i=0;i<13;i++) sum+=f[i];
  f[13]=sum;
}

// ===============================================================
// TRANSMIT
// ===============================================================
void MitsubishiACClimate::transmit_hex_variable(const uint8_t *data, size_t len) {
  if (!this->transmitter_) return;
  auto call = this->transmitter_->transmit();
  auto *raw = call.get_data();
  raw->set_carrier_frequency(IR_FREQUENCY);
  // encode raw
  std::vector<int32_t> buf;
  buf.reserve(2 + len*16 + 1);
  buf.push_back(HEADER_PULSE_US);
  buf.push_back(HEADER_SPACE_US);
  for (size_t i=0;i<len;i++) {
    uint8_t b=data[i];
    for (int j=7;j>=0;j--) {
      buf.push_back(PULSE_DURATION_US);
      buf.push_back((b>>j)&1 ? SPACE_ONE_US : SPACE_ZERO_US);
    }
  }
  buf.push_back(FINAL_PULSE_US);
  raw->set_data(buf);
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
  ESP_LOGCONFIG(TAG,"Mitsubishi/SAIJO AC (compact build)");
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

  uint8_t frame[14];
  if (this->mode == climate::CLIMATE_MODE_OFF)
    build_off_frame(frame);
  else
    build_frame(frame, this->mode, this->target_temperature,
                this->fan_mode.has_value() ? *this->fan_mode : climate::CLIMATE_FAN_AUTO);

  this->transmit_hex_variable(frame,14);
  this->publish_state();
}

// ===============================================================
// RECEIVE
// ===============================================================
bool MitsubishiACClimate::on_receive(remote_base::RemoteReceiveData data) {
  if (data.size() < 64) return false;
  uint8_t b[14] = {0};
  int bit_index=0;
  size_t start=0;
  for (size_t i=0;i+1<data.size();i++) {
    if (abs(data[i])>3000 && abs(data[i+1])>1500){ start=i+2; break; }
  }
  for (size_t i=start;i+1<data.size();i+=2) {
    int32_t sp=abs(data[i+1]);
    bool bit=(sp>=SPACE_ONE_MIN_US);
    size_t byte_idx=bit_index/8;
    if (byte_idx>=14) break;
    b[byte_idx]=(b[byte_idx]<<1)|(bit?1:0);
    bit_index++;
  }
  if (!(b[0]==0xC4 && b[1]==0xD3)) return false;

  if (b[5]==0x05){ this->mode=climate::CLIMATE_MODE_OFF; this->publish_state(); return true; }
  this->mode=decode_mode_byte(b[6]);
  if (this->mode==climate::CLIMATE_MODE_COOL) this->target_temperature=decode_temp_byte(b[7]);
  this->fan_mode=decode_fan_byte(b[8]);
  this->publish_state();
  return true;
}

}  // namespace mitsubishi_ac
}  // namespace esphome