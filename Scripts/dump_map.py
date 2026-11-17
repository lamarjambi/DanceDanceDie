import unreal, collections
# Opens MAP (set MAP=... before exec; default DemoMap) and dumps actors -> Saved/map_dump_<map>.txt
LES=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
MAP=globals().get('MAP','/Game/CyberClub/DemoMap/DemoMap'); LES.load_level(MAP)
EAS=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
acts=EAS.get_all_level_actors()
out=[]; cnt=collections.Counter()
for a in acts:
  c=a.get_class(); cn=c.get_name(); cnt[cn+' | '+c.get_path_name()]+=1
for k,v in cnt.most_common(): out.append('%5d %s'%(v,k))
out.append('---- details')
for a in acts:
  cn=a.get_class().get_name(); lab=a.get_actor_label()
  if any(s in cn for s in ['Camera','PlayerStart','Character','Pawn','Spline','Cliffhanger','NPCTalk','Trigger','Sequence','PostProcess','Fog','Sky','Directional','Volume']) or any(s in lab.lower() for s in ['lamp','street','light','cam','michelle','player']):
    l=a.get_actor_location(); r=a.get_actor_rotation()
    out.append('%s | %s | %s | (%.0f,%.0f,%.0f) rot(%.1f,%.1f,%.1f)'%(lab,cn,a.get_folder_path(),l.x,l.y,l.z,r.pitch,r.yaw,r.roll))
open(unreal.Paths.project_dir()+'Saved/map_dump_'+MAP.split('/')[-1]+'.txt','w').write('\n'.join(out))
unreal.log('DEMOMAP dump %d actors'%len(acts))
