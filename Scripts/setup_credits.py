import unreal
# Credits: makes /Game/UI/BP_CreditsGameMode (child of C++ CreditsGameMode) and sets it as CreditsScene's game mode.
# Edit the credits in BP_CreditsGameMode -> Class Defaults -> Credits (no rebuild). Re-running is safe.
#   exec(open(unreal.Paths.project_dir()+'Scripts/setup_credits.py').read())
EAL=unreal.EditorAssetLibrary
BP='/Game/UI/BP_CreditsGameMode'; MAP='/Game/UI/CreditsScene'
parent=unreal.load_class(None,'/Script/DanceDanceDie.CreditsGameMode')
if not parent: raise RuntimeError('CREDITS: C++ CreditsGameMode not found, rebuild first')
if not EAL.does_asset_exist(BP):
  f=unreal.BlueprintFactory(); f.set_editor_property('parent_class',parent)
  unreal.AssetToolsHelpers.get_asset_tools().create_asset('BP_CreditsGameMode','/Game/UI',unreal.Blueprint,f)
  EAL.save_asset(BP)
gm=unreal.EditorAssetLibrary.load_blueprint_class(BP)
if not EAL.does_asset_exist(MAP): raise RuntimeError('CREDITS: '+MAP+' missing')
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(MAP)
w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
w.get_world_settings().set_editor_property('default_game_mode',gm)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
unreal.log_warning('CREDITS OK: '+MAP+' game mode = '+gm.get_path_name())
