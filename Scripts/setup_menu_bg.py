import unreal, math, random
# Live main-menu background. Re-run after editing the params below (Python box):
#   exec(open(unreal.Paths.project_dir()+'Scripts/setup_menu_bg.py').read())
# MainMenu streams the city (NightCityCreator, Always Loaded) so baked lighting is kept.
# Persistent-level actors (Menu_*): camera + sequence (lamp flicker). Footsteps come from the city NPCs (setup_npc_footsteps.py).
# Menu_Cam is only placed on first run; after that the script keeps wherever you moved/piloted it. RESET_CAM=True puts it back.
CAM_LOC=(1040,-3620,110); CAM_PITCH=14.0; CAM_YAW=87.0; CAM_FOV=90.0
EXPOSURE_EV=1.0                          # added to the player camera's exposure bias (menu reads darker than the old screenshot)
RESET_CAM=False
LOOP=25.0                                # flicker sequence length (s)
LAMP_LIGHT_NEAR=(1164,-3286,261)         # the movable Point Light under the street lamp in the screenshot
LAMP_MESH_NEAR=(1240,-3316,3)            # StreetLight_LOD0
SEED=7
CITY='/Game/NightCityCreator/maps/NightCityCreator'
MENU='/Game/UI/MainMenu'
DIR='/Game/UI/MenuBG'
FPS=30
log=lambda s: unreal.log('MENUBG '+s)
def fail(s): raise RuntimeError('MENUBG '+s)

AT=unreal.AssetToolsHelpers.get_asset_tools()
EAL=unreal.EditorAssetLibrary
LES=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
EAS=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
UES=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)

