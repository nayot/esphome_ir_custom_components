#pragma once

#include "esphome/core/component.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/remote_transmitter/remote_transmitter.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/remote_receiver/remote_receiver.h"
// --- New Includes for Hex support ---
#include "esphome/core/log.h"
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cstdint>
#include <optional>
// --- End New Includes ---

namespace esphome {
namespace saijo_ac {

class SaijoACClimate : public climate::Climate, public Component,
                         public remote_base::RemoteReceiverListener {
 public:
  // --- Setter functions (Unchanged) ---
  void set_transmitter(remote_transmitter::RemoteTransmitterComponent *transmitter) {
    this->transmitter_ = transmitter;
  }
  void set_sensor(sensor::Sensor *sensor) { this->sensor_ = sensor; }

  // --- Overridden functions (Unchanged)---
  climate::ClimateTraits traits() override;
  void control(const climate::ClimateCall &call) override;
  void setup() override;
  void dump_config() override;
  bool on_receive(remote_base::RemoteReceiveData data) override;


 protected:
  // --- Helper Functions (MODIFIED) ---
  
  // This is our new helper function
  void transmit_hex(uint64_t hex_data);
  
  // This function is still used by transmit_hex
  void transmit_raw_code_(const int32_t *data, size_t len);
  
  // These are no longer needed
  // void send_ir_code_();
  // bool compare_raw_code_(...);


  // --- Member Variables (Unchanged) ---
  remote_transmitter::RemoteTransmitterComponent *transmitter_{nullptr};
  sensor::Sensor *sensor_{nullptr};
};

}  // namespace Saijo_ac
}  // namespace esphome