import unreal
# dumps club party NPCs (tag PartyNPC), sequences, audio actors -> Saved/party_dump.txt
# run with DemoMap open: exec(open(unreal.Paths.project_dir()+'Scripts/dump_party.py').read())
out=[]
w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
out.append('WORLD %s'%w.get_path_name())
gm=w.get_world_settings().get_editor_property('default_game_mode')
out.append('GM %s'%(gm.get_path_name() if gm else None))
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    c=a.get_class().get_name(); l=a.get_actor_label(); tags=[str(t) for t in a.tags]
    if 'PartyNPC' in tags:
        out.append('PARTY %s cls=%s loc=%s rot=%s hidden=%s root=%s'%(l,c,a.get_actor_location(),a.get_actor_rotation(),a.is_hidden_ed() if hasattr(a,'is_hidden_ed') else '?',a.root_component.get_class().get_name()))
        try: out.append('   bHidden=%s'%a.get_editor_property('hidden'))
        except Exception as e: out.append('   hidden? %s'%e)
        for m in a.get_components_by_class(unreal.SkeletalMeshComponent):
            d=[m.get_name(), 'mode=%s'%m.get_editor_property('animation_mode')]
            try: d.append('anim=%s'%m.get_editor_property('animation_data').get_editor_property('anim_to_play').get_path_name())
            except Exception as e: d.append('anim? %s'%e)
            try: d.append('animclass=%s'%m.get_editor_property('anim_class'))
            except Exception as e: pass
            sk=m.get_skeletal_mesh_asset() if hasattr(m,'get_skeletal_mesh_asset') else None
            d.append('mesh=%s'%(sk.get_path_name() if sk else None))
            d.append('relrot=%s'%m.get_editor_property('relative_rotation'))
            d.append('bones=%s'%[str(b) for b in ('HeadTop_End','Head','head') if m.get_bone_index(b)>=0])
            out.append('   SKM '+' '.join(d))
    elif 'Sequence' in c or 'Sound' in c or 'audio' in c.lower() or 'Spline' in c or 'PlayerStart' in c or 'Talk' in c or 'Cliff' in c or 'Trigger' in c:
        out.append('ACT %s cls=%s loc=%s tags=%s'%(l,c,a.get_actor_location(),tags))
open(unreal.Paths.project_saved_dir()+'party_dump.txt','w').write('\n'.join(out))
unreal.log_warning('PARTYDUMP OK %d'%len(out))
