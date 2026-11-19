import unreal
# club party (DemoMap): Bobby's club-only idle + the extra dancers. safe to re-run:
#   exec(open(unreal.Paths.project_dir()+'Scripts/setup_party.py').read())
# - imports Scripts/assets/<CLUB_IDLE>.fbx onto Bobby's skeleton and makes it Bobby's entry in the Cliffhanger actor's Idle Anims.
#   the city is untouched: LS_Walk_Bobby keeps idle-bobby.
# - adds the dancers in EXTRA as copies of an existing Party_<Name>_* actor (same folder, PartyNPC tag, hidden, collision off).
#   a dance a character does not have yet is retargeted from Bobby's (needs RTG_Bobby_to_<Name>).
#   re-running moves / re-dances the ones that exist (matched by label), so edit EXTRA and run again. deleting one: delete it in the level + here.
# - collision stays OFF in the level on purpose (hidden dancers would be invisible walls). Cliffhanger.cpp makes them solid when the party starts.
# DRY=True before exec = change nothing on disk (headless test).
CLUB_IDLE = 'new-bobby-idle'   # fbx in Scripts/assets, asset in /Game/Characters/NPC/Bobby
EXTRA = [  # label, character, dance, x, y, yaw (meshes face +Y: body faces yaw + 90)
    ('Party_Bobby_7',   'Bobby',   'Silly',   1550, -250,  150),
    ('Party_Dameon_8',  'Dameon',  'Silly',   1600, -800,   20),
    ('Party_Hal_9',     'Hal',     'HipHop',  2000, -350,  100),
    ('Party_Jackie_10', 'Jackie',  'HipHop',  1200,  250, -170),
    ('Party_Hal_11',    'Hal',     'Silly',    350, -600,  -45),
    ('Party_Dameon_12', 'Dameon',  'Gangnam',  600,  350, -120),
    ('Party_Jolleen_13', 'Jolleen', 'HipHop', 1000, -800,   60),
]
CLUB = '/Game/CyberClub/DemoMap/DemoMap'
NPC_DIR = '/Game/Characters/NPC'
DRY = globals().get('DRY', False)

EAL = unreal.EditorAssetLibrary
EAS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
UES = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
log = lambda s: unreal.log_warning('PARTY ' + s)
def save_asset(path):
    if not DRY: assert EAL.save_asset(path), 'could not save ' + path

if UES.get_editor_world().get_path_name().split('.')[0] != CLUB:
    assert unreal.EditorLoadingAndSavingUtils.load_map(CLUB), 'missing ' + CLUB
acts = EAS.get_all_level_actors()
party = [a for a in acts if 'PartyNPC' in [str(t) for t in a.tags]]
assert party, 'no actors tagged PartyNPC in ' + CLUB
mesh_of = lambda a: a.get_components_by_class(unreal.SkeletalMeshComponent)[0]
char_of = lambda a: str(mesh_of(a).get_skeletal_mesh_asset().get_name())

# ---- Bobby's club idle
bobby = unreal.load_asset(NPC_DIR + '/Bobby/Bobby')
bobby_skel = bobby.get_editor_property('skeleton')
idle_path = '%s/Bobby/%s' % (NPC_DIR, CLUB_IDLE)
if not EAL.does_asset_exist(idle_path):
    ui = unreal.FbxImportUI()
    for k, v in (('import_mesh', False), ('import_as_skeletal', True), ('import_animations', True), ('import_materials', False),
                 ('import_textures', False), ('create_physics_asset', False), ('automated_import_should_detect_type', False),
                 ('mesh_type_to_import', unreal.FBXImportType.FBXIT_ANIMATION), ('skeleton', bobby_skel)):
        ui.set_editor_property(k, v)
    t = unreal.AssetImportTask()
    t.filename = unreal.Paths.project_dir() + 'Scripts/assets/%s.fbx' % CLUB_IDLE
    t.destination_path = NPC_DIR + '/Bobby'; t.destination_name = CLUB_IDLE
    t.replace_existing = False; t.automated = True; t.save = False; t.options = ui
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
    assert EAL.does_asset_exist(idle_path), 'import of %s failed (see the Output Log), got %s' % (t.filename, list(t.imported_object_paths))
    log('imported ' + idle_path)
idle = unreal.load_asset(idle_path)
assert isinstance(idle, unreal.AnimSequence) and idle.get_editor_property('skeleton') == bobby_skel, idle_path + ' is not an anim on Bobby_Skeleton'
save_asset(idle_path)  # also when it was imported by hand and never saved: the level is about to point at it

# same body size as the old idle? (an anim downloaded for another Mixamo character stretches the limbs)
def hips_z(anim):
    p = unreal.AnimPoseExtensions.get_anim_pose_at_time(anim, 0.0, unreal.AnimPoseEvaluationOptions())
    return unreal.AnimPoseExtensions.get_bone_pose(p, 'Hips', unreal.AnimPoseSpaces.WORLD).translation.z
