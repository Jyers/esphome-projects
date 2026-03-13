import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_ID, CONF_ICON

from .. import Levoit, CONF_LEVOIT_ID, levoit_ns

CONF_TYPE = "type"

LevoitBinarySensor = levoit_ns.class_("LevoitBinarySensor", binary_sensor.BinarySensor, cg.Component)
BinarySensorType = levoit_ns.enum("BinarySensorType")

TYPE_MAP = {
    "filter_low": BinarySensorType.FILTER_LOW,
    # Superior only below
    "cover_removed": BinarySensorType.COVER_REMOVED,
    "water_tank_empty": BinarySensorType.WATER_TANK_EMPTY,
    "dry_active": BinarySensorType.DRY_ACTIVE,
    "humidifying": BinarySensorType.HUMIDIFYING,
}

ICON_MAP = {
    "filter_low": None,
    "cover_removed": "mdi:window-open",
    "water_tank_empty": "mdi:waves-arrow-up",
    "dry_active": "mdi:hair-dryer",
    "humidifying": "mdi:air-humidifier",
}

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema(LevoitBinarySensor).extend(
    {
        cv.Required(CONF_LEVOIT_ID): cv.use_id(Levoit),
        cv.Required(CONF_TYPE): cv.one_of(*TYPE_MAP.keys(), lower=True),
        cv.Optional(CONF_ICON): cv.icon,
    }
)

async def to_code(config):
    parent = await cg.get_variable(config[CONF_LEVOIT_ID])

    sensor_type = config[CONF_TYPE]
    if CONF_ICON not in config:
        icon = ICON_MAP.get(sensor_type)
        if icon:
            config[CONF_ICON] = icon

    var = cg.new_Pvariable(config[CONF_ID])
    await binary_sensor.register_binary_sensor(var, config)
    await cg.register_component(var, config)

    cg.add(var.set_parent(parent))

    bst = TYPE_MAP[sensor_type]
    cg.add(var.set_type(bst))
    cg.add(parent.register_binary_sensor(bst, var))
