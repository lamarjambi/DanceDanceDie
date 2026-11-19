# Runs INSIDE a Standalone game of the club (Scripts/party_test.ps1): fakes the win and checks the party without playing the song.
#   - before the win the hidden dancers block nothing
#   - after it each dancer has a solid body (Cliffhanger.cpp), except one the player is standing inside, until the player steps out
#   - the player walking into a dancer is stopped
#   - in the stare every dancer plays an idle; which one is listed (Bobby = the club idle)
# -> Saved/party_test.txt (PASS / FAIL lines) + two HighResShots (party, stare). Changes nothing on disk besides those.
import unreal, math

MAP = '/Game/CyberClub/DemoMap/DemoMap.DemoMap'
INSIDE = 'Party_Bobby_7'    # the player stands inside this one when the party starts
WALK_AT = 'Party_Dameon_8'  # and walks into this one
VIEW = (1430.0, -400.0)     # screenshots are taken from here, looking at INSIDE
DELAY = 6.0                 # secs after the level is up before the fake win
OUT = unreal.Paths.project_dir() + 'Saved/party_test.txt'
S = {'t': 0.0, 'ready': False, 'step': 0, 'log': [], 'fails': 0}
SL = unreal.SystemLibrary

def note(s):
    S['log'].append(s)
    unreal.log('PARTYTEST ' + s)
    open(OUT, 'w').write('\n'.join(S['log']))

def check(ok, s):
    if not ok: S['fails'] += 1
    note(('PASS ' if ok else 'FAIL ') + s)

def name(a):
    return a.get_actor_label() or a.get_name()

def body(a):
    return next((c for c in a.get_components_by_class(unreal.CapsuleComponent) if c.get_name() == 'PartyBody'), None)

def solid(a):
    b = body(a)
    return bool(b) and b.get_collision_enabled() != unreal.CollisionEnabled.NO_COLLISION

def blocked_at(loc, z):
    # a player-sized capsule pushed 1.2 m onto loc at the player's own height: which dancer stops it? (walls and chairs don't count)
    a, b = unreal.Vector(loc.x - 120, loc.y, z), unreal.Vector(loc.x, loc.y, z)
    hits = SL.capsule_trace_multi_by_profile(S['world'], a, b, 30.0, 60.0, 'Pawn', False, [S['pawn']], unreal.DrawDebugTrace.NONE, True) or []
    for h in hits:
        t = h.to_tuple()
        if t[9] in S['dancers']: return '%s.%s' % (name(t[9]), t[10].get_name() if t[10] else None)
    return None

def stand(x, y, yaw=None):
    p = S['pawn']
    p.set_actor_location(unreal.Vector(x, y, p.get_actor_location().z), False, True)
    if yaw is not None: S['pc'].set_control_rotation(unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))

def setup():
    w = unreal.find_object(None, MAP)
    if not w: return False
    pawn = unreal.GameplayStatics.get_player_pawn(w, 0)
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    if not pawn or not pc: return False
    S.update(world=w, pawn=pawn, pc=pc)
    S['dancers'] = sorted(unreal.GameplayStatics.get_all_actors_with_tag(w, 'PartyNPC'), key=name)
    S['by'] = {name(a): a for a in S['dancers']}
    note('%d dancers, pawn %s at %s' % (len(S['dancers']), pawn.get_class().get_name(), pawn.get_actor_location()))
    return True

