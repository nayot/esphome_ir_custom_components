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
  // ===== Lifecycle =====
  void setup() override;
  void dump_config() override;
  climate::ClimateTraits traits() override;
  void control(const climate::ClimateCall &call) override;
  bool on_receive(remote_base::RemoteReceiveData data) override;

  // ===== Injected Components =====
  void set_transmitter(remote_transmitter::RemoteTransmitterComponent *transmitter) {
    this->transmitter_ = transmitter;
  }
  void set_sensor(sensor::Sensor *sensor) { this->sensor_ = sensor; }

 protected:
  // ===== Helpers =====
  void transmit_hex_variable(const uint8_t *data, size_t len);
  void transmit_raw_code_(const int32_t *data, size_t len);

  // ===== Internal State =====
  remote_transmitter::RemoteTransmitterComponent *transmitter_{nullptr};
  sensor::Sensor *sensor_{nullptr};

  // Future extension fields
  int swing_level_{0};   // vertical vane level (0–5)
};

}  // namespace mitsubishi_ac
}  // namespace esphome