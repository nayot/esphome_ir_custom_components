import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate, remote_transmitter, sensor
from esphome.const import (
    CONF_ID,
    CONF_SENSOR,  # Keep this import
)

# ... (namespace and class definitions are the same) ...
carrier_ac_ns = cg.esphome_ns.namespace("carrier_ac")
CarrierACClimate = carrier_ac_ns.class_(
    "CarrierACClimate", climate.Climate, cg.Component
)


# Use the new climate_schema function
CONFIG_SCHEMA = climate.climate_schema(CarrierACClimate).extend(
    {
        # Keep your existing keys inside here
        cv.Required(remote_transmitter.CONF_TRANSMITTER_ID): cv.use_id(
            remote_transmitter.RemoteTransmitterComponent
        ),
        cv.Optional(CONF_SENSOR): cv.use_id(sensor.Sensor),
    }
).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    
    await cg.register_component(var, config)
    await climate.register_climate(var, config)

    transmitter = await cg.get_variable(config[remote_transmitter.CONF_TRANSMITTER_ID])
    cg.add(var.set_transmitter(transmitter))
    
    # --- FIX 2: Add this 'if' statement ---
    # This only adds the sensor to C++ if it exists in the YAML
    if CONF_SENSOR in config:
        sens = await cg.get_variable(config[CONF_SENSOR])
        cg.add(var.set_sensor(sens))