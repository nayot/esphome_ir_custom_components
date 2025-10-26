# File: custom_components/carrier_cartridge_rx/text_sensor.py

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor, remote_receiver, remote_base
from esphome.const import CONF_ID

CONF_RECEIVER_ID = "receiver_id"

# Define keys for the configuration options
CONF_MODE_SENSOR = "mode_sensor"
CONF_FAN_MODE_SENSOR = "fan_mode_sensor"
CONF_TARGET_TEMPERATURE_SENSOR = "target_temperature_sensor"
CONF_SWING_MODE_SENSOR = "swing_mode_sensor"

# Namespace
carrier_cartridge_rx_ns = cg.esphome_ns.namespace("carrier_cartridge_rx")

# C++ Class Name
CarrierCartridgeRx = carrier_cartridge_rx_ns.class_(
    "CarrierCartridgeRx", cg.Component, remote_base.RemoteReceiverListener
)

# --- CORRECTED CONFIG_SCHEMA ---
# We wrap the entire schema in cv.All() to add the dependency check.
CONFIG_SCHEMA = cv.All(
    # Start with the original schema
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(CarrierCartridgeRx),
            cv.Required(CONF_RECEIVER_ID): cv.use_id(
                remote_receiver.RemoteReceiverComponent
            ),
            # Expect the ID of an existing TextSensor
            cv.Required(CONF_MODE_SENSOR): cv.use_id(text_sensor.TextSensor),
            cv.Required(CONF_FAN_MODE_SENSOR): cv.use_id(text_sensor.TextSensor),
            cv.Required(CONF_TARGET_TEMPERATURE_SENSOR): cv.use_id(text_sensor.TextSensor),
            cv.Required(CONF_SWING_MODE_SENSOR): cv.use_id(text_sensor.TextSensor),
        }
    ).extend(cv.COMPONENT_SCHEMA), # Extend the base schema
    
    # Add the dependency check here, parallel to the base schema
    cv.requires_component("remote_receiver")
)


async def to_code(config):
    # Create the C++ variable for your custom component
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Get the C++ variable for the remote_receiver component
    receiver = await cg.get_variable(config[CONF_RECEIVER_ID])
    
    # Call the setter on our component (var)
    cg.add(var.set_receiver_component(receiver))

    # Get the existing sensors by ID and link them
    mode_sens = await cg.get_variable(config[CONF_MODE_SENSOR])
    cg.add(var.set_mode_sensor(mode_sens))

    fan_mode_sens = await cg.get_variable(config[CONF_FAN_MODE_SENSOR])
    cg.add(var.set_fan_mode_sensor(fan_mode_sens))

    target_temp_sens = await cg.get_variable(config[CONF_TARGET_TEMPERATURE_SENSOR])
    cg.add(var.set_target_temperature_sensor(target_temp_sens))

    swing_mode_sens = await cg.get_variable(config[CONF_SWING_MODE_SENSOR])
    cg.add(var.set_swing_mode_sensor(swing_mode_sens))