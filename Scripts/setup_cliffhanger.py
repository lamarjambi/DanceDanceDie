import unreal
# demo ending setup. places the Cliffhanger actor in the club (DemoMap), gives it one idle anim per dancer skeleton
# (retargets Bobby's idle for skeletons that have none), and makes DemoEnd use DemoEndGameMode.
# safe to re-run: exec(open(unreal.Paths.project_dir()+'Scripts/setup_cliffhanger.py').read())
# lines / timings are edited on the Cliffhanger actor's Details panel. the values below are only used when the actor is first created.
LINES = ["Michelle!! You made it!", "You dance so well.", "Stay."]  # 1st, 2nd, 3rd E press. the last one starts the flicker
IDLE_TIMEOUT = 10.0   # secs without an E press before the flicker starts anyway
CLUB = '/Game/CyberClub/DemoMap/DemoMap'
END = '/Game/UI/DemoEnd'
NPC_DIR = '/Game/Characters/NPC'
RTG_DIR = NPC_DIR + '/Dance/Retarget'
IDLE_SOURCE = 'Bobby'  # retarget source for skeletons without an idle (needs RTG_Bobby_to_<Name>)

ELS = unreal.EditorLoadingAndSavingUtils
EAL = unreal.EditorAssetLibrary
EAS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
UES = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
AR = unreal.AssetRegistryHelpers.get_asset_registry()
dcls = unreal.load_class(None, '/Script/DanceDanceDie.CliffhangerDirector')
gcls = unreal.load_class(None, '/Script/DanceDanceDie.DemoEndGameMode')
assert dcls and gcls, 'Cliffhanger classes missing: rebuild C++ first'

assert ELS.load_map(END), 'missing ' + END
ws = UES.get_editor_world().get_world_settings()
if ws.get_editor_property('default_game_mode') != gcls:
    ws.set_editor_property('default_game_mode', gcls)
    assert unreal.EditorLevelLibrary.save_current_level()

assert ELS.load_map(CLUB), 'missing ' + CLUB
acts = EAS.get_all_level_actors()
party = [a for a in acts if 'PartyNPC' in [str(t) for t in a.tags]]
assert party, 'no actors tagged PartyNPC in ' + CLUB

def idle_assets():
    # skeleton path -> idle AssetData, for every anim under NPC_DIR with "idle" in its name
    out = {}
    for ad in AR.get_assets_by_path(NPC_DIR, recursive=True):
        if str(ad.asset_class_path.asset_name) != 'AnimSequence' or 'idle' not in str(ad.asset_name).lower():
            continue
        tag = str(ad.get_tag_value('Skeleton') or '')
        out[tag.split("'")[1] if "'" in tag else tag] = ad
    return out

meshes = {}  # skeleton path -> (mesh asset, character name)
for a in party:
    m = a.get_components_by_class(unreal.SkeletalMeshComponent)[0].get_skeletal_mesh_asset()
    meshes[m.get_editor_property('skeleton').get_path_name()] = (m, str(m.get_name()))

idles = idle_assets()
src_mesh = unreal.load_asset('%s/%s/%s' % (NPC_DIR, IDLE_SOURCE, IDLE_SOURCE))
src_idle = idles.get(src_mesh.get_editor_property('skeleton').get_path_name())
for skel, (mesh, name) in meshes.items():
    if skel in idles:
        continue
    rtg = unreal.load_asset('%s/RTG_%s_to_%s' % (RTG_DIR, IDLE_SOURCE, name))
    assert rtg and src_idle, 'no idle for %s and no RTG_%s_to_%s to make one' % (name, IDLE_SOURCE, name)
    made = unreal.IKRetargetBatchOperation.duplicate_and_retarget([src_idle], src_mesh, mesh, rtg, suffix='_' + name)
    assert made, 'retarget to %s failed' % name
    for ad in made:
        EAL.save_asset(str(ad.package_name))
        unreal.log_warning('CLIFF made idle for %s: %s' % (name, ad.package_name))
    idles[skel] = next(ad for ad in made if str(ad.asset_class_path.asset_name) == 'AnimSequence')
missing = [n for s, (m, n) in meshes.items() if s not in idles]
assert not missing, 'still no idle for %s' % missing
anims = [idles[s].get_asset() for s in meshes]

d = next((a for a in acts if a.get_class() == dcls), None)
if not d:
    d = EAS.spawn_actor_from_class(dcls, unreal.Vector(0, 0, 100))
    d.set_actor_label('Cliffhanger')
    d.set_editor_property('lines', LINES)
    d.set_editor_property('idle_timeout', IDLE_TIMEOUT)
    d.set_editor_property('end_level', END)
d.set_editor_property('idle_anims', anims)
assert unreal.EditorLevelLibrary.save_current_level()
unreal.log_warning('CLIFF ok: %d dancers, idles: %s' % (len(party), ', '.join('%s=%s' % (meshes[s][1], idles[s].asset_name) for s in meshes)))