# ---- title texture (title keyed out of T_MainMenuBG, same 1195x622 frame -> same placement in the widget)
src=unreal.Paths.project_dir()+'Scripts/assets/T_MainMenuBG_Title.png'
t=unreal.AssetImportTask(); t.filename=src; t.destination_path='/Game/UI'; t.destination_name='T_MainMenuTitle'
t.replace_existing=True; t.automated=True; t.save=False
AT.import_asset_tasks([t])
tex=unreal.load_asset('/Game/UI/T_MainMenuTitle') or fail('title import failed')
for k,v in [('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI),('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS),
            ('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON),('never_stream',True),('srgb',True)]:
  tex.set_editor_property(k,v)
EAL.save_loaded_asset(tex)

# ---- pawnless game mode (default ThirdPerson mode would drop a character at the origin = inside the city's 2Club trigger)
gmp='/Game/UI/BP_MainMenuGameMode'
if EAL.does_asset_exist(gmp): gm=unreal.load_asset(gmp)
else:
  f=unreal.BlueprintFactory(); f.set_editor_property('parent_class',unreal.GameModeBase)
  gm=AT.create_asset('BP_MainMenuGameMode','/Game/UI',unreal.Blueprint,f)
gmc=unreal.BlueprintEditorLibrary.generated_class(gm)
cdo=unreal.get_default_object(gmc)
cdo.set_editor_property('default_pawn_class',None)
unreal.BlueprintEditorLibrary.compile_blueprint(gm)
EAL.save_loaded_asset(gm)

# ---- level
LES.load_level(MENU)
W=UES.get_editor_world()
if not any(l.get_path_name().startswith(CITY+'.') for l in unreal.EditorLevelUtils.get_levels(W)):
  unreal.EditorLevelUtils.add_level_to_world(W,CITY,unreal.LevelStreamingAlwaysLoaded)
  log('added city sublevel')
LES.set_current_level_by_name('MainMenu') or fail('could not make MainMenu current')
ws=W.get_world_settings(); ws.set_editor_property('default_game_mode',gmc)

acts=EAS.get_all_level_actors()
def mine(label,cls,loc,rot=unreal.Rotator()):
  for a in acts:
    if a.get_actor_label()==label: a.set_actor_location_and_rotation(loc,rot,False,True); return a
  a=EAS.spawn_actor_from_class(cls,loc,rot); a.set_actor_label(label); return a
def nearest(cls,p,maxd=60):
  best=None;bd=maxd
  for a in acts:
    if isinstance(a,cls):
      d=(a.get_actor_location()-unreal.Vector(*p)).length()
      if d<bd: best,bd=a,d
  return best or fail('no %s near %s'%(cls.__name__,p))
def ground(x,y):
  h=unreal.SystemLibrary.line_trace_single(W,unreal.Vector(x,y,250),unreal.Vector(x,y,-1000),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,[],unreal.DrawDebugTrace.NONE,True)
  return h.to_tuple()[4].z if h else fail('no ground at %s,%s'%(x,y))

cam=next((a for a in acts if a.get_actor_label()=='Menu_Cam'),None)
if cam is None or RESET_CAM:
  cam=mine('Menu_Cam',unreal.CameraActor,unreal.Vector(*CAM_LOC),unreal.Rotator(roll=0.0,pitch=CAM_PITCH,yaw=CAM_YAW))
cam.set_editor_property('auto_activate_for_player',unreal.AutoReceiveInput.PLAYER0)
cc=cam.camera_component; cc.set_editor_property('field_of_view',CAM_FOV); cc.set_editor_property('constrain_aspect_ratio',False)
cam.set_folder_path('MenuBG')
# same look as gameplay: copy post process (M_PS1Horror blendable, grading, exposure) + FOV from the player camera
tmp=EAS.spawn_actor_from_class(unreal.load_class(None,'/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter_C'),unreal.Vector(0,0,-100000))
pc=tmp.get_component_by_class(unreal.CameraComponent) or fail('player camera not found')
pp=pc.get_editor_property('post_process_settings')
bias=pp.get_editor_property('auto_exposure_bias') if pp.get_editor_property('override_auto_exposure_bias') else 0.0
pp.set_editor_property('override_auto_exposure_bias',True); pp.set_editor_property('auto_exposure_bias',bias+EXPOSURE_EV)
cc.set_editor_property('post_process_settings',pp)
cc.set_editor_property('post_process_blend_weight',pc.get_editor_property('post_process_blend_weight'))
log('copied player camera PP (weight %.2f, player fov %.1f)'%(pc.get_editor_property('post_process_blend_weight'),pc.get_editor_property('field_of_view')))
tmp.destroy_actor()


for a in acts:
  if a.get_actor_label()=='Menu_Michelle': a.destroy_actor(); log('removed Menu_Michelle')
lamp=nearest(unreal.PointLight,LAMP_LIGHT_NEAR)
lamp_mesh=nearest(unreal.StaticMeshActor,LAMP_MESH_NEAR)
I0=lamp.light_component.intensity
log('lamp light %s I=%.2f, lamp mesh %s'%(lamp.get_path_name(),I0,lamp_mesh.get_path_name()))

# ---- sequence
sp=DIR+'/LS_MenuBG'
seq=unreal.load_asset(sp) if EAL.does_asset_exist(sp) else AT.create_asset('LS_MenuBG',DIR,unreal.LevelSequence,unreal.LevelSequenceFactoryNew())
for b in seq.get_bindings(): b.remove()
for tr in seq.get_tracks(): seq.remove_track(tr)
seq.set_display_rate(unreal.FrameRate(FPS,1))
T4=LOOP
seq.set_playback_start_seconds(0); seq.set_playback_end_seconds(T4)
def fr(t): return unreal.FrameNumber(int(round(t*FPS)))
def key(ch,t,v,interp=unreal.MovieSceneKeyInterpolation.LINEAR):
  ch.add_key(fr(t),float(v),0.0,unreal.MovieSceneTimeUnit.DISPLAY_RATE,interp)


# lamp flicker: steady with irregular bursts of quick drop-outs, one long blackout. CONSTANT keys = hard flicker.
rnd=random.Random(SEED)
CONST=unreal.MovieSceneKeyInterpolation.CONSTANT
pattern=[(0.0,1.0)]
t=rnd.uniform(1.5,3.0)
while t<T4-1.0:
  if rnd.random()<0.2:                      # long outage
    pattern+=[(t,0.0),(t+0.08,0.6),(t+0.14,0.0),(t+rnd.uniform(0.6,1.1),1.0)]
  else:                                      # burst
    tt=t
    for _ in range(rnd.randint(2,6)):
      pattern+=[(tt,rnd.choice([0.0,0.05,0.25])),(tt+rnd.uniform(0.03,0.09),rnd.uniform(0.7,1.15))]
      tt+=rnd.uniform(0.06,0.2)
    pattern+=[(tt,1.0)]
  t=pattern[-1][0]+rnd.uniform(2.0,5.5)
lb=seq.add_possessable(lamp)
lcb=seq.add_possessable(lamp.light_component)
ft=lcb.add_track(unreal.MovieSceneFloatTrack); ft.set_property_name_and_path('Intensity','Intensity')
fs=ft.add_section(); fs.set_range_seconds(0,T4)
fc=fs.get_all_channels()[0]
for tt,v in pattern: key(fc,tt,I0*v,CONST)
key(fc,T4,I0,CONST)

# lamp head emissive follows the light, if MI_Streetlight exposes a scalar param for it
mat=lamp_mesh.static_mesh_component.get_material(0)
params=[str(n) for n in unreal.MaterialEditingLibrary.get_scalar_parameter_names(mat)]
log('MI_Streetlight scalar params: %s'%params)
em=next((p for p in params if 'emiss' in p.lower() or 'glow' in p.lower() or 'intens' in p.lower() or 'bright' in p.lower()),None)
if em:
  e0=unreal.MaterialEditingLibrary.get_material_instance_scalar_parameter_value(mat,em)
  mb=seq.add_possessable(lamp_mesh); mcb=seq.add_possessable(lamp_mesh.static_mesh_component)
  mt=mcb.add_track(unreal.MovieSceneComponentMaterialTrack); mt.set_material_index(0)
  ms=mt.add_section(); ms.set_range_seconds(0,T4)
  pi=unreal.MaterialParameterInfo(); pi.set_editor_property('name',em)
  for tt,v in pattern: ms.add_scalar_parameter_key(pi,fr(tt),e0*max(v,0.02),unreal.Name(''),unreal.Name(''))
  log('emissive param %s base %.2f keyed'%(em,e0))
else:
  log('WARNING no emissive scalar param on lamp material -> only the light flickers')

la=mine('Menu_Seq',unreal.LevelSequenceActor,unreal.Vector(*CAM_LOC))
la.set_folder_path('MenuBG')
la.set_sequence(seq)
ps=la.get_editor_property('playback_settings')
ps.set_editor_property('auto_play',True)
lc=unreal.MovieSceneSequenceLoopCount(); lc.set_editor_property('value',-1)
ps.set_editor_property('loop_count',lc)
la.set_editor_property('playback_settings',ps)

EAL.save_loaded_asset(seq)
LES.save_current_level()
UES.set_level_viewport_camera_info(cam.get_actor_location(),cam.get_actor_rotation())
log('done loop=%.1fs flicker keys=%d cam=%s %s'%(T4,len(pattern),cam.get_actor_location(),cam.get_actor_rotation()))
