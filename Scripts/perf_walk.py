# Runs INSIDE a Standalone perf capture (see AGENTS.md > Hardware + perf):
#   perf_capture.ps1 -Label walk -Extra '-ExecCmds="DisableAllScreenMessages,py <full path, forward slashes>/Scripts/perf_walk.py"'
#   python Scripts/perf_walk_report.py . walk
# Changes nothing on disk except Saved/walk_info.txt.
# Walks the player from the city spawn to the club door and back, forever, so the CSV shows fps while moving.
# Env: DDD_WALK_SPEED (uu/s, default 450), DDD_WALK_DELAY (s before the first step, default 6),
#      DDD_NOSHADOW = rule for lights that lose Cast Shadows for this run only (see pick()), default none.
import unreal, os, math

MAP = '/Game/NightCityCreator/maps/NightCityCreator.NightCityCreator'
PATH = [(1215, -4737), (800, -4200), (800, -600), (690, 10), (500, 250)]
Z = 96.0
SPEED = float(os.environ.get('DDD_WALK_SPEED', '450'))
DELAY = float(os.environ.get('DDD_WALK_DELAY', '6'))
RULE = os.environ.get('DDD_NOSHADOW', '')
STAND = os.environ.get('DDD_STAND', '')   # "x,y,yaw" = stand there instead of walking (look checks)
OUT = unreal.Paths.project_dir() + 'Saved/walk_info.txt'

S = {'t': 0.0, 'd': 0.0, 'dir': 1, 'ready': False, 'world': None, 'pawn': None, 'pc': None, 'log': []}
seg = [math.dist(PATH[i], PATH[i + 1]) for i in range(len(PATH) - 1)]
TOTAL = sum(seg)

def note(s):
    S['log'].append(s)
    unreal.log('WALK ' + s)
    open(OUT, 'w').write('\n'.join(S['log']))

def pick(rule, loc, comp):
    # which lights lose their shadow for this run; a+b = either rule
    if not rule:
        return False
    if '+' in rule:
        return any(pick(r, loc, comp) for r in rule.split('+'))
    if rule == 'all':
        return True
    if rule.startswith('zmin'):      # lights mounted high: zmin600 = z > 600
        return loc.z > float(rule[4:])
    if rule.startswith('zmax'):
        return loc.z < float(rule[4:])
    if rule.startswith('imax'):      # dim lights: imax4 = intensity < 4
        return comp.get_editor_property('intensity') < float(rule[4:])
    return False

def setup():
    w = unreal.find_object(None, MAP)
    if not w:
        return False
    pawn = unreal.GameplayStatics.get_player_pawn(w, 0)
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    if not pawn or not pc:
        return False
    S['world'], S['pawn'], S['pc'] = w, pawn, pc
    l = pawn.get_actor_location()
    note('pawn %s at (%.0f,%.0f,%.0f) path %.0f uu speed %.0f rule [%s]' % (pawn.get_class().get_name(), l.x, l.y, l.z, TOTAL, SPEED, RULE))
    unreal.SystemLibrary.execute_console_command(w, 'ghost', pc)
    n = off = 0
    for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.Light):
        c = a.get_component_by_class(unreal.LocalLightComponent)
        if not c:
            continue
        n += 1
        if c.get_editor_property('cast_shadows') and pick(RULE, c.get_world_location(), c):
            c.set_cast_shadows(False)
            off += 1
    note('point/spot lights %d, shadows turned off on %d' % (n, off))
    return True

def at(d):
    for i, L in enumerate(seg):
        if d <= L or i == len(seg) - 1:
            f = min(1.0, d / L)
            a, b = PATH[i], PATH[i + 1]
            return a[0] + (b[0] - a[0]) * f, a[1] + (b[1] - a[1]) * f, math.degrees(math.atan2(b[1] - a[1], b[0] - a[0]))
        d -= L

def tick(dt):
    try:
        if not S['ready']:
            S['ready'] = setup()
            return
        S['t'] += dt
        if S['t'] < DELAY:
            d = 0.0
        else:
            S['d'] += S['dir'] * SPEED * dt
            if S['d'] >= TOTAL:
                S['d'], S['dir'] = TOTAL, -1
            elif S['d'] <= 0:
                S['d'], S['dir'] = 0.0, 1
            d = S['d']
        x, y, yaw = at(d)
        if S['dir'] < 0:
            yaw += 180.0
        if STAND:
            x, y, yaw = [float(v) for v in STAND.split(',')]
        S['pawn'].set_actor_location(unreal.Vector(x, y, Z), False, True)
        S['pc'].set_control_rotation(unreal.Rotator(0.0, 0.0, yaw))  # roll, pitch, yaw
    except Exception as e:
        note('ERROR %r' % (e,))
        unreal.unregister_slate_post_tick_callback(H)

open(OUT, 'w').write('started\n')
H = unreal.register_slate_post_tick_callback(tick)
