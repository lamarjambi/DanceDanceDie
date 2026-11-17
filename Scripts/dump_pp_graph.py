# Dumps M_PS1Horror graph: every expression, its key props, and what feeds each input -> Saved/pp_graph.txt
import unreal
PATH='/Game/PostProcess/M_PS1Horror'
mat=unreal.load_asset(PATH); MEL=unreal.MaterialEditingLibrary
o=[]
def nm(e): return e.get_name() if e else None
for i in range(0,400):
    for cls in ['MaterialExpressionCustom','MaterialExpressionSceneTexture','MaterialExpressionScreenPosition','MaterialExpressionTextureCoordinate','MaterialExpressionComponentMask','MaterialExpressionAppendVector','MaterialExpressionMultiply','MaterialExpressionAdd','MaterialExpressionConstant','MaterialExpressionViewSize']:
        e=unreal.find_object(None,'%s.M_PS1Horror:%s_%d'%(PATH,cls,i))
        if not e: continue
        line=nm(e)
        for p in ['description','scene_texture_id','code']:
            try: line+=' | %s=%s'%(p,e.get_editor_property(p))
            except Exception: pass
        ins=MEL.get_inputs_for_material_expression(mat,e)
        try: names=MEL.get_material_expression_input_names(e)
        except Exception: names=[str(k) for k in range(len(ins))]
        line+=' | inputs: '+', '.join('%s<-%s'%(n,nm(x)) for n,x in zip(names,ins))
        o.append(line)
for p in [unreal.MaterialProperty.MP_EMISSIVE_COLOR]:
    o.append('EMISSIVE <- %s'%nm(MEL.get_material_property_input_node(mat,p)))
open(unreal.Paths.project_dir()+'Saved/pp_graph.txt','w').write('\n'.join(o))
unreal.log('PPGRAPH ok')
