import bpy, bmesh, math, json, importlib.util, ast
from pathlib import Path
from mathutils import Vector
P=Path(__file__).with_name('author_kit.py');s=P.read_text(encoding='utf-8-sig')
s=s.replace('phase=t;stride=.25;lift=.11;rootz=-.29*crouch','phase=t;stride=.60;lift=.11;rootz=-.29*crouch')
s=s.replace("gait=name in ['Walk','Run','Crouch_Walk','Walk_Start','Walk_Stop','Turn_L90','Turn_R90']\n    amp=1","gait=name in ['Walk','Run','Crouch_Walk','Walk_Start','Walk_Stop','Turn_L90','Turn_R90']\n    stance=.38 if name=='Run' else .62\n    amp=1")
s=s.replace("if name=='Run':stride=.43;lift=.22;rootz+=-.08+.035*cos(4*pi*t)","if name=='Run':stride=.70;lift=.22;rootz+=-.125+.035*cos(4*pi*t)")
s=s.replace('elif gait:rootz+=-.035+.013*cos(4*pi*t)','elif gait:rootz+=-.070+.013*cos(4*pi*t)')
s=s.replace("if p<.62:fy=(-stride/2+stride*p/.62)","if p<stance:fy=(-stride/2+stride*p/stance)")
s=s.replace("u=(p-.62)/.38;fy=stride/2-stride*ease(u)","u=(p-stance)/(1-stance);fy=stride/2-stride*ease(u)")
s=s.replace("if run:rootz+=h*(-.10+.04*cos(4*pi*t))","if run:rootz+=h*(-.10+.04*cos(4*pi*t))-(h*.03 if r['species']=='Tiger' else 0)")
s=s.replace("if cat:\n        # Separate polygon ribbons","if False and cat:\n        # Historical ribbon construction; the final asset uses a painted surface.")
s=s.replace("    add('Body',p,'BODY')","    body_module=add('Body',p,'BODY')\n    if cat:add_tiger_texture(body_module,cfg)")
s=s.replace("box('Peak',(0,-.107,1.773),(.16,.13,.013),'olive_dark','head',c,bevel=.008)","box('Peak',(0,-.107,1.773),(.16,.13,.013),'olive_dark','head',c,bevel=.003)")
tiger_function="""
def add_tiger_texture(o,cfg):
    n=256;image=bpy.data.images.get('TEX_Tiger_Stripes')
    if image is None:
        image=bpy.data.images.new('TEX_Tiger_Stripes',n,n,alpha=False);pixels=[]
        base=[int(PALETTE['tiger'][0][i:i+2],16)/255 for i in (1,3,5)]
        dark=[int(PALETTE['stripe'][0][i:i+2],16)/255 for i in (1,3,5)]
        cream=[int(PALETTE['fur_light'][0][i:i+2],16)/255 for i in (1,3,5)]
        for yy in range(n):
            v=yy/n
            for xx in range(n):
                u=xx/n
                phase=u*9+.18*sin(v*2*pi*1.6)+.07*sin(v*2*pi*4+u*6)
                stripe=cos(2*pi*phase)>(.86+.05*sin(v*11+u*9))
                color=dark if stripe and v>.23 else base
                if v<.30:
                    f=max(0,min(1,(.30-v)/.12));color=[color[i]*(1-f)+cream[i]*f for i in range(3)]
                pixels.extend([*[linear(c) for c in color],1])
        image.pixels.foreach_set(pixels);image.filepath_raw=str(ROOT/'Materials/Textures/Tiger_Stripes_BaseColor.png');image.file_format='PNG';image.save();image.pack()
    material=bpy.data.materials.get('MAT_Tiger_StripedFur')
    if material is None:
        material=bpy.data.materials.new('MAT_Tiger_StripedFur');material.use_nodes=True
        shader=material.node_tree.nodes.get('Principled BSDF');shader.inputs['Roughness'].default_value=.94
        tex=material.node_tree.nodes.new('ShaderNodeTexImage');tex.image=image;tex.extension='EXTEND';material.node_tree.links.new(tex.outputs['Color'],shader.inputs['Base Color'])
    for i,m in enumerate(o.data.materials):
        if m and m.name.split('.')[0] in ['MAT_tiger','MAT_Tiger_StripedFur']:o.data.materials[i]=material
    uv=o.data.uv_layers.active
    for loop in o.data.loops:
        co=o.data.vertices[loop.vertex_index].co
        uv.data[loop.index].uv=(.5+co.y/(cfg['length']*1.12),(co.z-cfg['height']*.42)/(cfg['height']*.85))
    o['fur_surface']='Painted stripes, embedded GLB image. No floating stripe geometry.'
"""
if 'def add_tiger_texture' not in s:s=s.replace('\nANIMALS={','\n'+tiger_function+'\nANIMALS={')
s=s.replace("'human_walk':1.35,'human_run':3.8,'human_crouch_walk':.65,'tiger_walk':1.2,'tiger_run':5.5,'wolf_walk':1.1,'wolf_run':4.8,'fox_walk':.8,'fox_run':3.5,'deer_walk':1.25,'deer_run':5.8","'human_walk':.968,'human_run':2.303,'human_crouch_walk':.194,'tiger_walk':.738,'tiger_run':2.786,'wolf_walk':.601,'wolf_run':2.266,'fox_walk':.389,'fox_run':1.467,'deer_walk':.581,'deer_run':2.191")
P.write_text(s,encoding='utf8')
spec=importlib.util.spec_from_file_location('author_final',P);a=importlib.util.module_from_spec(spec);spec.loader.exec_module(a);ROOT=a.ROOT
manifest_file=ROOT/'Documentation/asset_manifest.json';manifest=json.loads(manifest_file.read_text(encoding='utf8'))
# Reuse only helper definitions, without executing the earlier finalization pass.
tree=ast.parse(Path(__file__).with_name('finalize_animations.py').read_text(encoding='utf-8-sig'))
helpers=[n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name in ['motion_sheet','contacts']]
namespace={'a':a,'bpy':bpy,'Vector':Vector};exec(compile(ast.Module(body=helpers,type_ignores=[]),'<preview_helpers>','exec'),namespace)
contact_rows=json.loads((ROOT/'Documentation/contact_checks.json').read_text(encoding='utf8'))
for kind,relative in [('human','Characters/Human_Modular_Library.blend'),('Tiger','Animals/Tiger_Modular.blend')]:
    bpy.ops.wm.open_mainfile(filepath=str(ROOT/relative));a.MATS={key:bpy.data.materials.get('MAT_'+key) for key in a.PALETTE if bpy.data.materials.get('MAT_'+key)}
    r=next(o for o in bpy.context.scene.objects if o.type=='ARMATURE')
    objects=[o for o in bpy.context.scene.objects if o.type=='MESH' and o.parent==r]
    if kind=='human':
        cap=next(o for o in objects if o.name=='Accessory_Cap')
        bm=bmesh.new();bm.from_mesh(cap.data);bad=[f for f in bm.faces if f.calc_area()<1e-11]
        if bad:bmesh.ops.delete(bm,geom=bad,context='FACES')
        bm.to_mesh(cap.data);bm.free();cap.data.update()
        a.export('Characters/Modules/Accessory_Cap.glb',[r,cap],False)
    else:
        body=next(o for o in objects if o.name=='Tiger_Body')
        bm=bmesh.new();bm.from_mesh(body.data);slots=[i for i,m in enumerate(body.data.materials) if m and m.name.startswith('MAT_stripe')]
        bad=[f for f in bm.faces if f.material_index in slots]
        if bad:bmesh.ops.delete(bm,geom=bad,context='FACES')
        orphan=[v for v in bm.verts if not v.link_faces]
        if orphan:bmesh.ops.delete(bm,geom=orphan,context='VERTS')
        bm.to_mesh(body.data);bm.free();body.data.update();a.add_tiger_texture(body,a.ANIMALS['Tiger'])
        a.export('Animals/Tiger_Modules/Body.glb',[r,body],False)
    if r.animation_data:r.animation_data.action=None
    for action in list(bpy.data.actions):bpy.data.actions.remove(action)
    a.animate(r,r['rig_contract'],a.human_sampler if kind=='human' else a.animal_sampler)
    manifest['animations'][r['rig_contract']]=a.MANIFEST['animations'][r['rig_contract']]
    if kind=='human':
        presets=json.loads((ROOT/'Documentation/human_presets.json').read_text(encoding='utf8'))
        for name,parts in presets['presets'].items():
            chosen=[o for o in objects if o.name in presets['common']+parts];a.export('Characters/Presets/'+name+'.glb',[r]+chosen)
        names=presets['common']+['Head_A','Face_Calm','Hair_Crop','Torso_FieldJacket','Sleeve_FieldJacket_L','Sleeve_FieldJacket_R','Accessory_TacticalVest']
        chosen=[o for o in objects if o.name in names]
        for o in objects:o.hide_set(o not in chosen);o.hide_render=o not in chosen
    else:
        chosen=objects;a.export('Animals/Tiger.glb',[r]+chosen)
        a.preview('Previews/Tiger.png',center=(0,0,.60),distance=3,ortho=3.1,size=(1200,950))
        r.animation_data.action=bpy.data.actions['Run'];bpy.context.scene.frame_set(7)
        a.preview('Previews/Tiger_Run.png',center=(0,0,.60),distance=3,ortho=3.1,size=(1200,950))
        r.animation_data.action=None;a.apply_pose(r,{})
    # Update geometry counts after the cap/stripe cleanup.
    for item in manifest['assets']:
        if item['name']=='Accessory_Cap':item.update(a.stats([cap])) if kind=='human' else None
        if kind=='Tiger' and item['name']=='Tiger_Body':item.update(a.stats([body]))
        if kind=='Tiger' and item['name']=='Tiger':item.update(a.stats(chosen))
        if kind=='human' and item['kind']=='human_preset':
            parts=presets['common']+presets['presets'][item['name']];item.update(a.stats([o for o in objects if o.name in parts]))
    rows=namespace['contacts'](r,kind)
    # Human run now has a 38% stance, leaving a genuine aerial interval in each step.
    if kind=='human':
        r.animation_data.action=bpy.data.actions['Run'];err=0
        for frame in range(1,25):
            bpy.context.scene.frame_set(frame);t=(frame-1)/24
            for bn,phase in [('foot.L',0),('foot.R',.5)]:
                if (t+phase)%1<.38:err=max(err,abs(r.pose.bones[bn].head.z-r.data.bones[bn].head_local.z))
        rows[1]={'clip':'Run','stance_ankle_height_max_error_m':round(err,6),'stance_fraction':.38}
        r.animation_data.action=None;a.apply_pose(r,{})
    for item in contact_rows:
        if item['rig']==r['rig_contract']:item['stance_contacts']=rows
    bpy.context.scene.frame_set(1);a.apply_pose(r,{});a.select([r]+chosen);a.save(relative)
    namespace['motion_sheet'](r,chosen,'Human' if kind=='human' else kind)
manifest_file.write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf8')
(ROOT/'Documentation/contact_checks.json').write_text(json.dumps(contact_rows,indent=2),encoding='utf8')
trans_file=ROOT/'Documentation/animation_transitions.json';trans=json.loads(trans_file.read_text(encoding='utf8'))
trans['locomotion']['speed_mps_suggested']={'human_walk':.968,'human_run':2.303,'human_crouch_walk':.194,'tiger_walk':.738,'tiger_run':2.786,'wolf_walk':.601,'wolf_run':2.266,'fox_walk':.389,'fox_run':1.467,'deer_walk':.581,'deer_run':2.191}
trans['locomotion']['playback_speed_rule']='Start at the listed speed for playback_rate=1. For another planar speed, playback_rate = desired_speed / listed_speed. Fine-tune foot placement for terrain and blend changes.'
trans_file.write_text(json.dumps(trans,ensure_ascii=False,indent=2),encoding='utf8')
print('FINAL_POLISH_COMPLETE',flush=True)
