import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import (
    climate,
    remote_transmitter,
    sensor,
    remote_receiver,
    remote_base,
)
from esphome.const import CONF_ID, CONF_SENSOR

# Make sure these dependencies are prepared before our component
DEPENDENCIES = ["remote_transmitter", "remote_receiver"]
AUTO_LOAD = ["climate"]

CONF_RECEIVER_ID = "receiver_id"

saijo_ac_ns = cg.esphome_ns.namespace("saijo_ac")

SaijoACClimate = saijo_ac_ns.class_(
    "SaijoACClimate",
    climate.Climate,
    cg.Component,
    remote_base.RemoteReceiverListener,
)

CONFIG_SCHEMA = climate.climate_schema(SaijoACClimate).extend(
    {
        cv.Required(remote_transmitter.CONF_TRANSMITTER_ID): cv.use_id(
            remote_transmitter.RemoteTransmitterComponent
        ),
        cv.Optional(CONF_SENSOR): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_RECEIVER_ID): cv.use_id(
            remote_receiver.RemoteReceiverComponent
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    await cg.register_component(var, config)
    await climate.register_climate(var, config)

    tx = await cg.get_variable(config[remote_transmitter.CONF_TRANSMITTER_ID])
    cg.add(var.set_transmitter(tx))

    if CONF_SENSOR in config:
        sens = await cg.get_variable(config[CONF_SENSOR])
        cg.add(var.set_sensor(sens))

    if CONF_RECEIVER_ID in config:
        rcvr = await cg.get_variable(config[CONF_RECEIVER_ID])
        cg.add(rcvr.register_listener(var))