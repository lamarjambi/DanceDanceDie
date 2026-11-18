import unreal
# pushes the current SongHUD timing/size defaults onto the placed actor in the open level (Live Coding doesn't refresh CDO defaults)
cls = unreal.load_class(None, '/Script/DanceDanceDie.SongHUD')
EAS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
hud = [a for a in EAS.get_all_level_actors() if a.get_class() == cls]
assert len(hud) == 1, 'need exactly one SongHUD in the open level, found %d' % len(hud)
for k, v in {'intro_hold': 1.5, 'move_time': 1.2, 'small_size': 56.0, 'fade_in_time': 0.5}.items():
    hud[0].set_editor_property(k, v)
assert unreal.EditorLevelLibrary.save_current_level()
unreal.log_warning('SONGHUD defaults applied')
