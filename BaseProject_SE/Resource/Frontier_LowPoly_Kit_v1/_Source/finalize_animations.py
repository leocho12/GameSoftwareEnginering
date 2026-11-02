import bpy, sys, json, math, importlib.util, hashlib
from pathlib import Path
from mathutils import Vector
SOURCE=Path(__file__).with_name('author_kit.py')
s=SOURCE.read_text(encoding='utf-8-sig')
s=s.replace('rootz+=.045+.035*cos(4*pi*t)','rootz+=-.08+.035*cos(4*pi*t)')
s=s.replace('elif gait:rootz+=.013*cos(4*pi*t)','elif gait:rootz+=-.035+.013*cos(4*pi*t)')
s=s.replace('rootz+=h*(.045+.04*cos(4*pi*t))','rootz+=h*(-.10+.04*cos(4*pi*t))')
s=s.replace('elif gait:rootz+=h*.009*cos(4*pi*t)','elif gait:rootz+=h*(-.025+.009*cos(4*pi*t))')
s=s.replace("else 'none','root_yaw_degrees'","else 'vertical_posture_or_bob','horizontal_root_motion':False,'root_yaw_degrees'")
s=s.replace("floor=box('Preview_Ground',(center[0],center[1],-.11)","floor=box('Preview_Ground',(center[0],center[1],(-.11 if 'Nature_Modules' in str(path) else -.055))")
SOURCE.write_text(s,encoding='utf8')
spec=importlib.util.spec_from_file_location('kit_author',SOURCE);a=importlib.util.module_from_spec(spec);spec.loader.exec_module(a)
ROOT=a.ROOT
manifest_path=ROOT/'Documentation/asset_manifest.json';manifest=json.loads(manifest_path.read_text(encoding='utf8'))
checks=[]
def contacts(r,kind):
    rows=[]
    for clip,frames,stance in [('Walk',31,.62 if kind=='human' else .65),('Run',25,.62 if kind=='human' else .35)]:
        r.animation_data.action=bpy.data.actions[clip];worst=0;values=[]
        for frame in range(1,frames):
            bpy.context.scene.frame_set(frame);t=(frame-1)/(frames-1)
            if kind=='human':
                feet=[('foot.L',0),('foot.R',.5)]
            else:
                offsets={('F','L'):0,('H','R'):.25,('F','R'):.5,('H','L'):.75} if clip=='Walk' else {('F','L'):0,('F','R'):.1,('H','L'):.53,('H','R'):.63}
                feet=[(e+'_paw.'+side,phase) for (e,side),phase in offsets.items()]
            for bone,offset in feet:
                p=(t+offset)%1
                if p<stance:
                    expected=r.data.bones[bone].head_local.z
                    actual=(r.matrix_world@r.pose.bones[bone].head).z
                    error=actual-expected;values.append(error);worst=max(worst,abs(error))
        rows.append({'clip':clip,'stance_ankle_height_max_error_m':round(worst,6),'min_error_m':round(min(values),6),'max_error_m':round(max(values),6)})
    r.animation_data.action=None;a.apply_pose(r,{})
    return rows
