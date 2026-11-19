# Runs INSIDE a Standalone game (Scripts/look_test.ps1): looks down at the player's own body and takes screenshots.
# Changes nothing on disk except Saved/look_info.txt + Saved/Screenshots.
# Env DDD_LOOK_STEPS = "label:pitch:fwd:up:nearclip;..."  (fwd/up = camera push in cm, pawn space; nearclip 0 = leave).
# Default = the current look at a few pitches. DDD_LOOK_AT = "x,y,yaw" stands at a lit spot. DDD_LOOK_WALK=1 walks forward.
import unreal, os, math

MAP = '/Game/NightCityCreator/maps/NightCityCreator.NightCityCreator'
STEPS = os.environ.get('DDD_LOOK_STEPS', 'p0:0:0:0:0;p30:-30:0:0:0;p50:-50:0:0:0;p70:-70:0:0:0;p89:-89:0:0:0')
STEPS = [s.split(':') for s in STEPS.split(';') if s]
AT = os.environ.get('DDD_LOOK_AT', '')
WALK = os.environ.get('DDD_LOOK_WALK', '') == '1'
DELAY = float(os.environ.get('DDD_LOOK_DELAY', '7'))
OUT = unreal.Paths.project_dir() + 'Saved/look_info.txt'
S = {'t': 0.0, 'ready': False, 'i': -1, 'phase': 0, 'pt': 0.0, 'log': []}

def note(s):
    S['log'].append(s)
    unreal.log('LOOK ' + s)
    open(OUT, 'w').write('\n'.join(S['log']))

def v(x):
    return '(%.1f, %.1f, %.1f)' % (x.x, x.y, x.z)

def setup():
    w = unreal.find_object(None, MAP)
    if not w:
        return False
    pawn = unreal.GameplayStatics.get_player_pawn(w, 0)
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    if not pawn or not pc:
        return False
    S.update(world=w, pawn=pawn, pc=pc)
    S['boom'] = pawn.get_component_by_class(unreal.SpringArmComponent)
    S['cam'] = pawn.get_component_by_class(unreal.CameraComponent)
    S['mesh'] = pawn.get_component_by_class(unreal.SkeletalMeshComponent)
    r = S['cam'].relative_location
    S['base'] = (r.x, r.y, r.z)                   # plain floats: the struct is a live view of the component
    S['yaw'] = pc.get_control_rotation().yaw
    if AT:                                        # "x,y,yaw": stand there (lit spot) instead of the spawn
        x, y, S['yaw'] = [float(n) for n in AT.split(',')]
        pawn.set_actor_location(unreal.Vector(x, y, pawn.get_actor_location().z), False, True)
    note('pawn %s at %s yaw %.0f' % (pawn.get_class().get_name(), v(pawn.get_actor_location()), S['yaw']))
    for c in pawn.get_components_by_class(unreal.SceneComponent):
        par = c.get_attach_parent()
        note('comp %s (%s) parent %s socket %s rel %s rot %s world %s' % (
            c.get_name(), c.get_class().get_name(), par.get_name() if par else None, c.get_attach_socket_name(),
            v(c.relative_location), c.relative_rotation, v(c.get_world_location())))
    feet = pawn.get_actor_location().z - 90.0
    for b in ('Hips', 'Spine2', 'Neck', 'Head', 'HeadTop_End', 'LeftShoulder', 'RightShoulder', 'LeftToeBase'):
        l = S['mesh'].get_socket_location(b)
        note('bone %s world %s  above feet %.1f' % (b, v(l), l.z - feet))
    cm = pc.player_camera_manager
    note('view pitch min %.1f max %.1f' % (cm.view_pitch_min, cm.view_pitch_max))
    return True

def apply(step):
    label, pitch, fwd, up, clip = step[0], float(step[1]), float(step[2]), float(step[3]), float(step[4])
    S['pc'].set_control_rotation(unreal.Rotator(roll=0.0, pitch=pitch, yaw=S['yaw']))
    p = math.radians(pitch)
    # pawn-space push (fwd, up) -> boom space (boom is rotated by the control pitch)
    bx, by, bz = S['base']
    S['cam'].set_relative_location(unreal.Vector(bx + fwd * math.cos(p) + up * math.sin(p), by, bz - fwd * math.sin(p) + up * math.cos(p)), False, False)
    if WALK:                                      # walk forward so the body is in its walk pose
        S['pawn'].add_movement_input(unreal.Rotator(roll=0.0, pitch=0.0, yaw=S['yaw']).get_forward_vector(), 1.0, False)
    if clip > 0:
        unreal.SystemLibrary.execute_console_command(S['world'], 'r.SetNearClipPlane %s' % clip, S['pc'])

def tick(dt):
    try:
        if not S['ready']:
            S['ready'] = setup()
            return
        S['t'] += dt
        if S['t'] < DELAY:
            return
        S['pt'] += dt
        if S['phase'] == 0:                       # next step
            S['i'] += 1
            if S['i'] >= len(STEPS):
                note('done')
                unreal.unregister_slate_post_tick_callback(H)
                unreal.SystemLibrary.execute_console_command(S['world'], 'quit', S['pc'])
                return
            apply(STEPS[S['i']])
            S['phase'], S['pt'] = 1, 0.0
        elif S['phase'] == 1:
            apply(STEPS[S['i']])                  # hold it (the game may fight the rotation)
            if S['pt'] > 0.8:
                c = S['cam'].get_world_location()
                h = S['mesh'].get_socket_location('Head')
                note('shot %s cam %s head %s cam-head %s' % (STEPS[S['i']][0], v(c), v(h), v(c - h)))
                unreal.SystemLibrary.execute_console_command(S['world'], 'HighResShot 1', S['pc'])
                S['phase'], S['pt'] = 2, 0.0
        elif S['pt'] > 0.7:
            S['phase'] = 0
    except Exception as e:
        note('ERROR %r' % (e,))
        unreal.unregister_slate_post_tick_callback(H)

open(OUT, 'w').write('started\n')
H = unreal.register_slate_post_tick_callback(tick)
