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
- Photo message = `Photo` texture + empty Body. Shown in a fixed 320px-wide box (UImage desired-size override is ignored).
- History is a UScrollBox (`Scroll`) — nothing is evicted. New msgs auto-follow unless the player scrolled up (>40px from end).
  `TopSpacer` is resized each Step so short history hugs the bottom. Scroll input is polled in the game mode Tick
  (wheel, Up/Down, W/S, PgUp/PgDn, dpad, left stick) since input mode is GameOnly.
- After the last msg the chat stays in EndHold (all bubbles full bright) until Space → Skip() → FadeOut → NextLevel.
  `EndHoldTime` = delay before the hint changes to "press space to continue". Space mid-animation still skips.
- Widgets are found by name via WidgetTree->FindWidget so cpp-only changes work with Live Coding (Ctrl+Alt+F11). `Weight`/`LeaveAge` are dead (header untouched).
- Chat photos live in /Game/UI/Images; run `Scripts/setup_chat_photos.py` after importing (160px, nearest, no mips, UI group).
- Names use one `NameColor` (no per-sender rainbow); creepy msgs stay red.
- NEVER commit (user rule). Build with `Scripts/build_only.bat` (no git). Don't create Saved/commit_msg.txt.
- C++ header changes: run `Scripts/rebuild.bat` (waits for editor close, builds, commits with Saved/commit_msg.txt, reopens).

## NPC talk (Source/DanceDanceDie/NPCTalk.*)
- One `NPC_<Name>_Talk` (ANPCTalker) per city NPC, placed/updated by `Scripts/setup_npc_talk.py` (edit LINES there, re-run).
- E icon (`/Game/UI/Images/keyboard_e`) is screen-space, anchored to bone `HeadTop_End`. Mixamo bones import WITHOUT the
  `mixamorig:` prefix (Head, Hips, HeadTop_End). Mixamo walks aren't in-place, so the body drifts from the actor origin: use bones, not actor location.
- Lines go into WBP_Subtitle (first TextBlock). While an NPC line is up, other WBP_Subtitle instances (trigger text) are collapsed, then restored.

## Rhythm: hold notes ("slides") — BP_Spline / BP_Note1
- Note lane is random; timing comes from MetaSound `SCP Cue`. No per-note chart data yet.
- Hold chance/length live on BP_Spline spawn: `RandomBoolWithWeight.Weight` (0.25) → `SelectFloat.A` (1.0 s) → BP_Note1 `HoldDuration` (Expose on Spawn). 0 = tap.
- Press (TryHit) on a hold note → `StartHold` (IsHeld, timer HoldDuration → `HoldComplete` = Good Hit). IA_LaneX `Completed` → `ReleaseLane(X)` → `ReleaseHold`:
  ≤0.15 s left = Good Hit, earlier = RegisterMiss. Destroyer ignores IsHeld notes.
- Tail = `Tail` Plane + `M_HoldTail` (translucent unlit ribbon: params Color, Glow; made by `Scripts/setup_holdtail.py`). Drawn on Tick in the arrow's plane
  (rot = MakeRotFromXZ(spline dir, Arrow up)); length = remaining hold s × SplineLength / TimeToComplete.
- Hold visuals: tail + arrow are always the LANE colour (hard-coded MakeArray in BP_Note1 Tick, order Left/Down/Up/Right = MI_* Tint).
  Holding = brighter: Glow 1.0 → 2.5 on both (arrow M_Icon Glow default is 1.0; tail matches Tint×Glow so they read as one piece).
  StartHold stops the Timeline → head pins at the target, tail shrinks into it. Tail width 0.22 (MakeVector Y), pushed 4uu away from camera
  so the masked arrow hides the join (translucent tail is depth-tested).

## Editing Blueprints from text (T3D)
- Graph copy/paste is plain text. `Scripts/clip_t3d.py` puts `Scripts/t3d/<file>` on the clipboard:
  `py exec(open(unreal.Paths.project_dir()+'Scripts/clip_t3d.py').read()); clip('<file>')` → Ctrl+V in the graph → File ▸ Refresh All Nodes → Compile.
- Whole-graph replace (Ctrl+A, Delete, paste) shows "Fix Self Context Function References" → keep **Do Nothing**.
- ⚠ Deleting/re-pasting **Add Component** nodes (e.g. BP_Spline's two Add Audio Component) creates NEW blank templates → Sound=None → no music/cues.
  After any paste, re-set them: music = `SCP-x5x` (Sound Wave), cue = `MSS_SCP-x5x` (MetaSound). Better: never delete those nodes.
- Verify playtests from `Saved/Logs/DanceDanceDie_2.log` (standalone): expect `cue!!` lines.
- `Scripts/t3d/spline_backup.t3d` = BP_Spline Event Graph before hold notes.

## Demo ending / cliffhanger (Source/DanceDanceDie/Cliffhanger.*)
- All C++, no Blueprint wiring. `Cliffhanger` actor (ACliffhangerDirector) in DemoMap detects the win by watching the actors tagged
  `PartyNPC` go from hidden to visible (BP_Spline does that). Keep that tag + "start hidden" or the ending never fires (it errors on screen).
- Party: spawns one ANPCTalker per dancer at runtime; `ANPCTalker::LineProvider` (C++ delegate) feeds Lines[0..2] in order, whoever is asked.
- Last line, or `IdleTimeout` (10 s) without an E press, starts the flicker (`FlickerTime`, `FlickerCount`): `FlickerStartSound` (suspension-sfx) plays,
  every AudioComponent that was playing winds down (pitch + volume), the screen blacks out via manual camera fade with `FlickerTickSound` each time.
  In the LAST blackout each dancer plays the idle from `IdleAnims` that matches its skeleton and turns to the camera (mesh right vector = facing).
  No matching idle = on-screen error + frozen dance pose. Then lights back = stare for `StareHold` (optional `StareSound`), move input ignored, look still works.
- Then hard cut (`OpenLevel`) to `/Game/UI/DemoEnd`, whose World Settings game mode is `DemoEndGameMode`: black + "it's not over" (VT323) -> MainMenu.
  `DemoEnd` is in LoadingScreen `SkipMaps` (Config/DefaultGame.ini) so the cut has no loading screen.
- Edit lines, timings and sounds on the `Cliffhanger` actor in DemoMap (Details panel, no rebuild). `Scripts/setup_cliffhanger.py` only seeds
  them when it first creates the actor; re-running it refreshes `IdleAnims` (one idle per dancer skeleton, found by "idle" in the asset name
  under /Game/Characters/NPC; skeletons without one get Bobby's idle retargeted via RTG_Bobby_to_<Name>) and DemoEnd's game mode.
- Test without playing the song: PIE in DemoMap, console `ddd.FakeWin`. Log category `LogCliffhanger`.
- `Scripts/run_build.py` = start build_only.bat from the editor Python box, then close the editor.
