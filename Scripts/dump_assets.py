import unreal
# dumps BP/WBP assets + NPC level actors -> Saved/assets_dump.txt
ar=unreal.AssetRegistryHelpers.get_asset_registry()
out=[]
for d in ar.get_assets_by_path('/Game',recursive=True):
    c=str(d.asset_class_path.asset_name); p=str(d.package_name)
    if c in('Blueprint','WidgetBlueprint','AnimBlueprint') and not p.startswith('/Game/NightCity'):
        bp=unreal.load_asset(p)
        try: par=bp.get_editor_property('parent_class').get_name()
        except Exception as e: par='?'
        out.append('%s %s parent=%s'%(c,p,par))
    if p.startswith('/Game/UI/Images'): out.append('IMG %s %s'%(c,p))
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    l=a.get_actor_label()
    if l.startswith('NPC') or 'Jackie' in l or 'Bobby' in l or 'Jolleen' in l or isinstance(a,(unreal.PlayerStart,)):
        out.append('ACT %s %s %s'%(l,a.get_class().get_name(),a.get_actor_location()))
gm=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_world_settings().get_editor_property('default_game_mode')
out.append('WORLD %s gm=%s'%(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_path_name(), gm.get_name() if gm else None))
open(unreal.Paths.project_saved_dir()+'assets_dump.txt','w').write('\n'.join(out))
unreal.log('DUMP OK %d'%len(out))