old_h, new_h = hips_z(unreal.load_asset(NPC_DIR + '/Bobby/idle-bobby')), hips_z(idle)
log('%s len=%.2fs hips z=%.1f (idle-bobby %.1f)' % (CLUB_IDLE, idle.get_play_length(), new_h, old_h))
if abs(new_h - old_h) > 0.25 * old_h:
    unreal.log_error('PARTY %s hips are at %.1f, idle-bobby at %.1f: was it downloaded for Bobby? check him in the club' % (CLUB_IDLE, new_h, old_h))

director = next((a for a in acts if a.get_class().get_name() == 'CliffhangerDirector'), None)
assert director, 'no Cliffhanger actor in the club: run Scripts/setup_cliffhanger.py first'
idles = [a for a in director.get_editor_property('idle_anims') if a and a.get_editor_property('skeleton') != bobby_skel]
director.modify()
director.set_editor_property('idle_anims', [idle] + idles)
log('club idles: ' + ', '.join(str(a.get_name()) for a in director.get_editor_property('idle_anims')))

# ---- more dancers
def dance_for(char, dance):
    name = 'Dance_%s' % dance if char == 'Bobby' else 'Dance_%s_%s' % (dance, char)
    path = '%s/Dance/%s' % (NPC_DIR, name)
    if not EAL.does_asset_exist(path):
        stray = '/Game/' + name  # duplicate_and_retarget drops its copies in the content root
        if not EAL.does_asset_exist(stray):
            src = EAL.find_asset_data('%s/Dance/Dance_%s' % (NPC_DIR, dance))
            rtg = unreal.load_asset('%s/Dance/Retarget/RTG_Bobby_to_%s' % (NPC_DIR, char))
            tgt = unreal.load_asset('%s/%s/%s' % (NPC_DIR, char, char))
            assert src.is_valid() and rtg and tgt, 'no %s and nothing to retarget it from' % name
            made = unreal.IKRetargetBatchOperation.duplicate_and_retarget([src], bobby, tgt, rtg, suffix='_' + char)
            assert made and EAL.does_asset_exist(stray), 'retarget to %s failed, got %s' % (name, [str(m.package_name) for m in made])
        assert EAL.rename_asset(stray, path), 'could not move %s to %s' % (stray, path)
        if EAL.does_asset_exist(stray): EAL.delete_asset(stray)  # redirector left by the move
        save_asset(path)
        log('retargeted ' + path)
    anim = unreal.load_asset(path)
    assert isinstance(anim, unreal.AnimSequence), path + ' is not an anim'
    return anim

labels = [e[0] for e in EXTRA]
base = {}  # character -> the hand-placed dancers (the copies' source, and their floor height)
for a in party:
    if a.get_actor_label() not in labels: base.setdefault(char_of(a), []).append(a)
by_label = {a.get_actor_label(): a for a in party}
for i, (label, char, dance, x, y, yaw) in enumerate(EXTRA):
    assert char in base, 'no hand-placed %s in the club to copy' % char
    anim = dance_for(char, dance)
    a = by_label.get(label)
    if not a:
        a = EAS.duplicate_actor(base[char][0])
        assert a, 'could not copy ' + base[char][0].get_actor_label()
        a.set_actor_label(label)
    a.modify()
    z = sum(b.get_actor_location().z for b in base[char]) / len(base[char])  # feet height was tuned by hand per character
    a.set_actor_location_and_rotation(unreal.Vector(x, y, z), unreal.Rotator(0, 0, yaw), False, False)  # Rotator(roll, pitch, yaw)
    m = mesh_of(a)
    m.modify()
    m.override_animation_data(anim, True, True, (0.61 * (i + 1)) % anim.get_play_length(), 1.0)  # staggered so copies are out of step
    src = base[char][0]
    assert 'PartyNPC' in [str(t) for t in a.tags] and a.get_editor_property('hidden') == src.get_editor_property('hidden') and not a.get_actor_enable_collision(), label + ' did not copy cleanly'
    for o in [p for p in party if p != a] + [by_label[l] for l in labels[:i] if l in by_label and by_label[l] != a]:
        ol = o.get_actor_location()
        d = ((ol.x - x) ** 2 + (ol.y - y) ** 2) ** 0.5
        assert d > 150, '%s is %.0f uu from %s' % (label, d, o.get_actor_label())
    by_label[label] = a
    log('%s: %s dancing %s at (%d, %d, %.0f) yaw %d' % (label, char, anim.get_name(), x, y, z, yaw))

total = len([a for a in EAS.get_all_level_actors() if 'PartyNPC' in [str(t) for t in a.tags]])
if not DRY: assert unreal.EditorLevelLibrary.save_current_level()
log('ok%s: %d dancers in the club' % (' (dry run, nothing saved)' if DRY else '', total))
