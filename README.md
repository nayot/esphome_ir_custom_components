# esphome_ir_custom_components
ESPHome custom components to control Air Conditioning Systems not yet supported by ESPHome. The library is based on reverse engineering of RAW timing signal from the A/C remote controllers.

| Manufacturer |  Model  | Custom Component Platform | Transmit | Receive | Remarks |
|--------------|---------|----------------|----------|----------|--------|
|  Carrier     |  Old |   carrier_ac         |   Yes    |   Yes    | custom_components |
| Carrier      | Cartridge | carrier_cartridge_rx | Yes | Yes | Tx: heatpumpir, Rx: custom_components, HA Automation for reflecting received states to a/c card|

