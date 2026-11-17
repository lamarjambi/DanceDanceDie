import unreal
# Concrete footsteps on the city NPC walks (anim notifies on each walk anim -> plays in game AND in the menu, which streams the city).
#   exec(open(unreal.Paths.project_dir()+'Scripts/setup_npc_footsteps.py').read())
# Source: Scripts/assets/footsteps/SW_ConcreteStep_0X.wav (6 steps cut from concrete-footsteps.wav). Each NPC gets its own pair of steps.
# Notify time = heel strike: the moment each foot bone stops sliding forward (speed drops under 15% of its max). Re-running replaces the 'Footsteps' notify track.
NPCS={'Jolleen':(1,2),'Jackie':(3,4),'Bobby':(5,6)}
DST='/Game/Characters/NPC/Walk/Footsteps'
VOLUME=1.0
INNER=150.0; FALLOFF=1800.0     # attenuation (uu): full volume inside INNER, silent at INNER+FALLOFF
FEET=('LeftFoot','RightFoot')
log=lambda s: unreal.log('NPCFS '+s)
def fail(s): raise RuntimeError('NPCFS '+s)
AT=unreal.AssetToolsHelpers.get_asset_tools(); EAL=unreal.EditorAssetLibrary
src=unreal.Paths.project_dir()+'Scripts/assets/footsteps/'

# attenuation
ap=DST+'/ATT_Footstep'
att=unreal.load_asset(ap) if EAL.does_asset_exist(ap) else AT.create_asset('ATT_Footstep',DST,unreal.SoundAttenuation,unreal.SoundAttenuationFactory())
s=att.get_editor_property('attenuation')
s.set_editor_property('attenuation_shape',unreal.AttenuationShape.SPHERE)
s.set_editor_property('attenuation_shape_extents',unreal.Vector(INNER,0,0))
s.set_editor_property('falloff_distance',FALLOFF)
s.set_editor_property('distance_algorithm',unreal.AttenuationDistanceModel.NATURAL_SOUND)
s.set_editor_property('spatialize',True)
att.set_editor_property('attenuation',s); EAL.save_loaded_asset(att)

# waves
waves={}
for i in range(1,7):
  n='SW_ConcreteStep_%02d'%i
  t=unreal.AssetImportTask(); t.filename=src+n+'.wav'; t.destination_path=DST; t.destination_name=n
  t.replace_existing=True; t.automated=True; t.save=False
  AT.import_asset_tasks([t])
  w=unreal.load_asset(DST+'/'+n) or fail('import failed '+n)
  w.set_editor_property('attenuation_settings',att)
  EAL.save_loaded_asset(w); waves[i]=w

opts=unreal.AnimPoseEvaluationOptions()
for name,(a,b) in NPCS.items():
  anim=unreal.load_asset('/Game/Characters/NPC/%s/%s_Anim_mixamo_com'%(name,name)) or fail('no walk anim for '+name)
  L=anim.get_play_length(); N=max(2,int(L*60))
  hits=[]
  poses=[unreal.AnimPoseExtensions.get_anim_pose_at_time(anim,L*k/N,opts) for k in range(N)]
  for foot,wi in zip(FEET,(a,b)):
    P=[unreal.AnimPoseExtensions.get_bone_pose(p,foot,unreal.AnimPoseSpaces.WORLD).translation for p in poses]
    sp=[((P[(k+1)%N].x-P[k].x)**2+(P[(k+1)%N].y-P[k].y)**2)**0.5 for k in range(N)]
    sp[-1]=sp[-2]  # loop seam: root motion jumps back
    thr=0.15*max(sp)
    plants=[k for k in range(N) if sp[k]<thr and sp[k-1]>=thr]
    if len(plants)!=1: fail('%s %s: expected 1 foot plant per cycle, got %s'%(name,foot,plants))
    hits.append((L*plants[0]/N,wi,foot))
  tr='Footsteps'
  if tr in [str(x) for x in unreal.AnimationLibrary.get_animation_notify_track_names(anim)]: unreal.AnimationLibrary.remove_animation_notify_track(anim,tr)
  unreal.AnimationLibrary.add_animation_notify_track(anim,tr)
  for tt,wi,foot in hits:
    n=unreal.AnimationLibrary.add_animation_notify_event(anim,tr,tt,unreal.AnimNotify_PlaySound)
    n.set_editor_property('sound',waves[wi]); n.set_editor_property('volume_multiplier',VOLUME)
  EAL.save_loaded_asset(anim)
  others=[str(e) for e in unreal.AnimationLibrary.get_animation_notify_event_names(anim)]
  log('%s len=%.2f steps=%s all notifies=%s'%(name,L,[(round(t,2),f,'step%d'%w) for t,w,f in hits],others))
log('done')
