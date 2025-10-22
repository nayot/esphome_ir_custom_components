#pragma once

#include "esphome/core/component.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/remote_transmitter/remote_transmitter.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/remote_receiver/remote_receiver.h" // Correct include
// includes for decoding function
#include "esphome/core/log.h"
#include <string>       // For std::string
#include <sstream>      // For std::stringstream (hex conversion)
#include <iomanip>      // For std::setw and std::setfill
#include <cstdint>      // For uint64_t

namespace esphome {
namespace carrier_ac {

class CarrierACClimate : public climate::Climate, public Component,
                         public remote_base::RemoteReceiverListener { // Correct inheritance
 public:
  // --- Setter functions ---
  void set_transmitter(remote_transmitter::RemoteTransmitterComponent *transmitter) {
    this->transmitter_ = transmitter;
  }
  void set_sensor(sensor::Sensor *sensor) { this->sensor_ = sensor; }
  // --- REMOVED set_receiver ---

  // --- Overridden functions ---
  climate::ClimateTraits traits() override;
  void control(const climate::ClimateCall &call) override;
  void setup() override;
  void dump_config() override;
  bool on_receive(remote_base::RemoteReceiveData data) override;


 protected:
  // --- Helper Functions ---
  void send_ir_code_();
  void transmit_raw_code_(const int32_t *data, size_t len);
  bool compare_raw_code_(const remote_base::RemoteReceiveData &data, const int32_t *match_code, size_t match_len, int tolerance_percent = 25);


  // --- Member Variables ---
  remote_transmitter::RemoteTransmitterComponent *transmitter_{nullptr};
  sensor::Sensor *sensor_{nullptr};
};

}  // namespace carrier_ac
}  // namespace esphome