def tick(dt):
    try:
        if not S['ready']:
            S['ready'] = setup()
            return
        S['t'] += dt
        t, st, pawn = S['t'] - DELAY, S['step'], S['pawn']
        zc = pawn.get_actor_location().z
        if st == 0 and t >= 0:
            check(len(S['dancers']) >= 15 and INSIDE in S['by'] and WALK_AT in S['by'], '%d dancers in the level' % len(S['dancers']))
            hits = {name(a): blocked_at(a.get_actor_location(), zc) for a in S['dancers']}
            check(all(a.get_editor_property('hidden') for a in S['dancers']) and not any(hits.values()), 'before the win: all hidden, nothing blocks at their spots %s' % {k: v for k, v in hits.items() if v})
            l = S['by'][INSIDE].get_actor_location()
            stand(l.x, l.y)
            SL.execute_console_command(S['world'], 'ddd.FakeWin', S['pc'])
            S['step'] = 1
        elif st == 1 and t >= 1.0:
            others = [a for a in S['dancers'] if name(a) != INSIDE]
            check(all(body(a) for a in S['dancers']), 'every dancer got a body')
            check(all(solid(a) for a in others), 'solid: %d of %d others' % (len([a for a in others if solid(a)]), len(others)))
            check(not solid(S['by'][INSIDE]), '%s is not solid while the player stands inside it' % INSIDE)
            bad = []
            for a in others:
                h = blocked_at(body(a).get_world_location(), zc)
                if h != name(a) + '.PartyBody': bad.append((name(a), h))
            check(not bad, 'a player-sized capsule is stopped by each body %s' % bad)
            l = S['by'][WALK_AT].get_actor_location()
            stand(l.x + 150, l.y + 150)
            S['walk_from'] = pawn.get_actor_location()
            S['step'] = 2
        elif st == 2:
            if t < 1.5: return  # a moment for the INSIDE dancer to notice the player left
            if 'ins' not in S:
                S['ins'] = True
                check(solid(S['by'][INSIDE]), '%s turned solid once the player stepped out' % INSIDE)
            b = body(S['by'][WALK_AT]).get_world_location()
            p = pawn.get_actor_location()
            if t < 4.5:
                pawn.add_movement_input(unreal.Vector(b.x - p.x, b.y - p.y, 0).normal(), 1.0, False)
                return
            moved = math.hypot(p.x - S['walk_from'].x, p.y - S['walk_from'].y)
            gap = math.hypot(p.x - b.x, p.y - b.y)
            check(moved > 40 and gap > 32 + pawn.capsule_component.get_scaled_capsule_radius() - 6, 'walked %.0f uu at %s for 3 s and was stopped %.0f uu from its centre (body 32 + player %.0f)' % (moved, WALK_AT, gap, pawn.capsule_component.get_scaled_capsule_radius()))
            l = S['by'][INSIDE].get_actor_location()
            stand(VIEW[0], VIEW[1], math.degrees(math.atan2(l.y - VIEW[1], l.x - VIEW[0])))
            S['step'] = 3
        elif st == 3 and t >= 5.5:
            SL.execute_console_command(S['world'], 'HighResShot 1', S['pc'])
            note('shot party')
            S['f0'] = None
            S['step'] = 4
        elif st == 4 and t < 9.5:    # fps while standing in the party (uncapped by party_test.ps1), after the screenshot hitch
            if t >= 6.5:
                if S['f0'] is None: S['f0'] = [0, 0.0]
                S['f0'][0] += 1; S['f0'][1] += dt
        elif st == 4 and t >= 12.9:  # idle timeout 10 + flicker 1.5: everyone is staring now
            if S.get('f0') and S['f0'][1] > 0: note('party fps %.0f (3 s, standing at the view spot)' % (S['f0'][0] / S['f0'][1]))
            anims = {}
            for a in S['dancers']:
                m = a.get_component_by_class(unreal.SkeletalMeshComponent)
                sn = m.get_anim_instance()  # single node instance: PlayAnimation swapped its asset
                an = sn.get_animation_asset() if sn and hasattr(sn, 'get_animation_asset') else None
                anims.setdefault('%s -> %s' % (m.get_skeletal_mesh_asset().get_name(), an.get_name() if an else None), []).append(name(a))
            for k in sorted(anims): note('stare anim %s x%d' % (k, len(anims[k])))
            check(all('idle' in k.lower() for k in anims) and 'Bobby -> new-bobby-idle' in anims, 'everyone is in an idle, Bobby in new-bobby-idle')
            SL.execute_console_command(S['world'], 'HighResShot 1', S['pc'])
            note('shot stare')
            S['step'] = 5
        elif st == 5 and t >= 13.8:
            note('done: %d failed' % S['fails'])
            unreal.unregister_slate_post_tick_callback(H)
            SL.execute_console_command(S['world'], 'quit', S['pc'])
    except Exception as e:
        import traceback
        note('ERROR %r\n%s' % (e, traceback.format_exc()))
        unreal.unregister_slate_post_tick_callback(H)
        SL.execute_console_command(S['world'], 'quit', S['pc'])

open(OUT, 'w').write('started\n')
H = unreal.register_slate_post_tick_callback(tick)
