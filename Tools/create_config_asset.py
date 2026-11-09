"""Creates or updates /EasyDDConfig/EasyDDConfiguration, the mod's SML Mod Configuration asset.

Run headless from the project root (the editor must not be open):
  UnrealEditor-Cmd.exe FactoryGame.uproject -run=pythonscript -script="<this file>" -unattended -nullrhi -nosplash

The asset is a Blueprint because SML's Mod Options widgets come from its Blueprint property classes
(the native UConfigProperty::CreateEditorWidget returns null). Keys must match EasyDDConfigConstants.h.
"""
import unreal

ASSET_DIR = "/EasyDDConfig"
ASSET_NAME = "EasyDDConfiguration"
ASSET_PATH = f"{ASSET_DIR}/{ASSET_NAME}"
SML_PROPS = "/SML/Interface/UI/Menu/Mods/ConfigProperties"

HOST_NOTE = "In multiplayer the host's or server's value is used."

PROPERTIES = [
    {
        "key": "UploadSpeedMultiplier",
        "display_name": "Upload speed multiplier",
        "tooltip": (
            "Multiplies how fast Dimensional Depot Uploaders, and your own upload slots, send items, at every research level.\n"
            "1.0 = vanilla (15 items/min at base, 240 with all research). 2.0 doubles it.\n" + HOST_NOTE
        ),
        "default": 1.0,
        "min": 0.1,
        "max": 20.0,
    },
    {
        "key": "StackLimitMultiplier",
        "display_name": "Stack limit multiplier",
        "tooltip": (
            "Multiplies how many stacks of each item the Dimensional Depot can hold, including research upgrades.\n"
            "1.0 = vanilla (1 stack at base, 5 with all research). 3.0 triples it. It can't go below vanilla.\n" + HOST_NOTE
        ),
        "default": 1.0,
        "min": 1.0,
        "max": 100.0,
    },
]


def fail(message):
    unreal.log_error(f"[EasyDDConfig] {message}")
    raise RuntimeError(message)


def load_sml_class(name):
    cls = unreal.load_class(None, f"{SML_PROPS}/{name}.{name}_C")
    if cls is None:
        fail(f"Could not load SML class {name}")
    return cls


def generated_class_of(blueprint):
    try:
        cls = unreal.BlueprintEditorLibrary.generated_class(blueprint)
        if cls is not None:
            return cls
    except Exception as error:  # library may be unavailable; fall back to loading by path
        unreal.log_warning(f"[EasyDDConfig] BlueprintEditorLibrary.generated_class failed: {error}")
    return unreal.load_class(None, f"{ASSET_PATH}.{ASSET_NAME}_C")


def get_or_create_blueprint():
    """Updates the existing asset in place, so the asset never has to be deleted.

    Tries loading first: in the headless commandlet the asset registry has not scanned the plugin, so
    does_asset_exist() says False for an asset that exists, and create_asset() then collides with it.
    """
    blueprint = unreal.load_asset(ASSET_PATH)
    if blueprint is not None:
        return blueprint
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.ModConfiguration)
    blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(ASSET_NAME, ASSET_DIR, unreal.Blueprint, factory)
    if blueprint is None:
        fail("create_asset returned None")
    return blueprint


def get_or_create_subobject(existing, cls, outer, name):
    """Reuses an existing instanced subobject of the right class, so re-running never clashes on the object name."""
    if existing is not None and existing.get_class() == cls:
        return existing
    return unreal.new_object(cls, outer=outer, name=name)


def main():
    blueprint = get_or_create_blueprint()
    cls = generated_class_of(blueprint)
    if cls is None:
        fail("No generated class for the Blueprint")
    cdo = unreal.get_default_object(cls)

    section_cls = load_sml_class("BP_ConfigPropertySection")
    float_cls = load_sml_class("BP_ConfigPropertyFloat")

    cdo.modify()
    root = get_or_create_subobject(cdo.get_editor_property("root_section"), section_cls, cdo, "RootSection")
    root.modify()
    existing_properties = root.get_editor_property("section_properties") or {}

    section_properties = {}
    for spec in PROPERTIES:
        prop = get_or_create_subobject(existing_properties.get(spec["key"]), float_cls, root, spec["key"])
        prop.modify()
        prop.set_editor_property("display_name", spec["display_name"])
        prop.set_editor_property("tooltip", spec["tooltip"])
        prop.set_editor_property("default_value", spec["default"])
        prop.set_editor_property("value", spec["default"])
        prop.set_editor_property("widget_type", unreal.CP_FloatWidgetType.CPF_SPINBOX)
        prop.set_editor_property("min_value", spec["min"])
        prop.set_editor_property("max_value", spec["max"])
        prop.set_editor_property("requires_world_reload", False)
        section_properties[spec["key"]] = prop
    root.set_editor_property("section_properties", section_properties)
    # SML defaults sections to a horizontal list (with a scroll bar); the Mods menu reads top to bottom, so stack them.
    root.set_editor_property("widget_type", unreal.CP_SectionWidgetType.CPS_VERTICAL)

    config_id = unreal.ConfigId()
    config_id.set_editor_property("mod_reference", "EasyDDConfig")
    config_id.set_editor_property("config_category", "")
    cdo.set_editor_property("config_id", config_id)
    cdo.set_editor_property("display_name", "Easy DD Config")
    cdo.set_editor_property("description", "Dimensional Depot upload speed and stack limit. 1.0 = vanilla.")
    cdo.set_editor_property("root_section", root)

    if not unreal.EditorAssetLibrary.save_loaded_asset(blueprint, False):
        fail("Saving the asset failed")
    unreal.log(f"[EasyDDConfig] Saved {ASSET_PATH}")

main()
