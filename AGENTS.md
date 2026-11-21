# AGENTS.md — DanceDanceDie (UE 5.4)

## Hard rules
- NEVER commit (user rule). On a mounted/remote folder run no git command at all (even `git status` leaves `.git/index.lock`). Don't create `Saved/commit_msg.txt`.
- Build with `Scripts/build_only.bat` (no git). `Scripts/run_build.py` starts it from the editor Python box, then closes the editor.
  C++ header changes: `Scripts/rebuild.bat` (waits for editor close, builds, `git add`, commits only if `Saved/commit_msg.txt` exists, reopens). It runs git: not on a mounted folder.
- Closing the editor pops up the Unreal Project Browser, which blocks the build (it waits for every UnrealEditor.exe). Computer-use clicks don't reach it: ask the user to press Cancel.
  No browser when everything is saved and the editor is closed with PowerShell `(Get-Process -Id <pid>).CloseMainWindow()`; then run `Scripts\build_only.bat`.
- Computer use: the editor's app name is `UnrealEditor.exe`. `type` pastes the clipboard, and a crashed headless UE run leaves its callstack there:
  `Set-Clipboard` the command, then ctrl+v in the Cmd box.
- The picture must look identical after any perf/rendering change: compare screenshots first. Anything that changes the look: ask first.
- Approved looks, don't re-tune unasked: `M_UI_PS1` `Gain` 1.35, credits colours.
- User preference: change what already exists (Blueprint graphs, asset properties, Details panel values) before writing new code.
  A new C++ class or a kept script only when the editor route can't do it: say why first.

## Design
- GDD (source of truth for story, songs, scope): https://docs.google.com/document/d/1tTtxZwsyKF8KON_052Dtuo76aP4wTwPmAEXqh4nasIY
  Read it before design-facing work (dialogue, credits, level flow, new mechanics). Public page: https://playlamar.dev/games/dance-dance-die

## Hardware + perf
- Dev machine = AMD Radeon integrated GPU (~1 GB texture budget) = min spec. The game is GPU-bound. Measure in **Standalone**, not PIE.
- `Scripts/perf_capture.bat -Map <map> -Label <name> [-CVars "r.X=1,..."] [-Extra "-dx11"]` → `Saved/perf_<name>.txt` + `.png`.
  Fixed screen % test: `-CVars "ddd.RenderRows=0,r.ScreenPercentage=50"`. Runs drift ~15% (heat): compare back to back.
  Capture at a spot: `-Extra '-ExecCmds="DisableAllScreenMessages,BugItGo x y z pitch yaw roll"'` (club door `700 0 142 0 140 0`,
  street toward the bar `800 -1500 142 0 90 0`). `-ShotAt` must land before the run ends (load ~23 s) or there is no png.
  One capture at a time: a stopped background batch can keep launching runs. Minimise the editor (open behind the game = -20% fps)
  and close Chrome (a playing tab took 19% of the GPU: walking test 51 -> 31 fps). Check `\GPU Engine(*)` counters before measuring.
  Walking test (spawn -> club door and back, fps while moving): add `py <full path>/Scripts/perf_walk.py` to the ExecCmds, then
  `python Scripts/perf_walk_report.py . <label>`. `$env:DDD_NOSHADOW` (`all`, `imax4`, `zmin600`, `a+b`) drops Cast Shadows on a
  light group for that run only; `$env:DDD_STAND="x,y,yaw"` stands still for look shots.
- Level audit without the editor: `Scripts/dump_perf_cmd.bat` → `Saved/perf_dump_<map>.txt`.
- Nothing sets `sg.*`; the cheap overrides are in `Config/DefaultEngine.ini` `[SystemSettings]` (`t.MaxFPS=60`). Keep DX12 (DX11: 26 vs 40 fps).
- City: all 146 point/spot lights + the directional are movable with shadows. Cost = lights near the camera.
  Fps follows the lights on screen: spawn 27 (~63 fps), club door 51 (~49), street toward the bar 94, 74 shadowed (~46).
  Shadows = 1/3 of the frame at the door (all shadows off: 47 -> 70 fps).
  Tested, no gain, don't redo: no shadows on far lights, culling small far lights, 1 CSM cascade, hiding the far half,
  `r.Shadow.WholeSceneShadowCacheMb` 100 (shadow maps are already cached).
  Failed the look check: `ddd.RenderRows=360` (+15% fps, but pixels change everywhere and more lights drop their shadows).
  Walking, idle laptop: 57 fps now, 64 without shadows on the 65 dim lights (intensity < 4; small light leaks on walls, not applied,
  user decides), 70 with no shadows at all. Lights above z 600: no gain.
