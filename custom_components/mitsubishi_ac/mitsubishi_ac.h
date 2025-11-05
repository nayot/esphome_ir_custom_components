#pragma once

#include "esphome/core/component.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/remote_transmitter/remote_transmitter.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/remote_receiver/remote_receiver.h"
#include "esphome/core/log.h"

#include <array>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <cstdint>
#include <optional>

namespace esphome {
namespace mitsubishi_ac {

class MitsubishiACClimate : public climate::Climate,
                       public Component,
                       public remote_base::RemoteReceiverListener {
 public:
  void set_transmitter(remote_transmitter::RemoteTransmitterComponent *transmitter) {
    this->transmitter_ = transmitter;
  }
  void set_sensor(sensor::Sensor *sensor) { this->sensor_ = sensor; }

  climate::ClimateTraits traits() override;
  void control(const climate::ClimateCall &call) override;
  void setup() override;
  void dump_config() override;
  bool on_receive(remote_base::RemoteReceiveData data) override;

 protected:
  // --- 9-byte helpers ---
  void transmit_hex_9b(const std::array<uint8_t, 9> &bytes);
  void transmit_raw_code_(const int32_t *data, size_t len);

  // --- Members ---
  remote_transmitter::RemoteTransmitterComponent *transmitter_{nullptr};
  sensor::Sensor *sensor_{nullptr};
  int swing_level_{0};
};

}  // namespace mitsubishi_ac
}  // namespace esphome