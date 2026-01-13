import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button
from esphome.const import CONF_ID

from .. import Levoit, CONF_LEVOIT_ID, levoit_ns

CONF_TYPE = "type"

LevoitButton = levoit_ns.class_("LevoitButton", button.Button, cg.Component)
ButtonType = levoit_ns.enum("ButtonType")

TYPE_MAP = {
    "reset_filter_stats": ButtonType.RESET_FILTER_STATS,
}

CONFIG_SCHEMA = button.button_schema(LevoitButton).extend(
    {
        cv.Required(CONF_LEVOIT_ID): cv.use_id(Levoit),
        cv.Required(CONF_TYPE): cv.one_of(*TYPE_MAP.keys(), lower=True),
    }
)

async def to_code(config):
    parent = await cg.get_variable(config[CONF_LEVOIT_ID])

    var = cg.new_Pvariable(config[CONF_ID])
    await button.register_button(var, config)
    await cg.register_component(var, config)

    cg.add(var.set_parent(parent))

    bt = TYPE_MAP[config[CONF_TYPE]]
    cg.add(var.set_type(bt))
    cg.add(parent.register_button(bt, var))