- 1080p uncapped: city ~63, club ~78, menu ~50 fps (worst case; more = fewer shadowed lights near the camera = changes the look).

## Rendering
- No Lumen, Virtual Shadow Maps, Nanite or mesh distance fields. AA = FXAA.
- Render scale is adaptive (`Source/DanceDanceDie/AdaptiveRes.*`): `r.ScreenPercentage` = `ddd.RenderRows` (432) / viewport height, nearest upscale.
  Don't set `r.ScreenPercentage` anywhere else (the ini's 50 is only the fallback for `ddd.RenderRows 0`).
- Retro look = screen %, never window resolution (breaks UI scale + loading screen).
- Keep rendered rows ≥ ~2x the post process's 180 rows. Fewer = choppy blocks, worst in the city.
- Near clip = 2 cm (`[/Script/Engine.Engine] NearClipPlane`, read at startup; live: `r.SetNearClipPlane 2`). At the default 10 the camera
  cuts the player's own chest open when looking straight down (hollow body). Normal view is unchanged.
- Player = `BP_ThirdPersonCharacter` (Michelle): `CameraBoom` (arm length 0) on mesh socket `Head` + 15, `Head` bone hidden at BeginPlay.
  Dump: `Scripts/dump_player.py` (headless) → `Saved/player_dump.txt`. Look-down shots in Standalone:
  `Scripts/look_test.ps1 -Label x [-At "x,y,yaw"] [-Walk] [-Steps "label:pitch:fwd:up:nearclip;..."]` → `Saved/look_<label>_<step>.png`.

## Post process `/Game/PostProcess/M_PS1Horror`
- Pixelates to 180 rows + grades. EVERY SceneTexture (colour AND CustomStencil) must take `PixelUV2` as UV; it returns `ViewportUVToSceneTextureUV(vp, 14)`.
  Never use View.* math (world and UI stop lining up below 100%) or leave a UV unplugged (arrows draw twice).
- Graph dump: `Scripts/dump_pp_graph.py` → `Saved/pp_graph.txt`. Past fixes: `Scripts/fix_pp_*.py`.

