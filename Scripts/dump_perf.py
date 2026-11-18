import unreal, collections
# Perf audit of the OPEN level (or MAP=... to load one first) -> Saved/perf_dump_<map>.txt
# Lights: mobility / shadows / radius. Meshes: instances, triangles, LODs, cull distance. Read-only, saves nothing.
# Optional SPLIT_Y=<y> : also counts things on each side of that world Y (city: club door is at y~416).
EAS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
UES = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
world = None
if globals().get('MAP'):
    world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)  # works headless too (Scripts/dump_perf_cmd.bat)
world = world or UES.get_editor_world()
name = world.get_name()
SPLIT = globals().get('SPLIT_Y')

def prop(o, n, d=None):
    try:
        return o.get_editor_property(n)
    except Exception:
        return d

def mob(c):
    return str(prop(c, 'mobility', '?')).split('.')[-1].replace('>', '').split(':')[0]

out = []
lights = []
light_cnt = collections.Counter()
mesh_inst = collections.Counter()
mesh_info = {}
mesh_flags = collections.Counter()
skel = []
side = collections.Counter()
fog = []
acts = EAS.get_all_level_actors()
for a in acts:
    lab = a.get_actor_label()
    for c in a.get_components_by_class(unreal.LightComponentBase):
        cn = c.get_class().get_name().replace('Component', '')
        l = c.get_world_location()
        m = mob(c)
        sh = bool(prop(c, 'cast_shadows', False))
        vis = bool(c.is_visible()) and not a.is_hidden_ed()
        rad = prop(c, 'attenuation_radius', 0) or 0
        light_cnt['%-18s %-10s shadows=%-5s visible=%s' % (cn, m, sh, vis)] += 1
        lights.append((m, sh, cn, lab, l, rad, prop(c, 'intensity', 0), vis))
        if SPLIT is not None and m != 'STATIC':
            side['dynamic lights %s split' % ('past' if l.y > SPLIT else 'before')] += 1
    for c in a.get_components_by_class(unreal.StaticMeshComponent):
        sm = prop(c, 'static_mesh')
        if not sm:
            continue
        p = sm.get_path_name().split('.')[0]
        n = 1
        if isinstance(c, unreal.InstancedStaticMeshComponent):
            n = c.get_instance_count()
            mesh_flags['instanced components'] += 1
        mesh_inst[p] += n
        if p not in mesh_info:
            try:
                mesh_info[p] = (sm.get_num_triangles(0), sm.get_num_lods())
            except Exception:
                mesh_info[p] = (0, 0)
        mesh_flags['mobility ' + mob(c)] += 1
        if prop(c, 'cast_shadow', False):
            mesh_flags['cast shadow'] += 1
        if (prop(c, 'ld_max_draw_distance', 0) or 0) > 0:
            mesh_flags['has max draw distance'] += 1
        if SPLIT is not None:
            side['mesh components %s split' % ('past' if c.get_world_location().y > SPLIT else 'before')] += n
    for c in a.get_components_by_class(unreal.SkeletalMeshComponent):
        sk = None
        try:
            sk = c.get_skeletal_mesh_asset()
        except Exception:
            sk = prop(c, 'skeletal_mesh')
        if not sk:
            continue
        lods = verts = -1
        try:
            lods = unreal.EditorSkeletalMeshLibrary.get_lod_count(sk)
            verts = unreal.EditorSkeletalMeshLibrary.get_num_verts(sk, 0)
        except Exception:
            pass
        skel.append('%-28s %-30s verts=%-7d lods=%d hiddenInGame=%s' % (lab, sk.get_name(), verts, lods, prop(a, 'hidden')))
    for c in a.get_components_by_class(unreal.ExponentialHeightFogComponent):
        fog.append('%s volumetric=%s density=%s' % (lab, prop(c, 'volumetric_fog'), prop(c, 'fog_density')))

out.append('LEVEL %s : %d actors' % (name, len(acts)))
out.append('==== lights by type / mobility (STATIC = baked, free at runtime)')
for k, v in sorted(light_cnt.items()):
    out.append('%5d  %s' % (v, k))
out.append('==== NON-static lights (these cost frame time)  mobility | shadows | class | label | loc | radius | intensity | visible')
for m, sh, cn, lab, l, rad, inten, vis in sorted(lights, key=lambda x: (x[0], not x[1], x[3])):
    if m != 'STATIC':
        out.append('%-10s %-5s %-16s %-34s (%.0f,%.0f,%.0f) r=%.0f i=%.1f vis=%s' % (m, sh, cn, lab, l.x, l.y, l.z, rad, inten or 0, vis))
out.append('==== static mesh flags')
for k, v in sorted(mesh_flags.items()):
    out.append('%5d  %s' % (v, k))
tot_inst = sum(mesh_inst.values())
tot_tris = sum(mesh_inst[p] * mesh_info[p][0] for p in mesh_inst)
out.append('==== static meshes: %d unique, %d instances, %d triangles if all drawn at LOD0' % (len(mesh_inst), tot_inst, tot_tris))
out.append('  inst   tris/each  total tris  lods  asset   (top 60 by total triangles)')
for p in sorted(mesh_inst, key=lambda p: -mesh_inst[p] * mesh_info[p][0])[:60]:
    t, lo = mesh_info[p]
    out.append('%6d %10d %11d %5d  %s' % (mesh_inst[p], t, t * mesh_inst[p], lo, p))
out.append('  meshes with only 1 LOD: %d of %d' % (sum(1 for p in mesh_info if mesh_info[p][1] <= 1), len(mesh_info)))
out.append('==== skeletal meshes')
out += skel
out.append('==== fog')
out += fog
if SPLIT is not None:
    out.append('==== split at y=%s' % SPLIT)
    for k, v in sorted(side.items()):
        out.append('%5d  %s' % (v, k))
dst = unreal.Paths.project_dir() + 'Saved/perf_dump_' + name + '.txt'
open(dst, 'w').write('\n'.join(out))
unreal.log('PERF dump %s: %d actors, %d lights -> %s' % (name, len(acts), len(lights), dst))
