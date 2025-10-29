
#pragma once

#include "esphome/core/component.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/remote_transmitter/remote_transmitter.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/remote_receiver/remote_receiver.h"
#include "esphome/core/log.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace esphome {
namespace saijo_ac {

class SaijoACClimate : public climate::Climate,
                       public Component,
                       public remote_base::RemoteReceiverListener {
 public:
  // Wiring
  void set_transmitter(remote_transmitter::RemoteTransmitterComponent *transmitter) {
    this->transmitter_ = transmitter;
  }
  void set_sensor(sensor::Sensor *sensor) { this->sensor_ = sensor; }

  // Virtuals required by ESPHome
  climate::ClimateTraits traits() override;
  void control(const climate::ClimateCall &call) override;
  void setup() override;
  void dump_config() override;
  bool on_receive(remote_base::RemoteReceiveData data) override;

  // Ensure vtable is emitted in this TU
  virtual ~SaijoACClimate();

 protected:
  // TX helpers
  void transmit_hex(uint64_t hex_data);
  void transmit_raw_code_(const int32_t *data, size_t len);

  // Internal rolling sequence nibble (0..15), default 0x5 per your captures
  uint8_t seq_{0x05};

  // We keep swing as “auto swing” (0) unless you later expose it to HA UI
  int swing_level_{0};  // 0 = swing ON, 1..5 = fixed levels

  remote_transmitter::RemoteTransmitterComponent *transmitter_{nullptr};
  sensor::Sensor *sensor_{nullptr};
};

}  // namespace saijo_ac
}  // namespace esphome