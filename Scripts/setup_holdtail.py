# Creates /Game/Rhythm/Material/M_HoldTail (glowing soft ribbon) and points BP_Note1.Tail at Plane + M_HoldTail.
import unreal
MEL=unreal.MaterialEditingLibrary; EAL=unreal.EditorAssetLibrary
P='/Game/Rhythm/Material/M_HoldTail'
if EAL.does_asset_exist(P):
    mat=EAL.load_asset(P); MEL.delete_all_material_expressions(mat)
else:
    mat=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_HoldTail','/Game/Rhythm/Material',unreal.Material,unreal.MaterialFactoryNew())
mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property('two_sided',True)
E=lambda cls,x,y: MEL.create_material_expression(mat,cls,x,y)
C=MEL.connect_material_expressions
tc=E(unreal.MaterialExpressionTextureCoordinate,-1400,0)
mu=E(unreal.MaterialExpressionComponentMask,-1200,-100); mu.set_editor_property('r',True); C(tc,'',mu,'')
mv=E(unreal.MaterialExpressionComponentMask,-1200,100); mv.set_editor_property('g',True); C(tc,'',mv,'')
def edge(src,x,y):  # 1 at centre, 0 at edges : 1-abs(2v-1)
    m=E(unreal.MaterialExpressionMultiply,x,y); m.set_editor_property('const_b',2.0); C(src,'',m,'A')
    s=E(unreal.MaterialExpressionSubtract,x+150,y); s.set_editor_property('const_b',1.0); C(m,'',s,'A')
    a=E(unreal.MaterialExpressionAbs,x+300,y); C(s,'',a,'')
    o=E(unreal.MaterialExpressionOneMinus,x+450,y); C(a,'',o,'')
    return o
eu=edge(mu,-1000,-100); ev=edge(mv,-1000,100)
# opacity = sat(ev*2.5) * sat(eu*10) * 0.85
ov=E(unreal.MaterialExpressionMultiply,-400,100); ov.set_editor_property('const_b',7.0); C(ev,'',ov,'A')
ovs=E(unreal.MaterialExpressionSaturate,-250,100); C(ov,'',ovs,'')
ou=E(unreal.MaterialExpressionMultiply,-400,-100); ou.set_editor_property('const_b',5.0); C(eu,'',ou,'A')
ous=E(unreal.MaterialExpressionSaturate,-250,-100); C(ou,'',ous,'')
op=E(unreal.MaterialExpressionMultiply,-100,0); C(ovs,'',op,'A'); C(ous,'',op,'B')
op2=E(unreal.MaterialExpressionMultiply,50,0); op2.set_editor_property('const_b',0.95); C(op,'',op2,'A')
# emissive = Color * Glow * (0.9 + 0.35*ev^6)  ~= arrow (Tint*Glow) with a faint core
pw=E(unreal.MaterialExpressionPower,-400,300); pw.set_editor_property('const_exponent',6.0); C(ev,'',pw,'Base')
k=E(unreal.MaterialExpressionMultiply,-250,300); k.set_editor_property('const_b',0.35); C(pw,'',k,'A')
ad=E(unreal.MaterialExpressionAdd,-100,300); ad.set_editor_property('const_b',0.9); C(k,'',ad,'A')
col=E(unreal.MaterialExpressionVectorParameter,-400,500); col.set_editor_property('parameter_name','Color'); col.set_editor_property('default_value',unreal.LinearColor(1,1,1,1))
gl=E(unreal.MaterialExpressionScalarParameter,-400,650); gl.set_editor_property('parameter_name','Glow'); gl.set_editor_property('default_value',1.0)
cg=E(unreal.MaterialExpressionMultiply,-100,550); C(col,'',cg,'A'); C(gl,'',cg,'B')
em=E(unreal.MaterialExpressionMultiply,100,400); C(cg,'',em,'A'); C(ad,'',em,'B')
MEL.connect_material_property(em,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
MEL.connect_material_property(op2,'',unreal.MaterialProperty.MP_OPACITY)
MEL.recompile_material(mat); EAL.save_loaded_asset(mat)
# --- BP_Note1 Tail -> Plane + M_HoldTail
bp=EAL.load_asset('/Game/Rhythm/BP_Note1')
plane=EAL.load_asset('/Engine/BasicShapes/Plane')
ss=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
done=False
for h in ss.k2_gather_subobject_data_for_blueprint(bp):
    d=unreal.SubobjectDataBlueprintFunctionLibrary.get_data(h)
    o=unreal.SubobjectDataBlueprintFunctionLibrary.get_object(d)
    if o and o.get_name().startswith('Tail'):
        o.set_editor_property('static_mesh',plane); o.set_material(0,mat); done=True
        unreal.log_warning('HOLDTAIL tail template %s mesh=%s mat=%s'%(o.get_name(),o.get_editor_property('static_mesh').get_name(),o.get_material(0).get_name()))
if not done: raise RuntimeError('Tail component not found on BP_Note1')
unreal.BlueprintEditorLibrary.compile_blueprint(bp); EAL.save_loaded_asset(bp)
unreal.log_warning('HOLDTAIL setup ok')
