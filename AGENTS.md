# AGENTS.md — DanceDanceDie (UE 5.4)

## Target hardware
- Dev machine = AMD Radeon integrated GPU (shared RAM, ~1 GB texture budget). Treat it as min spec.
- Measure perf in **Standalone**, not PIE. `stat unit` → if GPU ms is highest, it's rendering.

## Rendering conventions (Config/DefaultEngine.ini)
- No Lumen (GI=0), no Virtual Shadow Maps, no Nanite, no mesh distance fields, no AA.
- 3D renders at `r.ScreenPercentage=25` with nearest-neighbor upscale (`r.Upscale.Quality=0`).
  UI (UMG/Slate) still draws at native resolution → stays sharp.
- Don't change the window resolution for the retro look (breaks UI scale + loading screen). Use screen percentage.

## Post process: /Game/PostProcess/M_PS1Horror
- Pixelates to `rows=180` and grades the image. Any custom HLSL that samples SceneTexture must
  convert viewport UV → buffer UV: `(vpUV*View.ViewSizeAndInvSize.xy+View.ViewRectMin.xy)*View.BufferSizeAndInvSize.zw`.
  Otherwise screen % < 100 renders the image into a corner.
- Keep rendered rows (screen height × screen %) ≥ 180.

## Scripts
- Editor Python lives in `Scripts/`. Run from the editor's Python box:
  `exec(open(unreal.Paths.project_dir()+'Scripts/<name>.py').read())`
