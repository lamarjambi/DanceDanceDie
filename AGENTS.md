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
- EVERY SceneTexture in M_PS1Horror must take PixelUV2 as UV (Col AND Stencil). The CustomStencil node (arrows) had no UV →
  full-res mask vs 180-row colour → arrows drew twice (dim graded + bright boosted copy). Fixed by Scripts/fix_pp_stencil_uv.py.
  Inspect the graph: Scripts/dump_pp_graph.py → Saved/pp_graph.txt.

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
- NEVER commit (user rule). Agents working through a mounted/remote folder: don't run ANY git command there (even `git status` leaves .git/index.lock when deletes are blocked). Build with `Scripts/build_only.bat` (no git). Don't create Saved/commit_msg.txt.
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

- Note facing: BP_Spline SpawnActor(BP_Note1) Spawn Transform Rotation <- GetActorRotation(self). Notes used to spawn at world rot 0
  while BP_Spline (DemoMap) is yaw -40 → moving arrows 40° off the Bullseye targets. Lane geometry dump: Scripts/dump_lanes.py → Saved/lanes_dump.txt.
  Sizes: note Arrow scale 0.6, Bullseye 0.70875.

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
- Then hard cut (`OpenLevel`) to `/Game/UI/DemoEnd`, whose World Settings game mode is `/Game/UI/BP_DemoEndGameMode` (child of C++ `DemoEndGameMode`): black + "it's not over" (VT323) -> MainMenu.
  End-screen text, font size, colour, delay and hold are that Blueprint's Class Defaults (Demo End category), no rebuild.
  `DemoEnd` is in LoadingScreen `SkipMaps` (Config/DefaultGame.ini) so the cut has no loading screen.