def motion_sheet(r,objects,label):
    samples=[('Walk',9),('Run',7),('Turn_L90',16),('Attack',16),('Jump',23),('Crouch',15)]
    spacing=2.15 if label=='Human' else 2.8;copies=[]
    for o in objects:o.hide_set(False);o.hide_render=False
    for idx,(clip,frame) in enumerate(samples):
        r.animation_data.action=bpy.data.actions[clip];bpy.context.scene.frame_set(frame)
        dg=bpy.context.evaluated_depsgraph_get()
        for o in objects:
            me=bpy.data.meshes.new_from_object(o.evaluated_get(dg),preserve_all_data_layers=True,depsgraph=dg)
            copy=bpy.data.objects.new('Snapshot_'+clip+'__'+o.name,me);bpy.context.scene.collection.objects.link(copy)
            copy.matrix_world=o.matrix_world.copy();copy.location.x+=idx*spacing;copies.append(copy)
        cv=bpy.data.curves.new('Label_'+clip,'FONT');cv.body=clip;cv.align_x='CENTER';cv.size=.16 if label=='Human' else .19;cv.extrude=.001
        ob=bpy.data.objects.new('Label_'+clip,cv);bpy.context.scene.collection.objects.link(ob);ob.location=(idx*spacing,-.95,.005);cv.materials.append(a.mat('ivory'));copies.append(ob)
    for o in objects:o.hide_render=True
    r.animation_data.action=None;a.apply_pose(r,{})
    center=((len(samples)-1)*spacing/2,0,.85 if label=='Human' else .7)
    a.preview('Previews/'+label+'_Motion_Sheet.png',center=center,distance=5,ortho=spacing*7.1,size=(2100,800))
    for o in copies:bpy.data.objects.remove(o,do_unlink=True)
    for o in objects:o.hide_render=False

families=[('Characters/Human_Modular_Library.blend','human')]+[('Animals/'+species+'_Modular.blend',species) for species in a.ANIMALS]
for relative,kind in families:
    bpy.ops.wm.open_mainfile(filepath=str(ROOT/relative))
    a.MATS={key:bpy.data.materials.get('MAT_'+key) for key in a.PALETTE if bpy.data.materials.get('MAT_'+key)}
    r=next(o for o in bpy.context.scene.objects if o.type=='ARMATURE')
    if r.animation_data:r.animation_data.action=None
    for act in list(bpy.data.actions):bpy.data.actions.remove(act)
    family=r['rig_contract'];a.animate(r,family,a.human_sampler if kind=='human' else a.animal_sampler)
    manifest['animations'][family]=a.MANIFEST['animations'][family]
    checks.append({'rig':family,'stance_contacts':contacts(r,kind)})
    objects=[o for o in bpy.context.scene.objects if o.type=='MESH' and o.parent==r]
    if kind=='human':
        combos=json.loads((ROOT/'Documentation/human_presets.json').read_text(encoding='utf8'))
        for name,parts in combos['presets'].items():
            chosen=[o for o in objects if o.name in combos['common']+parts]
            a.export('Characters/Presets/'+name+'.glb',[r]+chosen)
            for o in objects:o.hide_render=o not in chosen;o.hide_set(o not in chosen)
            pass # Human beauty previews already verified.
        defaults=combos['common']+['Head_A','Face_Calm','Hair_Crop','Torso_FieldJacket','Sleeve_FieldJacket_L','Sleeve_FieldJacket_R','Accessory_TacticalVest']
        chosen=[o for o in objects if o.name in defaults]
        for o in objects:o.hide_render=o not in chosen;o.hide_set(o not in chosen)
    else:
        chosen=objects;a.export('Animals/'+kind+'.glb',[r]+chosen)
        cfg=a.ANIMALS[kind];deer=kind=='Deer';h=cfg['height'];L=cfg['length']
        a.preview('Previews/'+kind+'.png',center=(0,0,h*(.88 if deer else .67)),distance=3,ortho=max(L+cfg['tail']+.6,h*(2.5 if deer else 2)),size=(1200,950))
        r.animation_data.action=bpy.data.actions['Run'];bpy.context.scene.frame_set(7)
        a.preview('Previews/'+kind+'_Run.png',center=(0,0,h*(.88 if deer else .67)),distance=3,ortho=max(L+cfg['tail']+.6,h*(2.5 if deer else 2)),size=(1200,950))
        r.animation_data.action=None;a.apply_pose(r,{})
    bpy.context.scene.frame_set(1);a.apply_pose(r,{});a.select([r]+chosen);a.save(relative)
    if kind=='Tiger':motion_sheet(r,chosen,kind)
    print('RIG_FINALIZED',family,flush=True)
manifest_path.write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf8')
(ROOT/'Documentation/contact_checks.json').write_text(json.dumps(checks,indent=2),encoding='utf8')
print('ANIMATION_FINALIZATION_COMPLETE',flush=True)


