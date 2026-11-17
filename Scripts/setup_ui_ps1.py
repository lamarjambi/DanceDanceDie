import unreal
# PS1 look for UMG: /Game/PostProcess/M_UI_PS1 = Retainer Box effect material (pixelate + 5-bit colour w/ ordered dither + RGB split + rare row glitch).
# Use: wrap widgets in a Retainer Box, Effect Material = M_UI_PS1 (Texture Parameter = "Texture"). Tweak the scalar params on the material (or an MI).
#   exec(open(unreal.Paths.project_dir()+'Scripts/setup_ui_ps1.py').read())
P='/Game/PostProcess'; N='M_UI_PS1'
AT=unreal.AssetToolsHelpers.get_asset_tools(); EAL=unreal.EditorAssetLibrary; MEL=unreal.MaterialEditingLibrary
m=unreal.load_asset(P+'/'+N) if EAL.does_asset_exist(P+'/'+N) else AT.create_asset(N,P,unreal.Material,unreal.MaterialFactoryNew())
# wipe old nodes. delete_all_material_expressions misses some in 5.4 (stale unconnected masks broke the compile), so also
# find the subobjects by name and delete them explicitly.
MEL.delete_all_material_expressions(m)
for cls in ['TextureObjectParameter','TextureCoordinate','Time','ScalarParameter','Custom','ComponentMask']:
  for i in range(64):
    e=unreal.find_object(None,'%s:MaterialExpression%s_%d'%(m.get_path_name(),cls,i))
    if e: MEL.delete_material_expression(m,e)
if MEL.get_num_material_expressions(m): raise RuntimeError('UIPS1 could not clear old nodes: %d left'%MEL.get_num_material_expressions(m))
m.set_editor_property('material_domain',unreal.MaterialDomain.MD_UI)
m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
def node(cls,x,y): return MEL.create_material_expression(m,cls,x,y)
tex=node(unreal.MaterialExpressionTextureObjectParameter,-900,0)
tex.set_editor_property('parameter_name','Texture')
tex.set_editor_property('texture',unreal.load_asset('/Engine/EngineResources/DefaultTexture'))
uv=node(unreal.MaterialExpressionTextureCoordinate,-900,200)
tm=node(unreal.MaterialExpressionTime,-900,300)
params={'Gain':1.35,'BlockPx':4.0,'Levels':31.0,'Dither':1.0,'Split':0.6,'Glitch':0.35}   # BlockPx: ~screen height/180 (4 @720p, 6 @1080p)
pn={}
for i,(k,v) in enumerate(params.items()):
  s=node(unreal.MaterialExpressionScalarParameter,-900,400+i*90); s.set_editor_property('parameter_name',k); s.set_editor_property('default_value',v); pn[k]=s
c=node(unreal.MaterialExpressionCustom,-400,0)
c.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT4)
c.set_editor_property('description','PS1 UI')
names=['Tex','UV','T']+list(params)
cis=[]
for n in names:
  ci=unreal.CustomInput(); ci.set_editor_property('input_name',n); cis.append(ci)
c.set_editor_property('inputs',cis)
c.set_editor_property('code',r'''
float w,h; Tex.GetDimensions(w,h);
float2 sz=float2(w,h);
float B=max(BlockPx,1.0);
float2 blk=floor(UV*sz/B);
// rare horizontal glitch: a few block-rows slide sideways for a moment
float tq=floor(T*12.0);
float r=frac(sin(dot(float2(blk.y,tq),float2(12.9898,78.233)))*43758.5453);
blk.x+= (r>1.0-0.04*Glitch) ? floor((r-0.5)*8.0) : 0.0;
float2 q=(blk+0.5)*B/sz;
float2 o=float2(Split*B/sz.x,0);
float4 cc=Texture2DSample(Tex,TexSampler,q);
float rr=Texture2DSample(Tex,TexSampler,q+o).r;
float bb=Texture2DSample(Tex,TexSampler,q-o).b;
float3 col=float3(rr,cc.g,bb);
col*=Gain;   // through the retainer the text comes out darker; 1.35 matches the original colour (measured from screenshots)
// 4x4 bayer dither then quantize (PS1 15-bit colour)
int2 p=int2(fmod(blk,4.0));
const float bay[16]={0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5};
float d=(bay[p.y*4+p.x]/16.0-0.5)*Dither/Levels;
col=floor(saturate(col+d)*Levels+0.5)/Levels;
float a=max(cc.a,max(Texture2DSample(Tex,TexSampler,q+o).a,Texture2DSample(Tex,TexSampler,q-o).a));
a=step(0.5,a);   // hard edges, no AA
return float4(col,a);
''')
def link(a,ao,b,bi):
  if not MEL.connect_material_expressions(a,ao,b,bi): raise RuntimeError('UIPS1 link failed %s.%s -> %s.%s'%(a.get_name(),ao,b.get_name(),bi))
link(tex,'',c,'Tex')
link(uv,'',c,'UV')
link(tm,'',c,'T')
for k,sp in pn.items(): link(sp,'',c,k)
rgb=node(unreal.MaterialExpressionComponentMask,-150,0); rgb.set_editor_property('r',True); rgb.set_editor_property('g',True); rgb.set_editor_property('b',True)
al=node(unreal.MaterialExpressionComponentMask,-150,150); al.set_editor_property('a',True)
link(c,'',rgb,''); link(c,'',al,'')
MEL.connect_material_property(rgb,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
MEL.connect_material_property(al,'',unreal.MaterialProperty.MP_OPACITY)
MEL.recompile_material(m)
if MEL.get_num_material_expressions(m)!=12: raise RuntimeError('UIPS1 expected 12 nodes, got %d'%MEL.get_num_material_expressions(m))
EAL.save_loaded_asset(m)
unreal.log('UIPS1 done')
