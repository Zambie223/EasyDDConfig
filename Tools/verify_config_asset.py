"""Loads /EasyDDConfig/EasyDDConfiguration in a fresh editor process and prints what was saved.

Run headless from the project root (the editor must not be open):
  UnrealEditor-Cmd.exe FactoryGame.uproject -run=pythonscript -script="<this file>" -unattended -nullrhi -nosplash
"""
import unreal

cls = unreal.load_class(None, "/EasyDDConfig/EasyDDConfiguration.EasyDDConfiguration_C")
if cls is None:
    unreal.log_error("[EasyDDConfig] VERIFY: class not found")
    raise RuntimeError("class not found")
cdo = unreal.get_default_object(cls)
config_id = cdo.get_editor_property("config_id")
unreal.log(f"[EasyDDConfig] VERIFY config_id = {config_id.get_editor_property('mod_reference')!r} / {config_id.get_editor_property('config_category')!r}")
unreal.log(f"[EasyDDConfig] VERIFY display_name = {cdo.get_editor_property('display_name')}")
root = cdo.get_editor_property("root_section")
if root is None:
    unreal.log_error("[EasyDDConfig] VERIFY: root_section is None")
    raise RuntimeError("root_section missing")
unreal.log(f"[EasyDDConfig] VERIFY root_section class = {root.get_class().get_path_name()} outer = {root.get_outer().get_path_name()}")
unreal.log(f"[EasyDDConfig] VERIFY root_section widget_type = {root.get_editor_property('widget_type')}")
for key, prop in root.get_editor_property("section_properties").items():
    is_float = "Float" in prop.get_class().get_name()
    unreal.log(
        "[EasyDDConfig] VERIFY {} -> {} default={} {} world_reload={} name='{}'".format(
            key,
            prop.get_class().get_name(),
            prop.get_editor_property("default_value"),
            "min={} max={} widget={}".format(
                prop.get_editor_property("min_value"),
                prop.get_editor_property("max_value"),
                prop.get_editor_property("widget_type"),
            ) if is_float else "",
            prop.get_editor_property("requires_world_reload"),
            prop.get_editor_property("display_name"),
        )
    )