- Edit lines, timings and sounds on the `Cliffhanger` actor in DemoMap (Details panel, no rebuild). `Scripts/setup_cliffhanger.py` only seeds
  them when it first creates the actor; re-running it refreshes `IdleAnims` (one idle per dancer skeleton, found by "idle" in the asset name
  under /Game/Characters/NPC; skeletons without one get Bobby's idle retargeted via RTG_Bobby_to_<Name>) and DemoEnd's game mode.
- Test without playing the song: PIE in DemoMap, console `ddd.FakeWin`. Log category `LogCliffhanger`.
- `Scripts/run_build.py` = start build_only.bat from the editor Python box, then close the editor.

## Main menu live background (Scripts/setup_menu_bg.py)
- MainMenu streams `NightCityCreator` as an **Always Loaded** sublevel (keeps the city's baked lighting; copying actors would lose lightmaps).
  Menu-only actors live in the persistent level, folder `MenuBG`: `Menu_Cam` (auto-activates for Player 0), `Menu_Seq` (`/Game/UI/MenuBG/LS_MenuBG`, loops, lamp flicker only).
- No Michelle in the menu (user choice). Menu footsteps come from the city NPCs (see NPC footsteps).
- Camera angle: pilot `Menu_Cam` in MainMenu (Outliner > right-click > Pilot, fly, Eject, save). The script keeps the camera where it is; `RESET_CAM=True` restores the default.
- Loading screen now also shows at boot (LoadingScreen.cpp) so the menu's city never warms up in front of the player.
- MainMenu game mode = `/Game/UI/BP_MainMenuGameMode` (no pawn). The default ThirdPerson mode would spawn a character at the origin = inside the city's `2Club` trigger.
- Flicker = city lamp's movable `Point Light (1)` intensity + `EmissiveValue` on `StreetLight_LOD0` (bound across into the sublevel). Seeded random (`SEED`).
- `Menu_Cam` copies the player camera's post process (M_PS1Horror etc.) + `EXPOSURE_EV` (+1). Re-run the script after changing the player camera PP.
- Title = `T_MainMenuTitle` (keyed out of the old `T_MainMenuBG`, same 1195x622 frame so the widget layout is unchanged). Source PNG in `Scripts/assets/`.
- `Scripts/vp.py`: `vp(x,y,z,pitch,yaw)` moves the editor viewport, `vpget()` logs it. `Scripts/dump_map.py` (set `MAP=` first) dumps a level's actors.

## NPC footsteps (Scripts/setup_npc_footsteps.py)
- `PlaySound` notifies on a `Footsteps` notify track of each NPC walk anim (`<Name>_Anim_mixamo_com`), so they play wherever the walk plays (city + menu).
- Sounds: `/Game/Characters/NPC/Walk/Footsteps/SW_ConcreteStep_01..06` (cut from the user's concrete-footsteps.wav; sources in `Scripts/assets/footsteps/`),
  each NPC gets its own pair. 3D via `ATT_Footstep` (`INNER`/`FALLOFF` in the script).
- Notify time = heel strike, computed from the foot bones (moment the foot stops sliding). New NPC → add it to `NPCS` and re-run.

## PS1 look on UI (Scripts/setup_ui_ps1.py)
- `/Game/PostProcess/M_UI_PS1` = Retainer Box effect material: pixelate (`BlockPx`, ~screen height/180), 5-bit colour + ordered dither, RGB split, rare row glitch.
- Used on WBP_MainMenu: `RetainerBox_0` wraps `Menu_VerticalBox` (PLAY/QUIT). Wrap any other widget the same way to get the look.
- `Gain` 1.35: text comes out darker through the retainer otherwise. User is happy with the colour as is, don't re-tune unasked.
- Script rebuilds the material in place. `delete_all_material_expressions` leaves orphan nodes in 5.4 (they broke the compile), so it also deletes them by name.

## Credits (Source/DanceDanceDie/Credits.*, Scripts/setup_credits.py)
- `CreditsScene` (/Game/UI) game mode = `/Game/UI/BP_CreditsGameMode` (child of C++ `CreditsGameMode`). Built in code like DemoEnd, no WBP.
- Lines, fonts, sizes, colours, speed, timings: BP_CreditsGameMode > Class Defaults > Credits (no rebuild). Line Style: Title / Heading / Name / Gap.
  Last line stops centred (`bStopOnLastLine`), holds `EndHold`, fades, opens `NextLevel` (MainMenu).
- Whole roll sits in a URetainerBox with `M_UI_PS1` (same look as PLAY/QUIT). Text under ~40 px gets eaten by the pixelation (headings = 44).
- Input (polled in UCreditsWidget::NativeTick): Esc / Enter / click / pad B = fade out + go on. In PIE Esc stops play, so test skipping with Enter/click. Hold Space / Down / S / pad A = x`FastMultiplier`.
- Menu: `Credits_Button` / `Credits_Text` in WBP_MainMenu (between PLAY and QUIT), same hover font swap; click = glitch sound, 0.8 s delay, OpenLevel CreditsScene.
  Graph nodes came from `Scripts/t3d/credits_menu.t3d`.
- `CreditsScene` is in LoadingScreen `SkipMaps`. setup_credits.py re-creates the BP if missing and re-sets the level's game mode.
- Credits colours = exact PLAY/QUIT values (text 0.25,0.9,0.08; shadow 0.45,0.02,0.42; offset 5). User approved this look; don't re-tune unasked.
  `Scripts/credits_match_menu.py` re-applies them to BP_CreditsGameMode (colours only, leaves the Lines alone).
- Demo end: `Cliffhanger` actor (DemoMap) has `bRollCredits` (default on) + `CreditsFrom` (= BP_CreditsGameMode). After the stare it rolls the
  credits as an overlay right in DemoMap (same widget, lines/look from CreditsFrom's Class Defaults = one list for menu + demo), then OpenLevel(EndLevel = DemoEnd).
  Flow: stare -> credits (glitch out) -> "it's not over" (white, then red + glitch together) -> MainMenu. No extra level.
  Demo-end credits are UNSKIPPABLE (FParams.bPlayerInput=false: no skip, no fast-forward). Menu credits keep skip + fast-forward.
- Glitch burst = `GlitchFX.h` (FGlitchFX Start/Frame) on a URetainerBox with M_UI_PS1: spikes Glitch/Split/BlockPx/Dither params, jitter, dropped frames,
  glitch-click at 0/40/75%. DemoEnd only puts the effect material on when the glitch starts, so the line is plain until then.
- Rebuild gotcha: closing the editor (File > Exit or quit_editor) pops up the Unreal Project Browser, which blocks build_only.bat
  (it waits for every UnrealEditor.exe). Computer-use clicks don't reach it: ask the user to press Cancel.
