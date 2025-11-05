#pragma once
#include "esphome/core/component.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/remote_transmitter/remote_transmitter.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/remote_receiver/remote_receiver.h"
#include "esphome/core/log.h"
#include <vector>
#include <sstream>
#include <iomanip>
#include <optional>
#include <cstdint>

namespace esphome {
namespace remote_reader_ac {

class RemoteReaderACClimate : public climate::Climate,
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

  // transmit any-length vector of bytes
  void transmit_hex(const std::vector<uint8_t> &bytes);

 protected:
  remote_transmitter::RemoteTransmitterComponent *transmitter_{nullptr};
  sensor::Sensor *sensor_{nullptr};
  int swing_level_{0};
};

}  // namespace remote_reader_ac
}  // namespace esphome