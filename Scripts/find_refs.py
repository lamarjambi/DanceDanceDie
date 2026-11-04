import unreal
o=[]
for a in ['/Game/Rhythm/BP_Spline']:
    o.append('%s <- %s'%(a, unreal.EditorAssetLibrary.find_package_referencers_for_asset(a, False)))
open(unreal.Paths.project_dir()+'Saved/refs.txt','w').write('\n'.join(o))
