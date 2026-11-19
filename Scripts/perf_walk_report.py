# usage: walk_report.py <project root> <label> [<label> ...]   -> fps by stretch of the walk, moving frames only
import csv, re, sys, statistics as st
root = sys.argv[1]
COLS = ['FrameTime', 'GPUTime', 'GPU/Lights', 'GPU/ShadowDepths', 'GPU/ShadowProjection', 'LightCount/All', 'LightCount/Unbatched', 'LightCount/UpdatedShadowMaps', 'View/PosX', 'View/PosY', 'RenderThreadTime', 'GameThreadTime']
BANDS = [(-4800, -3600), (-3600, -2400), (-2400, -1200), (-1200, -300), (-300, 400)]
def load(label):
    t = open('%s/Saved/perf_%s.txt' % (root, label)).read()
    p = re.search(r'csv:\s+(.*)', t).group(1).strip()
    rows = list(csv.reader(open(p, encoding='utf-8', errors='ignore')))
    h = rows[0]
    idx = {c: h.index(c) for c in COLS if c in h}
    out = []
    for r in rows[1:]:
        try:
            out.append({c: float(r[i]) for c, i in idx.items()})
        except Exception:
            pass
    return out
def fps(fr):
    return 1000.0 / (sum(f['FrameTime'] for f in fr) / len(fr))
def worst(fr):
    s = sorted(f['FrameTime'] for f in fr)
    return 1000.0 / s[min(len(s) - 1, int(len(s) * 0.99))]
for label in sys.argv[2:]:
    fr = load(label)
    mv = []
    for a, b in zip(fr, fr[1:]):
        if abs(b['View/PosY'] - a['View/PosY']) + abs(b['View/PosX'] - a['View/PosX']) > 0.5 and b['FrameTime'] < 200:
            mv.append(b)
    if len(mv) < 50:
        print(label, 'no moving frames'); continue
    g = lambda fr, c: st.mean(f.get(c, 0) for f in fr)
    print('=== %s: %d moving frames, %.1f fps avg, 1%% worst %.1f fps' % (label, len(mv), fps(mv), worst(mv)))
    print('  stretch (Y)        fps  worst  gpu ms  lights  shadowed  depth ms  proj ms  light ms  shadowmaps redone/frame')
    for lo, hi in BANDS:
        b = [f for f in mv if lo <= f['View/PosY'] < hi]
        if len(b) < 20:
            continue
        print('  %6d..%-6d %6.1f %6.1f %7.1f %7.0f %9.0f %9.2f %8.2f %9.2f %10.2f' % (lo, hi, fps(b), worst(b), g(b, 'GPUTime'), g(b, 'LightCount/All'), g(b, 'LightCount/Unbatched'), g(b, 'GPU/ShadowDepths'), g(b, 'GPU/ShadowProjection'), g(b, 'GPU/Lights'), g(b, 'LightCount/UpdatedShadowMaps')))
