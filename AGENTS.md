# AGENTS.md — DanceDanceDie (UE 5.4)

## Target hardware
- Dev machine = AMD Radeon integrated GPU (shared RAM, ~1 GB texture budget). Treat it as min spec.
- Measure perf in **Standalone**, not PIE. `stat unit` → if GPU ms is highest, it's rendering.

## Rendering conventions (Config/DefaultEngine.ini)
- No Lumen (GI=0), no Virtual Shadow Maps, no Nanite, no mesh distance fields. AA = FXAA (cheap; reduces shimmer in the city).
- 3D renders at `r.ScreenPercentage=60` with nearest-neighbor upscale (`r.Upscale.Quality=0`).
  UI (UMG/Slate) still draws at native resolution → stays sharp.
- Don't change the window resolution for the retro look (breaks UI scale + loading screen). Use screen percentage.

## Post process: /Game/PostProcess/M_PS1Horror
- Pixelates to `rows=180` and grades the image. Any custom HLSL that samples SceneTexture must
  convert viewport UV → buffer UV: `(vpUV*View.ViewSizeAndInvSize.xy+View.ViewRectMin.xy)*View.BufferSizeAndInvSize.zw`.
  Otherwise screen % < 100 renders the image into a corner.
- Keep rendered rows (screen height × screen %) ≥ 180.
- PixelUV2 now returns `ViewportUVToSceneTextureUV(vp, 14)` (Scripts/fix_pp_viewportuv.py). The old View.* math zoomed the
  image 1/0.6 from the top-left in PIE at 60%, so world and UI (e.g. NPC E prompt) didn't line up. Don't go back to View.* math.

## Scripts
- Editor Python lives in `Scripts/`. Run from the editor's Python box:
  `exec(open(unreal.Paths.project_dir()+'Scripts/<name>.py').read())`
- Rendered rows should be ~2x the effect's 180 rows or more. Fewer (e.g. 25% → ~233 rows) = uneven, choppy blocks,
  worst in the city (thin windows/signs/reflections). Club looks fine at any %.

## Intro chat (Source/DanceDanceDie/IntroChat.*)
- Messages are C++ constructor defaults; BP_IntroChatGameMode inherits them. Dump current BP values: `Scripts/dump_introchat.py` → Saved/introchat_dump.txt.
- Photo message = `Photo` texture + empty Body. Shown in a fixed 320px-wide box (UImage desired-size override is ignored). Photos weigh 4 toward MaxBubbles.
- Chat photos live in /Game/UI/Images; run `Scripts/setup_chat_photos.py` after importing (160px, nearest, no mips, UI group).
- Names use one `NameColor` (no per-sender rainbow); creepy msgs stay red.
- C++ header changes: run `Scripts/rebuild.bat` (waits for editor close, builds, commits with Saved/commit_msg.txt, reopens).

## NPC talk (Source/DanceDanceDie/NPCTalk.*)
- One `NPC_<Name>_Talk` (ANPCTalker) per city NPC, placed/updated by `Scripts/setup_npc_talk.py` (edit LINES there, re-run).
- E icon (`/Game/UI/Images/keyboard_e`) is screen-space, anchored to bone `HeadTop_End`. Mixamo bones import WITHOUT the
  `mixamorig:` prefix (Head, Hips, HeadTop_End). Mixamo walks aren't in-place, so the body drifts from the actor origin: use bones, not actor location.
- Lines go into WBP_Subtitle (first TextBlock). While an NPC line is up, other WBP_Subtitle instances (trigger text) are collapsed, then restored.
