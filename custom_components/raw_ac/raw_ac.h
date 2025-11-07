#pragma once

#include "esphome/core/component.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/remote_transmitter/remote_transmitter.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace raw_ac {

class RawACClimate : public climate::Climate, public Component {
 public:
  // --- Setter functions called by Python ---
  void set_transmitter(remote_transmitter::RemoteTransmitterComponent *transmitter) {
    this->transmitter_ = transmitter;
  }
  void set_sensor(sensor::Sensor *sensor) { this->sensor_ = sensor; }

  // --- Functions we MUST override ---
  
  // Defines the capabilities of our component (modes, fans, temps)
  climate::ClimateTraits traits() override;

  // Called by ESPHome whenever the state needs to change
  void control(const climate::ClimateCall &call) override;

  // Standard component functions
  void setup() override;
  void dump_config() override;

 protected:
  // --- Our Helper Functions ---
  
  // Sends the correct IR code based on the current internal state
  void send_ir_code_();

  // Helper to physically send the raw code
  void transmit_raw_code_(const int32_t *data, size_t len);

  // --- Member Variables ---
  remote_transmitter::RemoteTransmitterComponent *transmitter_{nullptr};
  sensor::Sensor *sensor_{nullptr};
};

}  // namespace raw_ac
}  // namespace esphome