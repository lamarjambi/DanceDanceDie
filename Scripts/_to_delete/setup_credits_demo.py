import unreal
# Demo ending credits: stare -> CreditsDemo (credits roll) -> DemoEnd ("it's not over") -> MainMenu.
# CreditsDemo = copy of CreditsScene whose game mode is BP_CreditsDemoGameMode (child of BP_CreditsGameMode, so it
# inherits the Lines/looks; only NextLevel differs). Points DemoMap's Cliffhanger actor at CreditsDemo. Safe to re-run.
#   exec(open(unreal.Paths.project_dir()+'Scripts/setup_credits_demo.py').read())
EAL=unreal.EditorAssetLibrary; ELS=unreal.EditorLoadingAndSavingUtils
EAS=unreal.get_editor_subsystem(unreal.EditorActorSubsystem); UES=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
SRC='/Game/UI/CreditsScene'; MAP='/Game/UI/CreditsDemo'; PARENT='/Game/UI/BP_CreditsGameMode'; BP='/Game/UI/BP_CreditsDemoGameMode'
NEXT='/Game/UI/DemoEnd'; CLUB='/Game/DemoMap'
assert EAL.does_asset_exist(PARENT), 'CREDITSDEMO: run setup_credits.py first'
if not EAL.does_asset_exist(BP):
  f=unreal.BlueprintFactory(); f.set_editor_property('parent_class',EAL.load_blueprint_class(PARENT))
  assert unreal.AssetToolsHelpers.get_asset_tools().create_asset('BP_CreditsDemoGameMode','/Game/UI',unreal.Blueprint,f), 'CREDITSDEMO: BP create failed'
gm=EAL.load_blueprint_class(BP)
unreal.get_default_object(gm).set_editor_property('next_level',NEXT)
assert EAL.save_asset(BP,False)
if not EAL.does_asset_exist(MAP): assert EAL.duplicate_asset(SRC,MAP), 'CREDITSDEMO: map copy failed'
assert ELS.load_map(MAP)
UES.get_editor_world().get_world_settings().set_editor_property('default_game_mode',gm)
assert unreal.EditorLevelLibrary.save_current_level()
club=[p for p in [CLUB] if EAL.does_asset_exist(p)] or [str(a.package_name) for a in unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_class(unreal.TopLevelAssetPath('/Script/Engine','World')) if str(a.asset_name)=='DemoMap']
assert club, 'CREDITSDEMO: DemoMap not found'
assert ELS.load_map(club[0])
dcls=unreal.load_class(None,'/Script/DanceDanceDie.CliffhangerDirector')
d=[a for a in EAS.get_all_level_actors() if a.get_class()==dcls]
assert len(d)==1, 'CREDITSDEMO: expected 1 Cliffhanger in DemoMap, found %d'%len(d)
d[0].set_editor_property('end_level',MAP)
assert unreal.EditorLevelLibrary.save_current_level()
unreal.log_warning('CREDITSDEMO OK: %s end_level=%s, %s next=%s'%(club[0],d[0].get_editor_property('end_level'),BP,unreal.get_default_object(gm).get_editor_property('next_level')))
