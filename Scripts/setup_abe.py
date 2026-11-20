import unreal
# Abe as the second playable character. EDITOR ONLY (duplicate_and_retarget crashes in a commandlet). safe to re-run:
#   exec(open(unreal.Paths.project_dir()+'Scripts/setup_abe.py').read())
# 1. player set: IK_AbePlayer (copy of Michelle's IK_Mixamo) + RTG_Michelle_to_Abe, then ABP_Michelle, the four lane
#    dance montages (AM_Up/Down/Left/Right) and everything they use are retargeted to Abe -> /Game/Characters/Abe/Anims/*_Abe.
#    BS_IdleWalk_Abe then uses Abe's own abe-idle / abe-walking (root locked: the Mixamo walk isn't in place).
# 2. party set: IK_Abe (copy of IK_Jackie) + RTG_Bobby_to_Abe in NPC/Dance/Retarget, so setup_party.py can use 'Abe'.
#    The club dances are retarget to Dance_<Dance>_Abe.
# Abe's rest pose has his feet wider apart than the others: both retargeters auto-align his leg bones to the source pose.
# FORCE=True before exec = redo the retargets (deletes the old *_Abe copies first).
FORCE = globals().get('FORCE', False)
ABE_DIR = '/Game/Characters/Abe'
OUT = ABE_DIR + '/Anims'
RIG_DIR = ABE_DIR + '/Retarget'
NPC = '/Game/Characters/NPC'
RT = NPC + '/Dance/Retarget'
PLAYER_ASSETS = ['/Game/Characters/Michelle/ABP_Michelle'] + ['/Game/Mixamo/AM_' + d for d in ('Up', 'Down', 'Left', 'Right')]
DANCES = ('HipHop', 'Silly', 'Gangnam')

EAL = unreal.EditorAssetLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
ar = unreal.AssetRegistryHelpers.get_asset_registry()
SRC, TGT = unreal.RetargetSourceOrTarget.SOURCE, unreal.RetargetSourceOrTarget.TARGET
log = lambda s: unreal.log_warning('ABE ' + s)
def save(path): assert EAL.save_asset(path, only_if_is_dirty=False), 'could not save ' + path

abe = unreal.load_asset(ABE_DIR + '/abe-walking')
michelle = unreal.load_asset('/Game/Characters/Michelle/michelle')
bobby = unreal.load_asset(NPC + '/Bobby/Bobby')
assert abe and michelle and bobby, 'missing a mesh'
abe_skel = abe.get_editor_property('skeleton')

def rig_copy(src_path, dst_path):
    if not EAL.does_asset_exist(dst_path):
        assert EAL.duplicate_asset(src_path, dst_path), 'could not copy %s' % src_path
    rig = unreal.load_asset(dst_path)
    c = unreal.IKRigController.get_controller(rig)
    assert c.set_skeletal_mesh(abe), 'Abe does not fit ' + dst_path
    for ch in c.get_retarget_chains():  # same Mixamo bone names, so every chain should still resolve
        n = ch.get_editor_property('chain_name')
        assert str(c.get_retarget_chain_start_bone(n)) != 'None', '%s chain %s lost its bones on Abe' % (dst_path, n)
    save(dst_path)
    return rig

def retargeter(path, src_rig, src_mesh, tgt_rig, copy_from=None):
    if not EAL.does_asset_exist(path):
        if copy_from:
            assert EAL.duplicate_asset(copy_from, path), 'could not copy ' + copy_from
        else:
            folder, name = path.rsplit('/', 1)
            assert AT.create_asset(name, folder, unreal.IKRetargeter, unreal.IKRetargetFactory()), 'could not create ' + path
    rtg = unreal.load_asset(path)
    c = unreal.IKRetargeterController.get_controller(rtg)
    c.set_ik_rig(SRC, src_rig); c.set_preview_mesh(SRC, src_mesh)
    c.set_ik_rig(TGT, tgt_rig); c.set_preview_mesh(TGT, abe)
    c.auto_map_chains(unreal.AutoMapChainType.EXACT, True)
    c.reset_retarget_pose(c.get_current_retarget_pose_name(TGT), [], TGT)
    # legs only: his arms are already a T like the others', and Bobby's head leans forward (aligning the neck would copy that)
    c.auto_align_bones([s + b for s in ('Left', 'Right') for b in ('UpLeg', 'Leg', 'Foot')], unreal.RetargetAutoAlignMethod.CHAIN_TO_CHAIN, TGT)
    c.snap_bone_to_ground('LeftToeBase', TGT)
    save(path)
    return rtg

def move_new(made, suffix, dst_folder):
    # duplicate_and_retarget names its copies <name><suffix> wherever it likes: move them into dst_folder
    moved = []
    for d in made:
        p = str(d.package_name)
        name = p.rsplit('/', 1)[1]
        dst = '%s/%s' % (dst_folder, name)
        if p != dst:
            assert EAL.rename_asset(p, dst), 'could not move %s to %s' % (p, dst)
        moved.append(dst)
    for d in made:  # redirectors left by the moves
        p = str(d.package_name)
        if p not in moved and EAL.does_asset_exist(p):
            a = ar.get_asset_by_object_path(p + '.' + p.rsplit('/', 1)[1])
            if str(a.asset_class_path.asset_name) == 'ObjectRedirector': EAL.delete_asset(p)
    for m in moved: save(m)
    return moved

def wipe(folder, suffix):
    for d in ar.get_assets_by_path(folder, recursive=False):
        if str(d.asset_name).endswith(suffix): EAL.delete_asset(str(d.package_name))