## Scripts
- Editor Python lives in `Scripts/`: `exec(open(unreal.Paths.project_dir()+'Scripts/<name>.py').read())`
- `Scripts/` = only what gets re-run (setups, tests, build). A one-off (single edit, quick dump, check of today's change) runs from a
  temp folder outside the project (`py exec(open('<full path>').read())`) and leaves no file behind.
- `vp.py`: `vp(x,y,z,pitch,yaw)` moves the editor viewport, `vpget()` logs it. `dump_map.py` (set `MAP=` first) dumps a level's actors.
- Standalone playtest log: `Saved/Logs/DanceDanceDie_2.log`.

## Blueprints from text (T3D)
- `py exec(open(unreal.Paths.project_dir()+'Scripts/clip_t3d.py').read()); clip('<file>')` puts `Scripts/t3d/<file>` on the clipboard
  → Ctrl+V in the graph → File ▸ Refresh All Nodes → Compile.
- Whole-graph replace asks "Fix Self Context Function References" → keep **Do Nothing**.
- ⚠ Never delete/re-paste **Add Component** nodes (BP_Spline's two Add Audio Component): new blank templates = Sound None = no music/cues.
  If it happens: music = `SCP-x5x` (Sound Wave), cue = `MSS_SCP-x5x` (MetaSound).
- `Scripts/t3d/spline_backup.t3d` = BP_Spline Event Graph before hold notes.

## Rhythm (BP_Spline / BP_Note1)
- Note lane is random; timing comes from MetaSound `SCP Cue` (log shows `cue!!`). No chart data yet.
- StartSong starts the cue MetaSound ~3.75 s BEFORE the music (note lead-in): a `Delay` node → music + `SongTimer`.
  StopSong does NOT cancel that Delay, and a pending Delay ignores the next StartSong. SongHUD cancels it after its own StopSong
  (`RemoveActionsForObject`, log `cancelled 1 pending delay(s)`); without that the first try's music starts 2.7 s early.
- Hold notes: BP_Spline spawn `RandomBoolWithWeight.Weight` (0.25) → `SelectFloat.A` (1.0 s) → BP_Note1 `HoldDuration` (0 = tap).
  TryHit → `StartHold` (timer → `HoldComplete` = Good Hit). IA_LaneX `Completed` → `ReleaseLane(X)` → `ReleaseHold`: ≤0.15 s left = Good Hit, earlier = RegisterMiss.
  Destroyer ignores IsHeld notes.
- Tail = `Tail` plane + `M_HoldTail` (params Color, Glow; `Scripts/setup_holdtail.py`), drawn on Tick, length = remaining s × SplineLength / TimeToComplete,
  pushed 4uu from the camera so the masked arrow hides the join. Tail + arrow = lane colour (MakeArray in BP_Note1 Tick, order Left/Down/Up/Right), Glow 1.0 → 2.5 while held.
- Notes spawn with BP_Spline's actor rotation (DemoMap spline is yaw -40). Arrow scale 0.6, Bullseye 0.70875. Lane dump: `Scripts/dump_lanes.py` → `Saved/lanes_dump.txt`.
- Unattended playtests fail ~2 s into the music (misses → Jumpscare → restart loop). Expected.

## Song HUD (`SongHUD.*`, `Scripts/setup_song_hud.py`) — all C++
- `SongHUD` actor in DemoMap reads BP_Spline `CueAudio` / `MusicAudio` by reflection. "Rhythm part started" = CueAudio playing.
- Flow: cue starts → HUD calls `StopSong` → title card → `StartSong` → time + bar follow BP_Spline `SongTimer` (45 s), NOT the whole track.
  ≥ 95% of the timer gone = complete. `bHoldSongForIntro` off = don't touch the song. `SongLength` > 0 = fixed length instead.
- Fail/jumpscare: HUD hides on the early stop, comes back at 0:00 when BP_Spline restarts (cue plays again, 3.5 s later), no 2nd intro.
  Don't wait for SongTimer to bring it back: it only starts ~3.7 s after the restart (title gone while notes are already coming).
  Don't key off BP_Jumpscare `ScareActive`/visibility (on for the whole rhythm part).
- Tuning: SongHUD actor Details > Song HUD (no rebuild). Test: PIE console `ddd.SongHUD [secs]`.

## Intro chat (`IntroChat.*`)
- Messages are C++ constructor defaults, inherited by BP_IntroChatGameMode. Dump BP values: `Scripts/dump_introchat.py` → `Saved/introchat_dump.txt`.
- Widgets are found by name (`WidgetTree->FindWidget`), so cpp-only changes work with Live Coding (Ctrl+Alt+F11). `Weight`/`LeaveAge` are dead.
- Photo message = `Photo` texture + empty Body, fixed 320 px box. Photos live in /Game/UI/Images; run `Scripts/setup_chat_photos.py` after importing.
- History = UScrollBox `Scroll`, auto-follows unless the player scrolled up. Scroll input is polled in the game mode Tick (input mode is GameOnly).
  Follow = `ScrollToEnd` every frame (`bFollow`), not once per message: wrapped text gets its 2nd line a frame late, so a one-off left the newest bubble cut off.
- After the last message: EndHold until Space → Skip() → FadeOut → NextLevel.
- One `NameColor` for every name; creepy messages stay red.
- Look = the profile screen's: square outlined bubbles, header + messages in URetainerBox `ChatRetainer` with `M_UI_PS1`, so all
  text is ≥ 40 px. The shader cuts alpha at 0.5: `DimOpacity` under 0.5 makes old messages vanish.
- Calm (`Glitch` 0) until the creepy texts. Each one fires a burst and raises the steady glitch (`Steady[]` in Step); after the
  third it stays glitchy to the end (user choice).

## Profile screen (`ProfileSetup.*`) — all C++, shown by the IntroChat game mode before the chat
- Character select that reads as a text invite (user choice, not a fight-game select): invite = message bubble, picked character =
  the chat's typing bubble over their head, name = message bar with JOIN CHAT as its send button. Font = VT323 only (no Rubik here).
- Everything goes through `M_UI_PS1` (text under ~40 px gets eaten; the small controls hint is outside it on purpose).
  Colours, glitch: `BP_IntroChatGameMode` Class Defaults > Profile (no rebuild).
- Calm until something is hovered (user choice): the material's `Glitch` param (default 0.35 = rows tearing all the time) is set
  to 0 every tick, and to `HoverGlitch` only on the piece under the mouse. The material glitches a whole retainer, so the still
  parts, each character, the name field and the button each have their own, side by side. Never nest them (pixelated + brightened
  twice). Small retainers must take `BlockPx` from the full-screen one (their own height gives tiny blocks).
- JOIN CHAT keeps its glitch burst into the chat (user choice).
- Widgets that overlap go in a UOverlay (one layer per child). Siblings of a box share a layer and their draw order is not fixed:
  the opaque character picture covered the bubble.
- Characters = live scene captures (unlit base colour, black background, no alpha). The backdrop is black so no box shows around
  them: any other backdrop colour brings the box back.
- Same `CameraHeight` / `CameraDistance` for every character = true relative heights. The bubble height comes from the mesh bounds.
- The idle runs only for the picked character and the one under the mouse.
- No "MICHELLE" / "ABE" on screen and no pre-filled name (user choice: players make up their own). The field starts empty; only a
  name the player typed before comes back. Joining with it empty still falls back to the character's name (the chat needs one).
- `UImage::SetDesiredSizeOverride` is dropped when called before the slate widget exists (NativeOnInitialized): set the brush `ImageSize`.
- `MaterialEditingLibrary.delete_all_material_expressions` crashes in a commandlet (`!IsRooted`): run material setup scripts in the editor.

## NPC talk (`NPCTalk.*`) + footsteps
- One `NPC_<Name>_Talk` (ANPCTalker) per city NPC: edit `LINES` in `Scripts/setup_npc_talk.py`, re-run.
- E icon is anchored to bone `HeadTop_End`. Mixamo bones have NO `mixamorig:` prefix, and the walks aren't in-place: use bones, not actor location.
- Lines show in WBP_Subtitle (first TextBlock); other WBP_Subtitle instances are collapsed while a line is up.
- Footsteps (`Scripts/setup_npc_footsteps.py`): PlaySound notifies on the `Footsteps` track of each `<Name>_Anim_mixamo_com`, timed at heel strike from the foot bones.
  Sounds `SW_ConcreteStep_01..06` in /Game/Characters/NPC/Walk/Footsteps, attenuation `ATT_Footstep`. New NPC → add to `NPCS`, re-run.

## Demo ending (`Cliffhanger.*`) — all C++
- `Cliffhanger` actor in DemoMap fires when the actors tagged `PartyNPC` go from hidden to visible (BP_Spline does that).
  Keep the tag + "start hidden" or the ending never fires.
- Flow: party lines (one ANPCTalker per dancer, fed by `LineProvider`) → last line or `IdleTimeout` → flicker → dancers idle + face the camera → stare
  → credits overlay in DemoMap (`bRollCredits`, `CreditsFrom`, UNSKIPPABLE) → OpenLevel `/Game/UI/DemoEnd` ("it's not over") → MainMenu.
- Lines, timings, sounds: Details panel on the actor (no rebuild). DemoEnd text/timing: `/Game/UI/BP_DemoEndGameMode` Class Defaults.
- `Scripts/setup_cliffhanger.py` seeds values only when it first creates the actor; re-running refreshes `IdleAnims` (one idle per dancer skeleton,
  missing = on-screen error) and DemoEnd's game mode.
- Test: PIE in DemoMap, console `ddd.FakeWin`. Log `LogCliffhanger`.

## Club dancers (`Scripts/setup_party.py`)
- 15 `Party_<Name>_<n>` SkeletalMeshActors in DemoMap (folder `PartyNPCs`, tag `PartyNPC`, hidden, collision OFF). 8 hand-placed + `EXTRA` in the script
  (label, character, dance, x, y, yaw): edit it, re-run in the editor. Not headless: `duplicate_actor` and `duplicate_and_retarget` crash in a commandlet.
- Collision stays off in the level (hidden dancers = invisible walls). `Cliffhanger.cpp` gives each one a pawn-only capsule `PartyBody` on the `Hips` bone
  when the party starts; it turns solid once the player is not standing inside it.
- Bobby's club idle = `new-bobby-idle` (Cliffhanger Idle Anims; `CLUB_IDLES` in `setup_cliffhanger.py` keeps it). The city keeps `idle-bobby` (`LS_Walk_Bobby`).
- `duplicate_and_retarget` drops its copies in `/Game/`: the script moves them to NPC/Dance.
- Room = x -870..2330, y -1340..660, floor z 20. Dancer z is hand-tuned per character (Bobby 32, Jackie ~37, Jolleen ~28, Dameon 19, Hal 30).
- Test: `Scripts/party_test.ps1 -Label x` (Standalone: fake win, walks into a dancer, stare idles, fps) → `Saved/party_test.txt` + `Saved/party_<x>_*.png`.

## Credits (`Credits.*`, `Scripts/setup_credits.py`)
- `/Game/UI/CreditsScene`, game mode `BP_CreditsGameMode` (child of C++ `CreditsGameMode`), built in code, no WBP.
  Lines, fonts, colours, timings: its Class Defaults > Credits (no rebuild). One list for menu + demo end.
- The roll sits in a URetainerBox with `M_UI_PS1`: text under ~40 px gets eaten by the pixelation.
- Menu credits: Esc / Enter / click / pad B = skip, hold Space / Down / S / pad A = fast. In PIE Esc stops play, so test skip with Enter/click.
- Colours = exact PLAY/QUIT values; `Scripts/credits_match_menu.py` re-applies them (colours only).
- `GlitchFX.h` (FGlitchFX): glitch burst on a retainer with `M_UI_PS1`; `FitBlocks` sets `BlockPx` = screen height / `PixelRows` (270). WBP_MainMenu keeps fixed BlockPx 4.
- Menu button `Credits_Button` in WBP_MainMenu; its graph nodes came from `Scripts/t3d/credits_menu.t3d`.

## Main menu + UI
- Background (`Scripts/setup_menu_bg.py`): MainMenu streams `NightCityCreator` as an **Always Loaded** sublevel (keeps the baked lighting).
  Menu-only actors: folder `MenuBG` (`Menu_Cam`, `Menu_Seq` = `/Game/UI/MenuBG/LS_MenuBG`, lamp flicker). No Michelle in the menu (user choice).
- Game mode `/Game/UI/BP_MainMenuGameMode` (no pawn: a default pawn spawns at the origin = inside the city's `2Club` trigger).
- Camera: pilot `Menu_Cam`, save; the script keeps it (`RESET_CAM=True` restores the default). `Menu_Cam` copies the player camera's post process
  (+ `EXPOSURE_EV`): re-run the script after changing that.
- PS1 look on UI (`Scripts/setup_ui_ps1.py`): `/Game/PostProcess/M_UI_PS1` as a Retainer Box effect material (WBP_MainMenu `RetainerBox_0`); wrap any widget the same way.
  The script rebuilds the material in place and deletes orphan nodes by name (`delete_all_material_expressions` leaves them in 5.4).
- Loading screen also shows at boot. `DemoEnd` and `CreditsScene` are in LoadingScreen `SkipMaps` (`Config/DefaultGame.ini`).
