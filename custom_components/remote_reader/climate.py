
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate, remote_transmitter, sensor, remote_receiver, remote_base
from esphome.const import CONF_ID, CONF_SENSOR

CONF_RECEIVER_ID = "receiver_id"
remote_reader_ac_ns = cg.esphome_ns.namespace("remote_reader_ac")

Remote_readerACClimate = remote_reader_ac_ns.class_(
    "RemoteReaderACClimate",
    climate.Climate,
    cg.Component,
    remote_base.RemoteReceiverListener
)

CONFIG_SCHEMA = climate.climate_schema(Remote_readerACClimate).extend(
    {
        cv.Required(remote_transmitter.CONF_TRANSMITTER_ID): cv.use_id(
            remote_transmitter.RemoteTransmitterComponent
        ),
        cv.Optional(CONF_SENSOR): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_RECEIVER_ID): cv.use_id(remote_receiver.RemoteReceiverComponent),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await climate.register_climate(var, config)

    transmitter = await cg.get_variable(config[remote_transmitter.CONF_TRANSMITTER_ID])
    cg.add(var.set_transmitter(transmitter))

    if CONF_SENSOR in config:
        sens = await cg.get_variable(config[CONF_SENSOR])
        cg.add(var.set_sensor(sens))

    if CONF_RECEIVER_ID in config:
        receiver = await cg.get_variable(config[CONF_RECEIVER_ID])
        cg.add(receiver.register_listener(var))