# ---- 1. player
EAL.make_directory(OUT); EAL.make_directory(RIG_DIR)
player_rig = rig_copy('/Game/Mixamo/IK_Mixamo', RIG_DIR + '/IK_AbePlayer')
rtg_m = retargeter(RIG_DIR + '/RTG_Michelle_to_Abe', unreal.load_asset('/Game/Mixamo/IK_Mixamo'), michelle, player_rig)
if FORCE: wipe(OUT, '_Abe')
if not EAL.does_asset_exist(OUT + '/BS_IdleWalk_Abe'):  # NOT the ABP: a re-run before it was moved would retarget everything twice
    src = [EAL.find_asset_data(p) for p in PLAYER_ASSETS]
    assert all(s.is_valid() for s in src), 'missing one of ' + str(PLAYER_ASSETS)
    made = unreal.IKRetargetBatchOperation.duplicate_and_retarget(src, michelle, abe, rtg_m, suffix='_Abe', include_referenced_assets=True)
    log('player retarget made: ' + ', '.join(str(d.package_name) for d in made))
    move_new(made, '_Abe', OUT)
# the ABP copy is made but not returned: sweep every *_Abe left in the content root into OUT
stray = [d for d in ar.get_assets_by_path('/Game', recursive=False) if str(d.asset_name).endswith('_Abe')]
if stray: log('moving strays: ' + ', '.join(str(d.package_name) for d in stray)); move_new(stray, '_Abe', OUT)
assert EAL.does_asset_exist(OUT + '/ABP_Michelle_Abe'), 'ABP_Michelle was not retargeted (see log above)'
abp = unreal.load_asset(OUT + '/ABP_Michelle_Abe')
assert abp.get_editor_property('target_skeleton') == abe_skel, 'ABP_Michelle_Abe is not on Abe\'s skeleton'

# Abe's own idle + walk in the copied blend space
walk = unreal.load_asset(ABE_DIR + '/abe-walking_Anim_mixamo_com')
idle = unreal.load_asset(ABE_DIR + '/abe-idle')
walk.set_editor_property('force_root_lock', True)  # Michelle's A_Walking has it too: the hips travel 1.7 m per loop
save(walk.get_path_name().split('.')[0])
bs = unreal.load_asset(OUT + '/BS_IdleWalk_Abe')
assert bs, 'no BS_IdleWalk_Abe'
# setting sample_data from Python doesn't stick in 5.4: replace Michelle's retargeted idle/walk everywhere with Abe's own
for mine, copy in ((idle, OUT + '/A_Idle_Abe'), (walk, OUT + '/A_Walking_Abe')):
    if EAL.does_asset_exist(copy):
        assert EAL.consolidate_assets(mine, [unreal.load_asset(copy)]), 'could not swap %s for %s' % (copy, mine.get_name())
        if EAL.does_asset_exist(copy): EAL.delete_asset(copy)  # redirector
save(OUT + '/BS_IdleWalk_Abe')
bs = unreal.load_asset(OUT + '/BS_IdleWalk_Abe')
log('blend space: ' + ', '.join('%s@%.0f' % (s.get_editor_property('animation').get_name(), s.get_editor_property('sample_value').x)
                               for s in bs.get_editor_property('sample_data')))

# ---- 2. party dances
party_rig = rig_copy(RT + '/IK_Jackie', RT + '/IK_Abe')
rtg_b = retargeter(RT + '/RTG_Bobby_to_Abe', unreal.load_asset(RT + '/IK_Bobby'), bobby, party_rig, copy_from=RT + '/RTG_Bobby_to_Jackie')
for dance in DANCES:
    dst = '%s/Dance/Dance_%s_Abe' % (NPC, dance)
    if FORCE and EAL.does_asset_exist(dst): EAL.delete_asset(dst)
    if EAL.does_asset_exist(dst): continue
    made = unreal.IKRetargetBatchOperation.duplicate_and_retarget([EAL.find_asset_data('%s/Dance/Dance_%s' % (NPC, dance))],
                                                                  bobby, abe, rtg_b, suffix='_Abe', include_referenced_assets=False)
    move_new(made, '_Abe', NPC + '/Dance')
    assert EAL.does_asset_exist(dst), 'retarget to %s failed' % dst

# ---- check: Abe's copies keep the source's stance (his rest pose has wider feet: auto-align should remove that)
APE = unreal.AnimPoseExtensions
def feet(anim, t=0.0):
    p = APE.get_anim_pose_at_time(anim, t, unreal.AnimPoseEvaluationOptions())
    l, r = (APE.get_bone_pose(p, b, unreal.AnimPoseSpaces.WORLD).translation for b in ('LeftFoot', 'RightFoot'))
    h = APE.get_bone_pose(p, 'Hips', unreal.AnimPoseSpaces.WORLD).translation.z
    return abs(l.x - r.x), h
for src, dst in [('/Game/Mixamo/A_Up_HipHop', OUT + '/A_Up_HipHop_Abe'), ('/Game/Mixamo/A_Down_Flair', OUT + '/A_Down_Flair_Abe'),
                 (NPC + '/Dance/Dance_HipHop', NPC + '/Dance/Dance_HipHop_Abe')]:
    a, b = unreal.load_asset(src), unreal.load_asset(dst)
    if not (a and b): log('check: missing %s or %s' % (src, dst)); continue
    (fa, ha), (fb, hb) = feet(a), feet(b)
    log('check %s: feet apart %.1f -> %.1f, hips z %.1f -> %.1f' % (b.get_name(), fa, fb, ha, hb))
log('ok: ' + ', '.join(str(d.asset_name) for d in ar.get_assets_by_path(OUT, recursive=False)))
