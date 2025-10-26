#pragma once

#include "esphome/core/component.h"
#include "esphome/components/remote_base/remote_base.h" 
#include "esphome/components/remote_receiver/remote_receiver.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/log.h"
#include <vector>
#include <optional>
#include <cstdint>
#include <string>

namespace esphome {
namespace carrier_cartridge_rx {

class CarrierCartridgeRx : public Component, public remote_base::RemoteReceiverListener {
 public:
  // Setters for text sensors
  void set_mode_sensor(text_sensor::TextSensor *sensor) { this->mode_sensor_ = sensor; }
  void set_fan_mode_sensor(text_sensor::TextSensor *sensor) { this->fan_mode_sensor_ = sensor; }
  void set_target_temperature_sensor(text_sensor::TextSensor *sensor) { this->target_temperature_sensor_ = sensor; }
  void set_swing_mode_sensor(text_sensor::TextSensor *sensor) { this->swing_mode_sensor_ = sensor; }

  // *** Only DECLARE the function here (no function body) ***
  void set_receiver_component(remote_receiver::RemoteReceiverComponent *receiver);
  
  // Overridden functions
  void setup() override;
  void dump_config() override;
  bool on_receive(remote_base::RemoteReceiveData data) override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

 protected:
  // Member Variables
  text_sensor::TextSensor *mode_sensor_{nullptr};
  text_sensor::TextSensor *fan_mode_sensor_{nullptr};
  text_sensor::TextSensor *target_temperature_sensor_{nullptr};
  text_sensor::TextSensor *swing_mode_sensor_{nullptr};
};

} // namespace carrier_cartridge_rx
} // namespace esphome