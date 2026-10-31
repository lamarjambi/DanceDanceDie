# run in editor python console:  py "Scripts/setup_intro_chat.py" (path relative to project) 
import unreal

BP_PATH = "/Game/UI"
BP_NAME = "BP_IntroChatGameMode"
LEVEL = "/Game/UI/IntroChat"

parent = unreal.load_class(None, "/Script/DanceDanceDie.IntroChatGameMode")
assert parent, "IntroChatGameMode C++ class not found - did the module rebuild?"

full = f"{BP_PATH}/{BP_NAME}"
if unreal.EditorAssetLibrary.does_asset_exist(full):
    bp = unreal.EditorAssetLibrary.load_asset(full)
else:
    f = unreal.BlueprintFactory()
    f.set_editor_property("parent_class", parent)
    bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(BP_NAME, BP_PATH, unreal.Blueprint, f)
    assert bp, "failed to create BP"
    unreal.EditorAssetLibrary.save_loaded_asset(bp)

gen = unreal.load_class(None, f"{full}.{BP_NAME}_C")
assert gen, "generated class missing"

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert les.load_level(LEVEL), "could not load IntroChat"
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
ws = world.get_world_settings()
ws.set_editor_property("default_game_mode", gen)
assert les.save_current_level(), "save failed"
print("INTRO_CHAT_SETUP_OK", gen.get_name(), ws.get_editor_property("default_game_mode"))
