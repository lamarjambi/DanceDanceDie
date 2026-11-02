import unreal, math
# Rebuilds NPC walk loops. Move the NPC_<Name>_A / _B target points, then run this again (Tools > Execute Python Script).
NPCS=[('Jolleen',180.0,'idle-jolleen-jackie'),('Jackie',180.0,'idle-jolleen-jackie'),('Bobby',150.0,'idle-bobby')]
DEF={'Jolleen':((700,-5400),(700,-7400)),'Jackie':((1700,-7600),(1700,-5600)),'Bobby':((300,-6400),(1500,-6400))}
IDLE=2.5
TURN=0.6
EAS=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
W=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
def find(lab):
  for a in EAS.get_all_level_actors():
    if a.get_actor_label()==lab: return a
  return None
def ground(x,y):
  h=unreal.SystemLibrary.line_trace_single(W,unreal.Vector(x,y,250),unreal.Vector(x,y,-1000),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,[],unreal.DrawDebugTrace.NONE,True)
  return h.to_tuple()[4].z if h else 0.0
def key(ch,t,v):
  ch.add_key(unreal.FrameNumber(int(round(t*30))),float(v),0.0,unreal.SequenceTimeUnit.DISPLAY_RATE,unreal.MovieSceneKeyInterpolation.LINEAR)
for name,speed,idlename in NPCS:
  base='/Game/Characters/NPC/%s/'%name
  walk=unreal.load_asset(base+name+'_Anim_mixamo_com')
  walk.set_editor_property('force_root_lock',True)
  unreal.EditorAssetLibrary.save_loaded_asset(walk)
  idle=unreal.load_asset(base+idlename)
  pts=[]
  for i in range(2):
    lab='NPC_%s_%s'%(name,'AB'[i])
    tp=find(lab)
    if tp is None:
      x,y=DEF[name][i]
      tp=EAS.spawn_actor_from_class(unreal.TargetPoint,unreal.Vector(x,y,ground(x,y)))
      tp.set_actor_label(lab)
    tp.set_folder_path('NPCs')
    pts.append(tp.get_actor_location())
  A,B=pts
  npc=find('NPC_'+name)
  if npc is None:
    npc=EAS.spawn_actor_from_class(unreal.SkeletalMeshActor,A)
    npc.set_actor_label('NPC_'+name)
  npc.set_folder_path('NPCs')
  npc.skeletal_mesh_component.set_skeletal_mesh_asset(unreal.load_asset(base+name))
  sp='/Game/Characters/NPC/Walk/LS_Walk_'+name
  seq=unreal.load_asset(sp) if unreal.EditorAssetLibrary.does_asset_exist(sp) else unreal.AssetToolsHelpers.get_asset_tools().create_asset('LS_Walk_'+name,'/Game/Characters/NPC/Walk',unreal.LevelSequence,unreal.LevelSequenceFactoryNew())
  for b in seq.get_bindings(): b.remove()
  seq.set_display_rate(unreal.FrameRate(30,1))
  leg=math.hypot(B.x-A.x,B.y-A.y)/speed
  h=math.degrees(math.atan2(B.y-A.y,B.x-A.x))-90
  T1=leg; T2=leg+IDLE; T3=2*leg+IDLE; T4=2*leg+2*IDLE
  seq.set_playback_start_seconds(0)
  seq.set_playback_end_seconds(T4)
  bd=seq.add_possessable(npc)
  sec=bd.add_track(unreal.MovieScene3DTransformTrack).add_section()
  sec.set_range_seconds(0,T4)
  ch=sec.get_all_channels()
  for t,p,yaw in [(0,A,h),(T1,B,h),(T1+TURN,B,h+180),(T2,B,h+180),(T3,A,h+180),(T3+TURN,A,h+360),(T4,A,h+360)]:
    key(ch[0],t,p.x); key(ch[1],t,p.y); key(ch[2],t,p.z); key(ch[5],t,yaw)
  at=bd.add_track(unreal.MovieSceneSkeletalAnimationTrack)
  for a,t0,t1 in [(walk,0,T1),(idle,T1,T2),(walk,T2,T3),(idle,T3,T4)]:
    s=at.add_section(); s.set_range_seconds(t0,t1)
    pr=s.get_editor_property('params'); pr.set_editor_property('animation',a); s.set_editor_property('params',pr)
  la=find('NPC_%s_Seq'%name)
  if la is None:
    la=EAS.spawn_actor_from_class(unreal.LevelSequenceActor,A)
    la.set_actor_label('NPC_%s_Seq'%name)
  la.set_folder_path('NPCs')
  la.set_sequence(seq)
  ps=la.get_editor_property('playback_settings')
  ps.set_editor_property('auto_play',True)
  lc=unreal.MovieSceneSequenceLoopCount()
  lc.set_editor_property('value',-1)
  ps.set_editor_property('loop_count',lc)
  la.set_editor_property('playback_settings',ps)
  unreal.EditorAssetLibrary.save_loaded_asset(seq)
  unreal.log('NPCW %s leg=%.1fs A=%s B=%s'%(name,leg,A,B))
