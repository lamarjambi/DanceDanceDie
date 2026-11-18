import unreal
# places the SongHUD actor (song title card + progress bar, Source/DanceDanceDie/SongHUD.*) in the club (DemoMap).
# safe to re-run: exec(open(unreal.Paths.project_dir()+'Scripts/setup_song_hud.py').read())
# title, sizes, colours, timings: SongHUD actor > Details > Song HUD (no rebuild).
CLUB = '/Game/CyberClub/DemoMap/DemoMap'
ELS = unreal.EditorLoadingAndSavingUtils
EAS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
cls = unreal.load_class(None, '/Script/DanceDanceDie.SongHUD')
assert cls, 'SongHUD class missing: rebuild C++ first'
assert ELS.load_map(CLUB), 'missing ' + CLUB
acts = EAS.get_all_level_actors()
assert any('BP_Spline' in a.get_class().get_name() for a in acts), 'no BP_Spline in ' + CLUB
hud = [a for a in acts if a.get_class() == cls]
assert len(hud) <= 1, 'more than one SongHUD in ' + CLUB
if not hud:
    a = EAS.spawn_actor_from_class(cls, unreal.Vector(0, 0, 100))
    a.set_actor_label('SongHUD')
    unreal.log_warning('SONGHUD placed')
assert unreal.EditorLevelLibrary.save_current_level()
unreal.log_warning('SONGHUD ok in ' + CLUB)
