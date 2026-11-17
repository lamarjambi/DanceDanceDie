import unreal
# Viewport helpers for the editor Python box: vp(x,y,z,pitch,yaw) moves the level viewport; vpget() logs it.
_UES=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
def vp(x,y,z,pitch=0.0,yaw=0.0):
  _UES.set_level_viewport_camera_info(unreal.Vector(x,y,z),unreal.Rotator(roll=0.0,pitch=pitch,yaw=yaw))
def vpget():
  l,r=_UES.get_level_viewport_camera_info()
  unreal.log('VP loc=(%.0f,%.0f,%.0f) pitch=%.1f yaw=%.1f'%(l.x,l.y,l.z,r.pitch,r.yaw))
  return l,r
