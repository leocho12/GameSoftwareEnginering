import bpy, math, json, random, struct, traceback, sys, os
from pathlib import Path
from mathutils import Vector, Quaternion, noise
from math import sin, cos, pi
ROOT=Path(__file__).resolve().parents[1]
FPS=30
bpy.context.preferences.filepaths.save_version=0
RNG=random.Random(721)
MANIFEST={'pack':'Frontier LowPoly Kit v1','blender':bpy.app.version_string,'units':'meters','up_blender':'+Z','forward_blender':'-Y','up_glb':'+Y','forward_glb':'+Z','fps':FPS,'assets':[],'rigs':{},'animations':{},'status':'authoring'}
for sub in ['Characters/Modules','Characters/Presets','Animals','Settlement/Modules','Nature/Modules','Materials/Textures','Documentation','Previews','_Source']:
    (ROOT/sub).mkdir(parents=True,exist_ok=True)
PALETTE={
'skin':('#b38771',.84),'skin_warm':('#956b55',.84),'skin_light':('#c5a58b',.84),
'olive':('#59604b',.92),'olive_dark':('#373e34',.92),'canvas':('#8c8970',.95),
'charcoal':('#313a3e',.88),'cloth_gray':('#707875',.95),'leather':('#5e4839',.79),
'leather_dark':('#332d28',.84),'rubber':('#232b2e',.95),'steel':('#667779',.51),
'rust':('#805447',.91),'wood':('#80705a',.9),'wood_light':('#a08e6b',.91),
'concrete':('#8e9288',.94),'glass':('#689295',.31),'ivory':('#d5ccb2',.8),
'eye':('#17292e',.3),'white':('#c5c9b9',.84),'hair_black':('#303432',.94),
'hair_brown':('#55443b',.94),'hair_ash':('#9a978a',.94),'accent':('#af674d',.8),
'grass':('#667148',1),'grass_dark':('#414f39',1),'leaf':('#62714d',1),
'pine':('#3e5b4c',1),'bark':('#675343',1),'soil':('#70624d',1),
'rock':('#787f78',.96),'sand':('#a59c7c',1),'tiger':('#b1854e',.93),
'fur_light':('#c7baa0',.98),'stripe':('#38382e',.98),'wolf':('#6d746c',.97),
'wolf_dark':('#46524d',.98),'fox':('#a5724a',.97),'deer':('#9a8464',.98),
'water':('#4e7276',.23)
}
MATS={}
def linear(c):
    return c/12.92 if c<=.04045 else ((c+.055)/1.055)**2.4
def rgb(h):
    return tuple(linear(int(h[i:i+2],16)/255) for i in (1,3,5))
def mat(key):
    if key in MATS:return MATS[key]
    h,r=PALETTE[key]
    m=bpy.data.materials.new('MAT_'+key);m.diffuse_color=(*rgb(h),1);m.use_nodes=True
    p=m.node_tree.nodes.get('Principled BSDF');p.inputs['Base Color'].default_value=m.diffuse_color;p.inputs['Roughness'].default_value=r
    if key=='steel':p.inputs['Metallic'].default_value=.65
    m['palette_srgb']=h;MATS[key]=m;return m
def clean():
    
    for obj in list(bpy.data.objects): bpy.data.objects.remove(obj,do_unlink=True)
    for collection in list(bpy.data.collections): bpy.data.collections.remove(collection)
    for a in list(bpy.data.actions):bpy.data.actions.remove(a)
    bpy.context.scene.frame_start=1;bpy.context.scene.frame_end=61;bpy.context.scene.render.fps=FPS
def coll(name):
    c=bpy.data.collections.new(name);bpy.context.scene.collection.children.link(c);return c
def move_to(o,c):
    for old in list(o.users_collection):old.objects.unlink(o)
    c.objects.link(o)
def shade(o):
    for p in o.data.polygons:p.use_smooth=False
def weights(o,w):
    for i,ws in enumerate(w):
        for bone,value in ws.items():
            if value>1e-7:
                g=o.vertex_groups.get(bone) or o.vertex_groups.new(name=bone);g.add([i],value,'REPLACE')
def mesh(name,vs,fs,material,w=None,c=None):
    me=bpy.data.meshes.new(name+'_Mesh');me.from_pydata(vs,[],fs);me.update()
    o=bpy.data.objects.new(name,me);(c or bpy.context.scene.collection).objects.link(o);me.materials.append(mat(material) if isinstance(material,str) else material)
    if w:weights(o,w)
    uv=me.uv_layers.new(name='UVMap')
    for p in me.polygons:
        axis=max(range(3),key=lambda i:abs(p.normal[i]));axes=([1,2],[0,2],[0,1])[axis]
        for li in p.loop_indices:
            v=me.vertices[me.loops[li].vertex_index].co;uv.data[li].uv=(v[axes[0]],v[axes[1]])
    shade(o);return o
def box(name,center,size,material,bone=None,c=None,bevel=0):
    x,y,z=center; a,b,d=[s/2 for s in size]
    v=[(x+sx*a,y+sy*b,z+sz*d) for sx,sy,sz in [(-1,-1,-1),(1,-1,-1),(1,1,-1),(-1,1,-1),(-1,-1,1),(1,-1,1),(1,1,1),(-1,1,1)]]
    f=[(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)]
    o=mesh(name,v,f,material,[{bone:1} for _ in v] if bone else None,c)
    if bevel:
        mod=o.modifiers.new('Worn_edges','BEVEL');mod.width=bevel;mod.segments=1
        bpy.context.view_layer.objects.active=o;bpy.ops.object.modifier_apply(modifier=mod.name)
    return o
def ellipsoid(name,center,radii,material,bone=None,c=None,segments=12,rings=7):
    x,y,z=center;rx,ry,rz=radii;v=[(x,y,z+rz)];f=[]
    for j in range(1,rings):
        p=pi*j/rings
        for i in range(segments):
            a=2*pi*i/segments;v.append((x+rx*sin(p)*cos(a),y+ry*sin(p)*sin(a),z+rz*cos(p)))
    v.append((x,y,z-rz));bottom=len(v)-1
    for i in range(segments):f.append((0,1+i,1+(i+1)%segments))
    for j in range(rings-2):
        s=1+j*segments;t=s+segments
        for i in range(segments):n=(i+1)%segments;f.append((s+i,t+i,t+n,s+n))
    s=1+(rings-2)*segments
    for i in range(segments):f.append((bottom,s+(i+1)%segments,s+i))
    return mesh(name,v,f,material,[{bone:1} for _ in v] if bone else None,c)
def tube(name,points,radii,material,bones=None,c=None,n=8,ring_weights=None):
    v=[];w=[];f=[]
    for j,p in enumerate(points):
        p=Vector(p);t=Vector(points[min(j+1,len(points)-1)])-Vector(points[max(j-1,0)]);t.normalize()
        ref=Vector((0,1,0)) if abs(t.y)<.85 else Vector((1,0,0))
        u=t.cross(ref).normalized();q=t.cross(u).normalized()
        r=radii[j];ra,rb=(r,r) if isinstance(r,(int,float)) else r
        for i in range(n):
            a=2*pi*i/n;v.append(tuple(p+u*(ra*cos(a))+q*(rb*sin(a))))
            if ring_weights:w.append(ring_weights[j])
            elif bones:w.append({bones[min(j,len(bones)-1)]:1})
    for j in range(len(points)-1):
        for i in range(n):
            a=j*n+i;b=j*n+(i+1)%n;f.append((a,b,b+n,a+n))
    f.append(tuple(reversed(range(n))));f.append(tuple(range((len(points)-1)*n,len(points)*n)))
    return mesh(name,v,f,material,w or None,c)
def join(objs,name):
    objs=[o for o in objs if o]
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs:o.select_set(True)
    bpy.context.view_layer.objects.active=objs[0];bpy.ops.object.join()
    o=objs[0];o.name=name;return o
def rig(name,spec,c):
    d=bpy.data.armatures.new(name+'_Skeleton');o=bpy.data.objects.new(name,d);c.objects.link(o)
    bpy.context.view_layer.objects.active=o;o.select_set(True);bpy.ops.object.mode_set(mode='EDIT')
    for n,a,b,p in spec:
        e=d.edit_bones.new(n);e.head=a;e.tail=b
        if p:e.parent=d.edit_bones[p]
        e.use_deform=not n.startswith('socket')
    bpy.ops.object.mode_set(mode='OBJECT');o.show_in_front=True
    o['units']='meters';o['rest_pose']='A_POSE' if name.startswith('HUM') else 'QUADRUPED_STAND'
    return o
def skin(o,r):
    o.parent=r;mod=o.modifiers.new('Skin','ARMATURE');mod.object=r;mod.use_vertex_groups=True
    o['module_socket_contract']=r['rig_contract'];o['asset_role']='skeletal_module'
def qaxis(axis,a):return Quaternion(Vector(axis),a)
def qx(a):return qaxis((1,0,0),a)
def qy(a):return qaxis((0,1,0),a)
def qz(a):return qaxis((0,0,1),a)
QI=Quaternion()
def apply_pose(r,deltas,root_z=0):
    actual={}
    for pb in r.pose.bones:
        parent=pb.parent.name if pb.parent else None;dp=actual.get(parent,QI);d=deltas.get(pb.name,dp);actual[pb.name]=d
        rest=pb.bone.matrix_local.to_quaternion();pb.rotation_mode='QUATERNION';pb.rotation_quaternion=rest.inverted()@dp.inverted()@d@rest
        pb.location=(0,0,0);pb.scale=(1,1,1)
    root=r.pose.bones['root'];root.location=root.bone.matrix_local.to_quaternion().inverted()@Vector((0,0,root_z))
def delta_to(r,bone,direction):
    b=r.data.bones[bone];return (b.tail_local-b.head_local).normalized().rotation_difference(Vector(direction).normalized())
def solve2(a,end,l1,l2,bend=-1):
    a=Vector(a);end=Vector(end);d=end-a;dist=max(.0001,min(d.length,l1+l2-.0001));u=d.normalized()
    along=(l1*l1-l2*l2+dist*dist)/(2*dist);h=math.sqrt(max(0,l1*l1-along*along))
    perp=Vector((0,-u.z,u.y)).normalized()*bend
    return a+u*along+perp*h
def ease(t):return t*t*(3-2*t)
CLIPS=[
('Pose_Neutral',2,False,'pose'),('Pose_Turn_L45',2,False,'pose'),('Pose_Turn_R45',2,False,'pose'),('Pose_Crouch',2,False,'pose'),
('Idle',61,True,'idle'),('Walk',31,True,'locomotion'),('Run',25,True,'locomotion'),
('Turn_L90',31,False,'turn'),('Turn_R90',31,False,'turn'),('Attack',31,False,'attack'),
('Jump',46,False,'jump'),('Jump_Start',10,False,'jump_start'),('Jump_Air',25,True,'jump_air'),('Jump_Land',13,False,'jump_land'),
('Crouch',61,True,'crouch'),('Crouch_Walk',41,True,'locomotion'),
('Stand_To_Crouch',13,False,'transition'),('Crouch_To_Stand',13,False,'transition'),
('Walk_Start',13,False,'transition'),('Walk_Stop',13,False,'transition')]
def animate(r,family,sampler):
    info=[]
    for name,frames,loop,kind in CLIPS:
        if r.animation_data:r.animation_data.action=None
        action=bpy.data.actions.new(name);action.use_fake_user=True;r.animation_data_create();r.animation_data.action=action
        for f in range(1,frames+1):
            phase=(f-1)/(frames-1);sampler(r,name,phase)
            for pb in r.pose.bones:
                pb.keyframe_insert('rotation_quaternion',frame=f,group=pb.name)
                if pb.name=='root':pb.keyframe_insert('location',frame=f,group=pb.name)
        action['loop']=loop;action['category']=kind;action['fps']=FPS;action['rig_contract']=r['rig_contract']
        action.use_frame_range=True;action.frame_start=1;action.frame_end=frames
        info.append({'name':name,'frames':frames,'duration_seconds':round((frames-1)/FPS,4),'loop':loop,'category':kind,'blend_in_seconds':.14 if kind!='attack' else .08,'root_translation':'vertical_only' if name in ['Jump','Jump_Start','Jump_Land'] else 'vertical_posture_or_bob','horizontal_root_motion':False,'root_yaw_degrees':90 if name=='Turn_L90' else -90 if name=='Turn_R90' else 0})
    r.animation_data.action=None;apply_pose(r,{});bpy.context.scene.frame_set(1)
    MANIFEST['animations'][family]=info
def select(objs):
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs:o.hide_set(False);o.select_set(True)
    bpy.context.view_layer.objects.active=objs[0]
def export(path,objs,animation=True):
    select(objs)
    bpy.ops.export_scene.gltf(filepath=str(ROOT/path),export_format='GLB',use_selection=True,
      export_animations=animation,export_animation_mode='ACTIONS',export_force_sampling=True,export_frame_range=False,
      export_anim_slide_to_zero=True,export_reset_pose_bones=True,export_skins=True,export_yup=True,
      export_extras=True,export_cameras=False,export_lights=False,export_materials='EXPORT',export_morph=False)
def stats(objs):
    ms=[o for o in objs if o.type=='MESH']
    return {'meshes':len(ms),'vertices':sum(len(o.data.vertices) for o in ms),'triangles':sum(len(p.vertices)-2 for o in ms for p in o.data.polygons)}
def register(name,kind,path,objs,**kwargs):
    MANIFEST['assets'].append(dict(name=name,kind=kind,file=str(path).replace('\\','/'),**stats(objs),**kwargs))
def save(path):
    bpy.context.view_layer.update()
    visible=[o for o in bpy.context.scene.objects if o.type=='MESH' and not o.hide_get() and not o.hide_render]
    if visible:
        corners=[o.matrix_world@Vector(v) for o in visible for v in o.bound_box]
        minimum=Vector(tuple(min(v[i] for v in corners) for i in range(3)));maximum=Vector(tuple(max(v[i] for v in corners) for i in range(3)))
        center=(minimum+maximum)*.5
        for screen in bpy.data.screens:
            for area in screen.areas:
                if area.type=='VIEW_3D':
                    region=area.spaces.active.region_3d;region.view_location=center;region.view_distance=max(2,(maximum-minimum).length*1.05)
                    region.view_rotation=Vector((3,-5,3)).to_track_quat('Z','Y');region.view_perspective='ORTHO'
    bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/path),compress=True)
def preview(path,center=(0,0,1),distance=4,ortho=3,size=(1200,1000)):
    sc=bpy.context.scene;sc.render.engine='CYCLES';sc.cycles.samples=24;sc.cycles.use_denoising=True
    sc.render.resolution_x=size[0];sc.render.resolution_y=size[1];sc.render.resolution_percentage=100
    sc.render.image_settings.file_format='PNG';sc.render.film_transparent=False
    sc.world.color=(.15,.15,.15);sc.world.use_nodes=True;sc.world.node_tree.nodes['Background'].inputs[0].default_value=(.22,.27,.3,1);sc.world.node_tree.nodes['Background'].inputs[1].default_value=.55
    added=[];floor=box('Preview_Ground',(center[0],center[1],(-.11 if 'Nature_Modules' in str(path) else -.055)),(100,100,.1),'charcoal');added.append(floor)
    d=bpy.data.lights.new('Preview_Sun','SUN');d.energy=1.2;d.angle=.15
    o=bpy.data.objects.new('Preview_Sun',d);sc.collection.objects.link(o);o.rotation_euler=(.35,-.45,-.4);added.append(o)
    for name,loc,power,size_light in [('Key',(center[0]-3,center[1]-4,7),1100,5),('Fill',(center[0]+5,center[1]-1,4),700,4),('Rim',(center[0],center[1]+4,6),1300,3)]:
        d=bpy.data.lights.new('Preview_'+name,'AREA');d.energy=power;d.shape='DISK';d.size=size_light
        o=bpy.data.objects.new('Preview_'+name,d);sc.collection.objects.link(o);o.location=loc;o.rotation_euler=(Vector(center)-o.location).to_track_quat('-Z','Y').to_euler();added.append(o)
    d=bpy.data.cameras.new('Preview_Camera');o=bpy.data.objects.new('Preview_Camera',d);sc.collection.objects.link(o);added.append(o)
    o.location=Vector(center)+Vector((distance,-distance*1.65,distance*.85));o.rotation_euler=(Vector(center)-o.location).to_track_quat('-Z','Y').to_euler();d.type='ORTHO';d.ortho_scale=ortho;sc.camera=o
    sc.render.filepath=str(ROOT/path);bpy.ops.render.render(write_still=True)
    for o in added:bpy.data.objects.remove(o,do_unlink=True)
    sc.camera=None

def human_spec():
    s=[('root',(0,0,0),(0,0,.22),None),('pelvis',(0,0,.98),(0,0,1.10),'root'),('spine_01',(0,0,1.10),(0,0,1.27),'pelvis'),('spine_02',(0,0,1.27),(0,0,1.43),'spine_01'),('neck',(0,0,1.43),(0,0,1.57),'spine_02'),('head',(0,0,1.57),(0,0,1.82),'neck')]
    for side,sign in [('L',1),('R',-1)]:
        s += [('clavicle.'+side,(0,0,1.43),(sign*.19,0,1.43),'spine_02'),
              ('upper_arm.'+side,(sign*.19,0,1.43),(sign*.365,0,1.19),'clavicle.'+side),
              ('forearm.'+side,(sign*.365,0,1.19),(sign*.48,-.015,.985),'upper_arm.'+side),
              ('hand.'+side,(sign*.48,-.015,.985),(sign*.51,-.025,.85),'forearm.'+side),
              ('thigh.'+side,(sign*.098,0,.98),(sign*.098,-.018,.54),'pelvis'),
              ('shin.'+side,(sign*.098,-.018,.54),(sign*.098,0,.145),'thigh.'+side),
              ('foot.'+side,(sign*.098,0,.145),(sign*.098,-.16,.065),'shin.'+side),
              ('toe.'+side,(sign*.098,-.16,.065),(sign*.098,-.235,.065),'foot.'+side)]
        for digit,off in [('index',-.036),('middle',-.012),('ring',.012),('little',.033)]:
            a=(sign*.511,off-.02,.89);b=(sign*.528,off-.02,.835);d=(sign*.534,off-.02,.808)
            s += [(digit+'_01.'+side,a,b,'hand.'+side),(digit+'_02.'+side,b,d,digit+'_01.'+side)]
        s += [('thumb_01.'+side,(sign*.489,-.05,.958),(sign*.472,-.07,.915),'hand.'+side),
              ('thumb_02.'+side,(sign*.472,-.07,.915),(sign*.474,-.07,.884),'thumb_01.'+side)]
    s += [('socket_head',(0,0,1.80),(0,0,1.9),'head'),('socket_back',(0,.125,1.32),(0,.23,1.32),'spine_02'),
          ('socket_hand.L',(.51,-.04,.91),(.51,-.13,.91),'hand.L'),('socket_hand.R',(-.51,-.04,.91),(-.51,-.13,.91),'hand.R')]
    return s
def human_sampler(r,name,t):
    crouch=1.0 if name in ['Crouch','Pose_Crouch','Crouch_Walk'] else ease(t) if name=='Stand_To_Crouch' else 1-ease(t) if name=='Crouch_To_Stand' else 0
    phase=t;stride=.60;lift=.11;rootz=-.29*crouch;pitch=.2*crouch;heading=0
    gait=name in ['Walk','Run','Crouch_Walk','Walk_Start','Walk_Stop','Turn_L90','Turn_R90']
    stance=.38 if name=='Run' else .62
    amp=1
    if name=='Run':stride=.70;lift=.22;rootz+=-.125+.035*cos(4*pi*t);pitch=.13
    elif gait:rootz+=-.070+.013*cos(4*pi*t)
    if name=='Walk_Start':phase=-.5+t*.5;amp=ease(t);stride*=amp;lift*=amp
    if name=='Walk_Stop':phase=t*.5;amp=1-ease(t);stride*=amp;lift*=amp
    if name=='Crouch_Walk':stride=.16;lift=.045
    if name.startswith('Turn_'):
        heading=(1 if '_L' in name else -1)*pi/2*ease(t);stride=.09;lift=.075
    jump=0;air=0
    if name=='Jump':
        if t<.2:crouch=.55*sin(pi*t/.2);rootz=-.15*sin(pi*t/.2)
        elif t<.75:air=sin(pi*(t-.2)/.55);jump=.58*air;rootz=jump
        else:rootz=-.13*sin(pi*(t-.75)/.25)
    elif name=='Jump_Start':rootz=-.16*sin(pi*t)+.01*t;pitch=.12*sin(pi*t)
    elif name=='Jump_Air':rootz=0;air=1;pitch=.04*sin(2*pi*t)
    elif name=='Jump_Land':rootz=-.16*sin(pi*t);pitch=.15*sin(pi*t)
    if name in ['Crouch','Idle']:rootz+=.006*sin(2*pi*t)
    turnpose=(1 if name=='Pose_Turn_L45' else -1 if name=='Pose_Turn_R45' else 0)
    rootq=qz(heading);pelq=rootq@qz(turnpose*.20)@qx(pitch);spq=rootq@qz(turnpose*.5)@qx(pitch*.65)
    dq={'root':rootq,'pelvis':pelq,'spine_01':spq,'spine_02':spq,'neck':rootq@qz(turnpose*.65),'head':rootq@qz(turnpose*pi/4)}
    attack=sin(pi*min(1,max(0,(t-.12)/.68))) if name=='Attack' else 0
    if attack:
        dq['spine_02']=rootq@qz(-.45*attack)@qx(.1*attack);dq['head']=rootq@qz(-.18*attack)
    for side,sign,offset in [('L',1,0),('R',-1,.5)]:
        p=(phase+offset)%1
        fy=0;fz=.145
        if gait:
            if p<stance:fy=(-stride/2+stride*p/stance)
            else:
                u=(p-stance)/(1-stance);fy=stride/2-stride*ease(u);fz+=lift*sin(pi*u)
        if air:fy=-.07;fz+=.20*air
        if not gait:fy=-.05*crouch
        hip=Vector((sign*.098,0,.98+rootz));ank=Vector((sign*.098,fy,fz+max(0,jump)))
        if name=='Jump_Air':ank.z+=0
        knee=solve2(hip,ank,.4404,.3954,-1)
        dq['thigh.'+side]=rootq@delta_to(r,'thigh.'+side,knee-hip)
        dq['shin.'+side]=rootq@delta_to(r,'shin.'+side,ank-knee)
        dq['foot.'+side]=rootq@qx(-.15*air+.08*sin(2*pi*p) if gait else -.12*air)
        dq['toe.'+side]=dq['foot.'+side]
        arm=.43*sin(2*pi*p)*amp if gait else .015*sin(2*pi*t)*sign
        dq['clavicle.'+side]=spq
        dq['upper_arm.'+side]=rootq@qx(arm-.35*crouch-.65*air)@qy(sign*.08*air)
        dq['forearm.'+side]=rootq@qx(arm-(.72 if name=='Run' else .13)-.7*crouch-.45*air)
        dq['hand.'+side]=dq['forearm.'+side]
        if name=='Attack' and side=='R':
            dq['upper_arm.R']=rootq@qx(-1.45*attack)@qy(-.25*attack)
            dq['forearm.R']=rootq@qx(-1.7*attack)@qz(-.35*attack);dq['hand.R']=dq['forearm.R']
    apply_pose(r,dq,rootz)

def make_humans():
    clean();c=coll('HUMAN_ModularLibrary');r=rig('HUM_7H_v1',human_spec(),c);r['rig_contract']='HUM_7H_v1';r['height_m']=1.82;r['head_count']=7.0
    modules={};default=[]
    def add(name,parts,visible=True,slot=''):
        o=join(parts,name);skin(o,r);o['slot']=slot;modules[name]=o
        if visible:default.append(o)
        else:o.hide_render=True;o.hide_set(True)
        return o
    for variant,material in [('A','skin'),('B','skin_warm')]:
        parts=[ellipsoid('Cranium',(0,.005,1.698),(.104,.091,.122),material,'head',c,12,9),
          tube('Jaw',[(0,-.005,1.68),(0,-.012,1.615)],[(.082,.076),(.056,.060)],material,['head','head'],c,n=8),
          tube('Neck',[(0,0,1.48),(0,0,1.595)],[.046,.045],material,['neck','head'],c),
          ellipsoid('Nose',(0,-.087,1.688),(.019,.034,.027),material,'head',c,6,4)]
        for sign in [-1,1]:parts.append(ellipsoid('Ear',(sign*.105,.005,1.692),(.017,.026,.038),material,'head',c,6,4))
        add('Head_'+variant,parts,variant=='A','HEAD')
    for variant,angry in [('Calm',False),('Alert',True)]:
        p=[]
        for sign in [-1,1]:
            p.append(ellipsoid('Eye',(sign*.041,-.081,1.724),(.023,.012,.012),'ivory','head',c,8,4))
            p.append(ellipsoid('Pupil',(sign*.039,-.092,1.723),(.008,.003,.009),'eye','head',c,6,4))
            brow=box('Brow',(sign*.041,-.09,1.745),(.044,.009,.008),'hair_black','head',c);p.append(brow)
            if angry:
                center=Vector((sign*.041,-.09,1.745));rotation=qy(sign*-.18)
                for vertex in brow.data.vertices:vertex.co=center+rotation@(vertex.co-center)
        p.append(box('Mouth',(0,-.077,1.637),(.039,.005,.004),'leather_dark','head',c))
        add('Face_'+variant,p,variant=='Calm','FACE')
    crop=ellipsoid('Crop',(0,.014,1.772),(.108,.097,.052),'hair_black','head',c,12,5)
    # Only the upper cap is used, avoiding a full dark helmet over the face.
    add('Hair_Crop',[crop],True,'HAIR')
    p=[ellipsoid('Cap',(0,.017,1.773),(.112,.102,.051),'hair_brown','head',c,12,5)]
    for sign in [-1,1]:
        p.append(tube('Bob_side',[(sign*.091,.017,1.76),(sign*.106,.025,1.65),(sign*.09,.025,1.59)],[.034,.038,.025],'hair_brown',['head']*3,c,n=6))
    p.append(box('Bob_Back',(0,.083,1.675),(.18,.055,.19),'hair_brown','head',c,bevel=.008))
    add('Hair_Bob',p,False,'HAIR')
    add('Hair_Tied',[ellipsoid('Cap',(0,.02,1.77),(.106,.096,.049),'hair_ash','head',c,12,5),
          tube('Ponytail',[(0,.095,1.74),(0,.14,1.66),(0,.15,1.51)],[(.036,.035),(.031,.03),(.018,.018)],'hair_ash',['head']*3,c,n=7)],False,'HAIR')
    for variant,material in [('FieldJacket','olive'),('CivilianShirt','cloth_gray'),('WorkerCoat','canvas')]:
        pts=[(0,0,1.035),(0,0,1.14),(0,0,1.29),(0,0,1.405),(0,0,1.465)]
        rad=[(.165,.105),(.158,.104),(.202,.119),(.208,.105),(.118,.066)]
        # A vertical tube uses X as its cross-section second axis.
        rad=[(width,depth) for width,depth in rad]
        rw=[{'pelvis':.8,'spine_01':.2},{'pelvis':.25,'spine_01':.75},{'spine_01':.35,'spine_02':.65},{'spine_02':1},{'spine_02':.7,'neck':.3}]
        p=[tube('Garment',pts,rad,material,None,c,n=12,ring_weights=rw),
           tube('Collar',[(0,0,1.45),(0,0,1.50)],[(.06,.075),(.052,.06)],material,['neck','neck'],c,n=10)]
        if variant!='CivilianShirt':
            for sign in [-1,1]:
                p.append(box('ChestPocket',(sign*.092,-.112,1.325),(.11,.023,.105),material,'spine_02',c,bevel=.006))
                p.append(box('PocketFlap',(sign*.092,-.126,1.366),(.116,.012,.025),'olive_dark' if variant=='FieldJacket' else 'wood','spine_02',c))
        p += [box('Zipper',(0,-.118,1.28),(.009,.008,.29),'steel','spine_02',c)]
        add('Torso_'+variant,p,variant=='FieldJacket','TORSO')
        for side,sign in [('L',1),('R',-1)]:
            p=[tube('Sleeve',[(sign*.18,0,1.43),(sign*.276,0,1.302),(sign*.365,0,1.19),(sign*.428,-.009,1.075),(sign*.48,-.015,.985)],
              [.089,.077,.067,.058,.052],material,None,c,n=8,
              ring_weights=[{'clavicle.'+side:.25,'upper_arm.'+side:.75},{'upper_arm.'+side:1},{'upper_arm.'+side:.5,'forearm.'+side:.5},{'forearm.'+side:1},{'forearm.'+side:1}])]
            p.append(ellipsoid('Shoulder',(sign*.197,0,1.425),(.091,.086,.085),material,'upper_arm.'+side,c,10,5))
            add('Sleeve_'+variant+'_'+side,p,variant=='FieldJacket','ARM_'+side)
    p=[tube('Hip',[(0,0,.90),(0,0,1.06)],[(.166,.10),(.167,.10)],'charcoal',['pelvis']*2,c,n=10),
       tube('Belt',[(0,0,1.045),(0,0,1.079)],[(.172,.108),(.17,.108)],'leather',['pelvis']*2,c,n=12),
       box('Buckle',(0,-.112,1.06),(.055,.016,.03),'steel','pelvis',c)]
    add('Pelvis_Trousers',p,True,'PELVIS')
    for side,sign in [('L',1),('R',-1)]:
        p=[tube('Trousers',[(sign*.098,0,.985),(sign*.098,-.004,.79),(sign*.098,-.018,.54),(sign*.098,-.008,.34),(sign*.098,0,.16)],
          [.09,.085,.067,.06,.053],'charcoal',None,c,n=8,
          ring_weights=[{'pelvis':.3,'thigh.'+side:.7},{'thigh.'+side:1},{'thigh.'+side:.5,'shin.'+side:.5},{'shin.'+side:1},{'shin.'+side:1}]),
          box('KneePanel',(sign*.098,-.065,.54),(.107,.025,.13),'olive_dark','shin.'+side,c,bevel=.009)]
        add('Leg_Trousers_'+side,p,True,'LEG_'+side)
        p=[tube('BootUpper',[(sign*.098,.004,.085),(sign*.098,0,.265)],[(.066,.075),(.06,.06)],'leather',['foot.'+side,'shin.'+side],c,n=8),
           ellipsoid('BootToe',(sign*.098,-.084,.078),(.075,.154,.065),'leather_dark','foot.'+side,c,10,5),
           box('Sole',(sign*.098,-.07,.026),(.151,.28,.045),'rubber','foot.'+side,c,bevel=.012)]
        add('Boot_'+side,p,True,'FOOT_'+side)
        p=[ellipsoid('Palm',(sign*.5,-.018,.926),(.044,.047,.072),'leather_dark','hand.'+side,c,8,5)]
        for digit,off in [('index',-.036),('middle',-.012),('ring',.012),('little',.033)]:
            p.append(tube('Finger',[(sign*.511,off-.02,.895),(sign*.528,off-.02,.835),(sign*.534,off-.02,.811)],[.011,.01,.008],'leather_dark',[digit+'_01.'+side,digit+'_02.'+side,digit+'_02.'+side],c,n=5))
        p.append(tube('Thumb',[(sign*.489,-.05,.958),(sign*.472,-.07,.915),(sign*.474,-.07,.884)],[.015,.012,.009],'leather_dark',['thumb_01.'+side,'thumb_02.'+side,'thumb_02.'+side],c,n=5))
        add('Glove_'+side,p,True,'HAND_'+side)
    p=[]
    for sign in [-1,1]:
        p.append(box('VestPanel',(sign*.09,-.133,1.30),(.145,.065,.26),'olive_dark','spine_02',c,bevel=.012))
        p.append(box('UtilityPouch',(sign*.085,-.18,1.20),(.12,.05,.095),'canvas','spine_01',c,bevel=.006))
    add('Accessory_TacticalVest',p,True,'OVERLAY_TORSO')
    add('Accessory_Backpack',[box('Pack',(0,.18,1.30),(.28,.17,.34),'canvas','spine_02',c,bevel=.028),
          box('PackLid',(0,.19,1.48),(.30,.19,.055),'olive_dark','spine_02',c,bevel=.012)],False,'BACK')
    add('Accessory_Cap',[ellipsoid('Cap',(0,.018,1.78),(.113,.102,.052),'olive','head',c,12,5),
          box('Peak',(0,-.107,1.773),(.16,.13,.013),'olive_dark','head',c,bevel=.003)],False,'HEADWEAR')
    animate(r,'HUM_7H_v1',human_sampler)
    presets={
      'Player_Commander':['Head_A','Face_Alert','Hair_Crop','Torso_FieldJacket','Sleeve_FieldJacket_L','Sleeve_FieldJacket_R','Accessory_TacticalVest','Accessory_Cap'],
      'NPC_Scout':['Head_B','Face_Calm','Hair_Tied','Torso_FieldJacket','Sleeve_FieldJacket_L','Sleeve_FieldJacket_R','Accessory_Backpack'],
      'NPC_Resident':['Head_A','Face_Calm','Hair_Bob','Torso_CivilianShirt','Sleeve_CivilianShirt_L','Sleeve_CivilianShirt_R'],
      'NPC_Worker':['Head_B','Face_Calm','Hair_Crop','Torso_WorkerCoat','Sleeve_WorkerCoat_L','Sleeve_WorkerCoat_R','Accessory_Backpack']}
    common=['Pelvis_Trousers','Leg_Trousers_L','Leg_Trousers_R','Boot_L','Boot_R','Glove_L','Glove_R']
    for name,parts in presets.items():
        path='Characters/Presets/'+name+'.glb';obs=[modules[n] for n in common+parts]
        export(path,[r]+obs);register(name,'human_preset',path,obs,rig='HUM_7H_v1',modules=common+parts)
        for o in modules.values():o.hide_render=o not in obs;o.hide_set(o not in obs)
        preview('Previews/'+name+'.png',center=(0,0,.93),distance=3,ortho=2.25,size=(900,1100))
    for name,o in modules.items():
        export('Characters/Modules/'+name+'.glb',[r,o],False)
        register(name,'human_module','Characters/Modules/'+name+'.glb',[o],rig='HUM_7H_v1',slot=o['slot'])
    for o in modules.values():o.hide_render=o not in default;o.hide_set(o not in default)
    apply_pose(r,{});select([r]+default);save('Characters/Human_Modular_Library.blend')
    MANIFEST['rigs']['HUM_7H_v1']={'bones':len(r.data.bones),'height':1.82,'head_height':.26,'bone_names':[b.name for b in r.data.bones],'part_interchangeability':'All human modules in this pack share exact bind matrices. Overlay vest requires field jacket; hair can be hidden under cap.'}
    (ROOT/'Documentation/human_presets.json').write_text(json.dumps({'common':common,'presets':presets},indent=2),encoding='utf8')
    print('HUMANS_COMPLETE',len(modules),flush=True)


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

ANIMALS={
 'Tiger':dict(family='FELINE_TIGER_v1',behavior='dangerous',length=1.50,height=.90,hip=.84,width=.26,fur='tiger',head=.24,tail=.96,leg=.80),
 'Wolf':dict(family='CANINE_WOLF_v1',behavior='dangerous',length=1.22,height=.98,hip=.91,width=.195,fur='wolf',head=.205,tail=.58,leg=.88),
 'Fox':dict(family='CANINE_FOX_v1',behavior='neutral',length=.79,height=.59,hip=.55,width=.13,fur='fox',head=.145,tail=.60,leg=.52),
 'Deer':dict(family='CERVID_DEER_v1',behavior='gentle',length=1.18,height=1.32,hip=1.25,width=.20,fur='deer',head=.19,tail=.16,leg=1.20)}
def animal_spec(cfg):
    L=cfg['length'];h=cfg['height'];hip=cfg['hip'];w=cfg['width'];deer=cfg['family'].startswith('CERVID');cat=cfg['family'].startswith('FELINE')
    front=-L*.34;rear=L*.34;head_y=-L*.59;head_z=h+(.36 if deer else .08)
    specs=[('root',(0,0,0),(0,0,.20),None),
      ('pelvis',(0,rear,hip),(0,L*.12,hip+.02),'root'),
      ('spine',(0,L*.12,hip+.02),(0,-L*.14,h),'pelvis'),
      ('chest',(0,-L*.14,h),(0,front,h+.01),'spine'),
      ('neck',(0,front,h+.01),(0,head_y,head_z),'chest'),
      ('head',(0,head_y,head_z),(0,head_y-.25,head_z-.025),'neck'),
      ('jaw',(0,head_y-.02,head_z-.065),(0,head_y-.22,head_z-.095),'head')]
    for side,sg in [('L',1),('R',-1)]:
        x=sg*w*.67
        for end in ['F','H']:
            fore=end=='F';y=front if fore else rear;z=h if fore else hip
            if fore:points=[(x,y,z),(x,y+L*.07,z*.56),(x,y-L*.025,z*.18),(x,y-L*.05,.055)]
            else:points=[(x,y,z),(x,y-L*.20,z*.68),(x,y+L*.09,z*.29),(x,y-L*.01,.06)]
            for i,tag in enumerate(['upper','lower','meta']):
                specs.append((end+'_'+tag+'.'+side,points[i],points[i+1],('chest' if fore else 'pelvis') if i==0 else end+'_'+['upper','lower'][i-1]+'.'+side))
            specs.append((end+'_paw.'+side,points[3],(x,points[3][1]-.1,.04),end+'_meta.'+side))
        specs.append(('ear.'+side,(sg*cfg['head']*.6,head_y+.015,head_z+cfg['head']*.54),(sg*cfg['head']*.78,head_y+.015,head_z+cfg['head']*.54+.15),'head'))
    start=Vector((0,rear+L*.1,hip+.01));prev='pelvis'
    for i in range(4):
        end=start+Vector((0,cfg['tail']/4,-.04 if deer else -.085*cfg['tail']/.6))
        specs.append(('tail_%02d'%(i+1),tuple(start),tuple(end),prev));prev='tail_%02d'%(i+1);start=end
    specs += [('socket_back',(0,0,h+.2),(0,0,h+.32),'spine'),('socket_head',(0,head_y,head_z+.2),(0,head_y,head_z+.3),'head')]
    return specs
def animal_sampler(r,name,t):
    cfg=ANIMALS[r['species']];L=cfg['length'];h=cfg['height'];hip=cfg['hip']
    crouch=1 if name in ['Pose_Crouch','Crouch','Crouch_Walk'] else ease(t) if name=='Stand_To_Crouch' else 1-ease(t) if name=='Crouch_To_Stand' else 0
    rootz=-h*.23*crouch;pitch=.11*crouch;heading=0;phase=t
    gait=name in ['Walk','Run','Crouch_Walk','Walk_Start','Walk_Stop','Turn_L90','Turn_R90']
    run=name=='Run';stride=L*(.52 if run else .32);lift=h*(.19 if run else .09);stance=.35 if run else .65
    amp=1
    if name=='Walk_Start':phase=-.5+t*.5;amp=ease(t)
    if name=='Walk_Stop':phase=t*.5;amp=1-ease(t)
    stride*=amp;lift*=amp
    if name=='Crouch_Walk':stride*=.5;lift*=.6
    if run:rootz+=h*(-.10+.04*cos(4*pi*t))-(h*.03 if r['species']=='Tiger' else 0)
    elif gait:rootz+=h*(-.025+.009*cos(4*pi*t))
    if name.startswith('Turn_'):
        heading=(1 if '_L' in name else -1)*pi/2*ease(t);stride=L*.12;lift=h*.07
    air=0;jump=0
    if name=='Jump':
        if t<.2:rootz=-h*.14*sin(pi*t/.2)
        elif t<.76:air=sin(pi*(t-.2)/.56);jump=h*.55*air;rootz=jump
        else:rootz=-h*.15*sin(pi*(t-.76)/.24)
    elif name=='Jump_Start':rootz=-h*.15*sin(pi*t)
    elif name=='Jump_Air':air=1;jump=0;rootz=0
    elif name=='Jump_Land':rootz=-h*.15*sin(pi*t)
    if name in ['Idle','Crouch']:rootz+=h*.004*sin(2*pi*t)
    rootq=qz(heading);body=rootq@qx(pitch)
    turnpose=1 if name=='Pose_Turn_L45' else -1 if name=='Pose_Turn_R45' else 0
    attack=sin(pi*max(0,min(1,(t-.12)/.7))) if name=='Attack' else 0
    deer=r['species']=='Deer'
    dq={'root':rootq,'pelvis':body,'spine':body@qz(turnpose*.10),'chest':body@qz(turnpose*.17),
        'neck':body@qz(turnpose*.38)@qx((.5 if deer else -.10)*attack),
        'head':body@qz(turnpose*pi/4)@qx((.68 if deer else -.15)*attack),
        'jaw':body@qz(turnpose*pi/4)@qx((.68 if deer else -.15)*attack+(.0 if deer else .52*attack))}
    offsets_walk={('F','L'):0,('H','R'):.25,('F','R'):.5,('H','L'):.75}
    offsets_run={('F','L'):0,('F','R'):.10,('H','L'):.53,('H','R'):.63}
    for end in ['F','H']:
        for side in ['L','R']:
            bn=end+'_upper.'+side;bl=end+'_lower.'+side;bm=end+'_meta.'+side;bp=end+'_paw.'+side
            a=r.data.bones[bn].head_local.copy();rest_foot=r.data.bones[bm].tail_local.copy()
            p=(phase+(offsets_run if run else offsets_walk)[(end,side)])%1;dy=0;dz=0
            if gait:
                if p<stance:dy=-stride/2+stride*p/stance
                else:
                    u=(p-stance)/(1-stance);dy=stride/2-stride*ease(u);dz=lift*sin(pi*u)
            foot=rest_foot+Vector((0,dy,dz+jump))
            if air:foot.z+=h*.18;foot.y+=(-.10 if end=='F' else -.17)*air
            if attack and end=='F':
                foot.y-=L*.10*attack
                if not deer and side=='L':foot.z+=h*.28*attack;foot.y-=L*.15*attack
                elif deer and side=='R':foot.z+=h*.12*attack
            # Preserve the species-specific wrist/hock segment when solving the upper two joints.
            distal=r.data.bones[bm].head_local-r.data.bones[bm].tail_local
            wrist=foot+distal
            a.z+=rootz
            l1=r.data.bones[bn].length;l2=r.data.bones[bl].length
            knee=solve2(a,wrist,l1,l2,1 if end=='F' else -1)
            dq[bn]=rootq@delta_to(r,bn,knee-a);dq[bl]=rootq@delta_to(r,bl,wrist-knee)
            dq[bm]=rootq@delta_to(r,bm,foot-wrist);dq[bp]=rootq@qx(-.15*air)
    for i in range(4):dq['tail_%02d'%(i+1)]=body@qz(.10*sin(2*pi*t-i*.5)+turnpose*.10)@qx(.06*sin(2*pi*t))
    for side,sg in [('L',1),('R',-1)]:dq['ear.'+side]=dq['head']@qy(sg*.07*sin(2*pi*t))
    apply_pose(r,dq,rootz)

def make_animal(species):
    clean();cfg=ANIMALS[species];c=coll(species+'_ModularLibrary');r=rig(cfg['family'],animal_spec(cfg),c);r['rig_contract']=cfg['family'];r['species']=species;r['behavior_default']=cfg['behavior']
    L=cfg['length'];h=cfg['height'];hp=cfg['hip'];w=cfg['width'];fur=cfg['fur'];s=cfg['head'];head=r.data.bones['head'].head_local;hy=head.y;hz=head.z;cat=species=='Tiger';deer=species=='Deer';modules={}
    def add(name,parts,slot):
        o=join(parts,species+'_'+name);skin(o,r);o['slot']=slot;modules[name]=o;return o
    body=ellipsoid('Body',(0,0,h*.86),(w,L*.56,h*.285),fur,None,c,16,9)
    # Soft transitions between pelvis, spine and shoulder retain the modular bind contract.
    wts=[]
    for v in body.data.vertices:
        y=v.co.y
        if y>L*.13:
            a=min(1,(y-L*.13)/(L*.25));wts.append({'spine':1-a,'pelvis':a})
        elif y<-.10*L:
            a=min(1,(-y-L*.10)/(L*.25));wts.append({'spine':1-a,'chest':a})
        else:wts.append({'spine':1})
    weights(body,wts);p=[body,ellipsoid('Chest',(0,-L*.30,h*.83),(w*1.08,L*.26,h*.29),fur,'chest',c,12,7)]
    p.append(ellipsoid('Haunch',(0,L*.35,hp*.84),(w*1.05,L*.22,h*.23),fur,'pelvis',c,12,7))
    if False and cat:
        # Historical ribbon construction; the final asset uses a painted surface. follow the ellipsoid surface rather than floating cards.
        for i in range(8):
            y=-L*.41+i*L*.105
            for sg in [-1,1]:
                vs=[];fs=[]
                for j in range(6):
                    angle=.15+j*.43;yy=y+.035*sin(j*1.4+i);xx=sg*(w+.004)*sin(angle);zz=h*.86+(h*.285+.004)*cos(angle)
                    for off in [-.014,.014]:
                        origin=Vector((0,yy+off*(.7+.3*cos(j)),h*.86))
                        direction=Vector((sg*sin(angle),0,cos(angle))).normalized()
                        hit,location,normal,face_index=body.ray_cast(origin,direction)
                        vs.append(tuple(location+normal*.0018) if hit else (xx,origin.y,zz))
                for j in range(5):fs.append((2*j,2*j+1,2*j+3,2*j+2))
                b='pelvis' if y>L*.2 else 'chest' if y<-L*.2 else 'spine';p.append(mesh('Stripe',vs,fs,'stripe',[{b:1} for _ in vs],c))
    if species in ['Wolf','Fox']:
        p.append(ellipsoid('ShoulderMane',(0,-L*.32,h*.95),(w*1.1,L*.22,h*.20),'wolf_dark' if species=='Wolf' else 'fox','chest',c,10,6))
        p.append(ellipsoid('Belly',(0,-L*.04,h*.675),(w*.78,L*.39,h*.12),'fur_light','spine',c,12,5))
    body_module=add('Body',p,'BODY')
    if cat:add_tiger_texture(body_module,cfg)
    neckstart=r.data.bones['neck'].head_local;neckend=r.data.bones['neck'].tail_local
    add('Neck',[tube('Neck',[tuple(neckstart),tuple(neckstart.lerp(neckend,.52)),tuple(neckend)],
       [(w*.78,w*.75),(s*.8,s*.87),(s*.70,s*.73)],fur,None,c,n=10,ring_weights=[{'chest':.5,'neck':.5},{'neck':1},{'neck':.6,'head':.4}])],'NECK')
    p=[ellipsoid('Cranium',(0,hy,hz),(s*.73,s*.86,s*.80),fur,'head',c,12,8)]
    if cat:
        p += [ellipsoid('Muzzle_L',(s*.28,hy-s*.79,hz-s*.29),(s*.38,s*.44,s*.27),'fur_light','head',c,8,5),
              ellipsoid('Muzzle_R',(-s*.28,hy-s*.79,hz-s*.29),(s*.38,s*.44,s*.27),'fur_light','head',c,8,5)]
        nose_y=hy-s*1.13
    else:
        p.append(tube('Muzzle',[(0,hy-s*.1,hz-s*.19),(0,hy-s*.94,hz-s*.40),(0,hy-s*1.28,hz-s*.42)],
           [(s*.47,s*.48),(s*.31,s*.29),(s*.26,s*.25)],fur,['head']*3,c,n=8))
        nose_y=hy-s*1.29
    p.append(ellipsoid('Nose',(0,nose_y,hz-s*(.29 if cat else .40)),(s*.24,s*.13,s*.16),'eye','head',c,8,4))
    for sg in [-1,1]:
        p.append(ellipsoid('EyeSocket',(sg*s*.63,hy-s*.43,hz+s*.17),(s*.12,s*.14,s*.125),'leather_dark','head',c,8,4))
        p.append(ellipsoid('Eye',(sg*s*.69,hy-s*.48,hz+s*.18),(s*.065,s*.085,s*.07),'accent' if cat else 'eye','head',c,6,4))
        p.append(ellipsoid('EyeGlint',(sg*s*.71,hy-s*.53,hz+s*.21),(s*.018,s*.02,s*.02),'ivory','head',c,5,3))
        if cat:
            p.append(tube('BrowStripe',[(sg*s*.32,hy-s*.66,hz+s*.49),(sg*s*.57,hy-s*.51,hz+s*.38)],[s*.035,s*.055],'stripe',['head']*2,c,n=5))
    add('Head',p,'HEAD')
    p=[ellipsoid('LowerJaw',(0,hy-s*.58,hz-s*.53),(s*.44,s*(.68 if cat else .83),s*.14),'fur_light','jaw',c,10,5)]
    if not deer:
        for sg in [-1,1]:p.append(tube('Fang',[(sg*s*.25,hy-s*.84,hz-s*.36),(sg*s*.25,hy-s*.84,hz-s*.60)],[s*.045,.001],'ivory',['head']*2,c,n=5))
    add('Jaw',p,'JAW')
    p=[]
    for side,sg in [('L',1),('R',-1)]:
        a=r.data.bones['ear.'+side].head_local;b=r.data.bones['ear.'+side].tail_local
        if cat:
            p.append(ellipsoid('Ear',tuple(a+Vector((sg*.02,.008,.025))),(s*.30,s*.14,s*.31),'stripe','ear.'+side,c,8,5))
            p.append(ellipsoid('EarInside',tuple(a+Vector((sg*.02,-s*.12,.025))),(s*.19,.008,s*.20),'fur_light','ear.'+side,c,8,4))
        else:
            tip=tuple(a+Vector((sg*s*(.85 if deer else .25),.01,s*(.65 if deer else 1.2))))
            p.append(tube('Ear',[tuple(a),tuple(a.lerp(Vector(tip),.5)),tip],[(s*.20,s*.13),(s*.30,s*.12),(.004,.004)],fur,['ear.'+side]*3,c,n=6))
    add('Ears',p,'EARS')
    for end in ['F','H']:
        for side in ['L','R']:
            bn=[end+'_'+seg+'.'+side for seg in ['upper','lower','meta','paw']]
            pts=[tuple(r.data.bones[n].head_local) for n in bn]
            # Additional rings preserve definition at each articulation.
            ps=[pts[0],tuple(Vector(pts[0]).lerp(Vector(pts[1]),.45)),pts[1],tuple(Vector(pts[1]).lerp(Vector(pts[2]),.55)),pts[2],pts[3]]
            scale=.72 if deer else 1
            rr=[w*(.61 if end=='H' else .45),w*.40,w*.28,w*.23,w*.20,w*.16]
            rr=[v*scale for v in rr];rw=[{bn[0]:1},{bn[0]:1},{bn[0]:.5,bn[1]:.5},{bn[1]:1},{bn[1]:.5,bn[2]:.5},{bn[2]:1}]
            p=[tube('Leg',ps,rr,fur,None,c,n=8,ring_weights=rw)]
            foot=Vector(pts[3]);foot.y-=s*.20;foot.z=.047
            if deer:
                for sg in [-1,1]:
                    p.append(box('ClovenHoof',tuple(foot+Vector((sg*w*.10,0,0))),(w*.17,s*.55,.084),'leather_dark',bn[3],c,bevel=.006))
            else:
                p.append(ellipsoid('Paw',tuple(foot),(w*.31,s*.46,.06 if cat else h*.055),'fur_light' if species=='Fox' else fur,bn[3],c,8,5))
                if cat:
                    for j in [-1,0,1]:p.append(ellipsoid('Toe',tuple(foot+Vector((j*w*.16,-s*.25,0))),(w*.10,s*.19,.035),fur,bn[3],c,6,4))
            add('Leg_'+end+'_'+side,p,end+'_LEG_'+side)
    p=[];ps=[tuple(r.data.bones['tail_01'].head_local)]+[tuple(r.data.bones['tail_%02d'%(i+1)].tail_local) for i in range(4)]
    radii=[w*(.43 if species=='Fox' else .23)]*3+[w*.21,.009]
    rw=[{'tail_01':1},{'tail_01':.5,'tail_02':.5},{'tail_02':.5,'tail_03':.5},{'tail_03':.5,'tail_04':.5},{'tail_04':1}]
    p.append(tube('Tail',ps,radii,fur,None,c,n=8,ring_weights=rw))
    if species=='Fox':p.append(ellipsoid('TailTip',tuple(Vector(ps[-1]).lerp(Vector(ps[-2]),.45)),(w*.22,cfg['tail']*.13,w*.22),'fur_light','tail_04',c,8,5))
    add('Tail',p,'TAIL')
    if deer:
        p=[]
        for sg in [-1,1]:
            a=Vector((sg*s*.48,hy+.045,hz+s*.66));b=a+Vector((sg*.12,.10,.30));d=b+Vector((sg*.14,.08,.20))
            p.append(tube('Antler',[tuple(a),tuple(b),tuple(d)],[.027,.021,.005],'wood_light',['head']*3,c,n=6))
            for u in [.4,.75]:
                base=a.lerp(d,u);tip=base+Vector((-sg*.04,-.14,.14));p.append(tube('Tine',[tuple(base),tuple(tip)],[.015,.002],'wood_light',['head']*2,c,n=5))
        add('Antlers',p,'HEAD_ACCESSORY')
    animate(r,cfg['family'],animal_sampler)
    obs=list(modules.values());path='Animals/'+species+'.glb';export(path,[r]+obs);register(species,'quadruped',path,obs,rig=cfg['family'],behavior=cfg['behavior'],modules=list(modules))
    for name,o in modules.items():
        export('Animals/'+species+'_Modules/'+name+'.glb',[r,o],False) if (ROOT/('Animals/'+species+'_Modules')).exists() else None
    # Save all modules in the native source; individual GLBs are added in the final module pass.
    select([r]+obs);apply_pose(r,{});save('Animals/'+species+'_Modular.blend')
    preview('Previews/'+species+'.png',center=(0,0,h*(.88 if deer else .67)),distance=3,ortho=max(L+cfg['tail']+.6,h*(2.5 if deer else 2)),size=(1200,950))
    if r.animation_data is None:r.animation_data_create()
    r.animation_data.action=bpy.data.actions.get('Run');bpy.context.scene.frame_set(7)
    preview('Previews/'+species+'_Run.png',center=(0,0,h*(.88 if deer else .67)),distance=3,ortho=max(L+cfg['tail']+.6,h*(2.5 if deer else 2)),size=(1200,950))
    r.animation_data.action=None;apply_pose(r,{})
    module_dir=ROOT/('Animals/'+species+'_Modules');module_dir.mkdir(exist_ok=True)
    for name,o in modules.items():
        module_path='Animals/'+species+'_Modules/'+name+'.glb'
        export(module_path,[r,o],False)
        register(species+'_'+name,'animal_module',module_path,[o],rig=cfg['family'],slot=o['slot'])
    MANIFEST['rigs'][cfg['family']]={'bones':len(r.data.bones),'bone_names':[b.name for b in r.data.bones],'modules':list(modules),'hind_leg_chain':'hip -> stifle (forward) -> hock (rearward) -> ankle -> paw/hoof','part_interchangeability':'Within this species/bind contract only. Different species require retargeting and reskinning.'}
    print('ANIMAL_COMPLETE',species,flush=True)

def procedural_materials():
    tex_specs={
      'wood':('#80705a','wood'), 'concrete':('#8e9288','concrete'),
      'steel':('#667779','steel'), 'soil':('#70624d','soil'),
      'bark':('#675343','bark'), 'grass':('#667148','grass'),
      'rock':('#787f78','rock'),'canvas':('#8c8970','canvas')}
    n=256
    for key,(h,kind) in tex_specs.items():
        base=[int(h[i:i+2],16)/255 for i in (1,3,5)];pixels=[];rough=[]
        rg=random.Random(128+len(key));waves=[(rg.randint(1,18),rg.randint(1,18),rg.uniform(0,2*pi),1/(i+2)) for i in range(7)]
        for y in range(n):
            v=y/n
            for x in range(n):
                u=x/n
                torus=Vector((cos(2*pi*u)*(2+cos(2*pi*v)),sin(2*pi*u)*(2+cos(2*pi*v)),sin(2*pi*v)))
                low=noise.noise(torus*4)*.065+noise.noise(torus*27)*.018
                grain=sin(2*pi*(u*37+sin(v*2*pi)*.3))*.045 if kind in ['wood','bark'] else 0
                woven=(sin(2*pi*u*64)*sin(2*pi*v*64))*.025 if kind=='canvas' else 0
                stain=max(0,noise.noise(torus*3)-.16)*.16 if kind=='steel' else 0
                d=low+grain+woven-stain
                colors=[linear(max(.01,min(.98,c+d))) for c in base]
                pixels.extend([*colors,1])
                rr=max(.1,min(1,PALETTE[key][1]+d*.35));rough.extend([rr,rr,rr,1])
        img=bpy.data.images.new('TEX_'+key+'_BaseColor',n,n,alpha=False)
        img.pixels.foreach_set(pixels);img.filepath_raw=str(ROOT/('Materials/Textures/'+key+'_BaseColor.png'));img.file_format='PNG';img.save();img.pack()
        ri=bpy.data.images.new('TEX_'+key+'_Roughness',n,n,alpha=False);ri.colorspace_settings.name='Non-Color'
        ri.pixels.foreach_set(rough);ri.filepath_raw=str(ROOT/('Materials/Textures/'+key+'_Roughness.png'));ri.file_format='PNG';ri.save();ri.pack()
        m=mat(key);nt=m.node_tree;p=nt.nodes.get('Principled BSDF')
        node=nt.nodes.new('ShaderNodeTexImage');node.image=img;node.extension='REPEAT';nt.links.new(node.outputs['Color'],p.inputs['Base Color'])
        rn=nt.nodes.new('ShaderNodeTexImage');rn.image=ri;rn.extension='REPEAT';nt.links.new(rn.outputs['Color'],p.inputs['Roughness'])
        m['texture_tiling']='1 UV unit = 1 meter; periodic procedural albedo and roughness';m['texture_resolution']='256 x 256'
    # Named flat materials remain deliberately untextured on characters and fur.
    (ROOT/'Materials/palette.json').write_text(json.dumps({k:{'hex_srgb':v[0],'roughness':v[1]} for k,v in PALETTE.items()},indent=2),encoding='utf8')
    print('MATERIALS_COMPLETE',flush=True)

def make_settlement():
    clean();c=coll('SETTLEMENT_2m_Modules');assets={}
    def add(name,parts,snap='2m_grid'):
        o=join(parts,name);o['snap_contract']=snap;o['units']='meters';o['asset_role']='static_module';assets[name]=o;return o
    add('Floor_Concrete_2x2',[box('Slab',(0,0,.055),(2,2,.11),'concrete',c=c,bevel=.012)])
    add('Floor_Timber_2x2',[box('Frame',(0,0,.06),(2,2,.12),'wood',c=c)] + [box('Plank',(-.875+i*.25,0,.137),(.242,2,.035),'wood_light' if i%3==0 else 'wood',c=c) for i in range(8)])
    add('Wall_Concrete_2m',[box('Wall',(0,0,1.3),(2,.18,2.6),'concrete',c=c,bevel=.014),box('Base',(0,-.025,.12),(2,.21,.24),'rock',c=c)])
    p=[box('Stud',(x,0,1.3),(.12,.16,2.6),'wood',c=c) for x in [-.94,0,.94]]
    p += [box('Rail',(0,0,z),(2,.16,.13),'wood',c=c) for z in [.065,1.25,2.535]]
    p += [box('Cladding',(0,.04,.13+i*.26),(1.85,.12,.247),'wood_light' if i%4==0 else 'wood',c=c) for i in range(10)]
    add('Wall_Timber_2m',p)
    add('Wall_Doorway_2m',[box('Left',(-.76,0,1.3),(.48,.18,2.6),'concrete',c=c),box('Right',(.76,0,1.3),(.48,.18,2.6),'concrete',c=c),
        box('Lintel',(0,0,2.36),(1.04,.18,.48),'concrete',c=c),
        box('FrameL',(-.49,-.05,1.06),(.09,.22,2.12),'wood',c=c),box('FrameR',(.49,-.05,1.06),(.09,.22,2.12),'wood',c=c)])
    add('Wall_Window_2m',[box('Bottom',(0,0,.46),(2,.18,.92),'concrete',c=c),box('Top',(0,0,2.23),(2,.18,.74),'concrete',c=c),
        box('Side',(-.76,0,1.39),(.48,.18,.94),'concrete',c=c),box('Side',(.76,0,1.39),(.48,.18,.94),'concrete',c=c),
        box('Sill',(0,-.08,.92),(1.13,.31,.09),'wood',c=c)])
    add('Post_Timber_2p6m',[box('Post',(0,0,1.3),(.18,.18,2.6),'wood',c=c,bevel=.012)],'0.1m_grid')
    add('Beam_Timber_2m',[box('Beam',(0,0,.09),(2,.18,.18),'wood',c=c,bevel=.009)])
    for side,sg in [('L',-1),('R',1)]:
        vs=[(0,-1,0),(sg*2,-1,1.05),(sg*2,1,1.05),(0,1,0),(0,-1,-.09),(sg*2,-1,.96),(sg*2,1,.96),(0,1,-.09)]
        p=[mesh('RoofPanel',vs,[(0,1,2,3),(4,7,6,5),(0,4,5,1),(1,5,6,2),(2,6,7,3),(3,7,4,0)],'steel',c=c)]
        for j in range(9):
            y=-.95+j*.235;p.append(tube('RoofRib',[(0,y,.012),(sg*2,y,1.062)],[.025,.025],'steel',c=c,n=5))
        add('Roof_Slope_'+side+'_2x2',p)
    add('Roof_Gable_4m',[mesh('Gable',[(-2,-.09,0),(2,-.09,0),(0,-.09,1.05),(-2,.09,0),(2,.09,0),(0,.09,1.05)],
      [(0,2,1),(3,4,5),(0,1,4,3),(1,2,5,4),(2,0,3,5)],'wood',c=c)],'2m_grid')
    add('Roof_Flat_2x2',[box('Sheet',(0,0,.045),(2.02,2.02,.09),'steel',c=c),box('Lip',(0,-1,.11),(2.02,.06,.15),'rust',c=c)])
    add('Door_Timber_0p92x2p05',[box('Door',(.46,0,1.025),(.92,.065,2.05),'wood',c=c),
        box('Brace',(.46,-.043,.34),(.82,.03,.09),'steel',c=c),box('Brace',(.46,-.043,1.7),(.82,.03,.09),'steel',c=c),
        ellipsoid('Handle',(.79,-.058,1.02),(.03,.024,.025),'steel',c=c,segments=6,rings=4)],'hinge_at_local_origin')
    add('Window_Frame_1x1',[box('Frame',(0,0,-.47),(1.1,.12,.08),'wood',c=c),box('Frame',(0,0,.47),(1.1,.12,.08),'wood',c=c),
        box('Frame',(-.51,0,0),(.08,.12,1),'wood',c=c),box('Frame',(.51,0,0),(.08,.12,1),'wood',c=c),
        box('Cross',(0,-.02,0),(.05,.07,.93),'wood',c=c),box('Pane',(0,.025,0),(.95,.02,.89),'glass',c=c)],'window_center')
    add('Stairs_Concrete_3step',[box('Step',(0,-.30+i*.30,.1+i*.1),(1.2,.3,.2+i*.2),'concrete',c=c) for i in range(3)],'0.1m_grid')
    # Furniture retains plausible human dimensions and a ground pivot.
    p=[box('Top',(0,0,.76),(1.35,.75,.065),'wood',c=c,bevel=.008)]
    p += [box('Leg',(x,y,.37),(.065,.065,.74),'steel',c=c) for x in [-.58,.58] for y in [-.29,.29]]
    add('Table_Work',p,'free')
    p=[box('Seat',(0,0,.47),(.45,.44,.055),'wood',c=c,bevel=.007),box('Back',(0,.19,.72),(.45,.045,.43),'wood',c=c)]
    p += [box('Leg',(x,y,.23),(.045,.045,.46),'steel',c=c) for x in [-.17,.17] for y in [-.16,.16]]
    add('Chair',p,'free')
    add('Stool',[box('Seat',(0,0,.44),(.38,.38,.06),'wood',c=c)]+[box('Leg',(x,y,.21),(.05,.05,.42),'wood',c=c) for x in [-.13,.13] for y in [-.13,.13]],'free')
    add('Bed_Field',[box('Frame',(0,0,.31),(.86,1.98,.12),'steel',c=c),box('Mattress',(0,0,.43),(.8,1.90,.14),'canvas',c=c,bevel=.035),
        box('Blanket',(0,-.25,.512),(.81,1.32,.03),'olive',c=c),box('Pillow',(0,.70,.55),(.54,.30,.14),'cloth_gray',c=c,bevel=.035)]+
        [box('Leg',(x,y,.15),(.045,.045,.30),'steel',c=c) for x in [-.38,.38] for y in [-.86,.86]],'free')
    add('Shelf_Metal',[box('Shelf',(0,0,z),(1,.4,.045),'wood',c=c) for z in [.12,.55,.98,1.41]]+
        [box('Upright',(x,y,.76),(.04,.04,1.52),'steel',c=c) for x in [-.48,.48] for y in [-.18,.18]],'free')
    p=[box('Case',(0,0,.38),(.8,.55,.76),'wood',c=c,bevel=.01)]
    for x in [-.37,.37]:p.append(box('Strap',(x,0,.39),(.055,.57,.79),'steel',c=c))
    for z in [.10,.66]:p.append(box('Crossbar',(0,-.29,z),(.80,.04,.06),'wood_light',c=c))
    add('Supply_Crate',p,'0.1m_grid')
    p=[tube('Drum',[(0,0,.04),(0,0,.83)],[.28,.28],'steel',c=c,n=12)]
    for z in [.1,.4,.76]:p.append(tube('Rim',[(0,0,z),(0,0,z+.025)],[.293,.293],'rust',c=c,n=12))
    add('Barrel_Steel',p,'free')
    add('Jerrycan',[box('Tank',(0,0,.27),(.32,.17,.5),'olive',c=c,bevel=.035),box('Handle',(0,0,.57),(.16,.10,.07),'steel',c=c),
        tube('Spout',[(.11,0,.51),(.13,0,.57)],[.035,.028],'steel',c=c,n=8)],'free')
    add('Sack',[ellipsoid('Sack',(0,0,.28),(.22,.18,.28),'canvas',c=c,segments=10,rings=6),
        tube('TiedTop',[(0,0,.50),(0,0,.58)],[.09,.045],'canvas',c=c,n=8)],'free')
    add('Bucket',[tube('Pail',[(0,0,.025),(0,0,.32)],[.13,.17],'steel',c=c,n=12),
        tube('Handle',[(-.17,0,.30),(-.12,0,.52),(.12,0,.52),(.17,0,.30)],[.009]*4,'steel',c=c,n=5)],'free')
    add('Shovel',[tube('Shaft',[(0,0,.15),(0,0,1.14)],[.023,.023],'wood',c=c,n=8),box('Spade',(0,-.012,.15),(.23,.035,.30),'steel',c=c,bevel=.025),
        tube('Grip',[(-.09,0,1.1),(-.09,0,1.24),(.09,0,1.24),(.09,0,1.1)],[.018]*4,'wood',c=c,n=6)],'free')
    add('Axe',[tube('Handle',[(0,0,.03),(.03,0,.78)],[.025,.022],'wood',c=c,n=8),
        mesh('Head',[(-.04,-.055,.65),(.23,-.02,.63),(.23,.02,.81),(-.04,.055,.81),(-.04,.055,.65),(.23,.02,.63),(.23,-.02,.81),(-.04,-.055,.81)],
          [(0,1,2,3),(4,7,6,5),(0,4,5,1),(3,2,6,7),(1,5,6,2),(0,3,7,4)],'steel',c=c)],'free')
    add('Cooking_Pot',[tube('Pot',[(0,0,.03),(0,0,.22)],[.17,.18],'steel',c=c,n=12),
        ellipsoid('Lid',(0,0,.24),(.19,.19,.035),'steel',c=c,segments=12,rings=4),box('Knob',(0,0,.285),(.055,.055,.035),'wood',c=c)],'free')
    add('Lamp_Utility',[box('Base',(0,0,.025),(.20,.17,.05),'steel',c=c),tube('Cage',[(0,0,.05),(0,0,.29)],[.078,.078],'steel',c=c,n=8),
        ellipsoid('Glass',(0,0,.17),(.065,.065,.105),'ivory',c=c,segments=8,rings=5),box('Top',(0,0,.30),(.19,.16,.04),'steel',c=c)],'free')
    add('Fence_Timber_2m',[box('Post',(x,0,.65),(.12,.12,1.3),'wood',c=c) for x in [-.94,.94]]+
        [box('Rail',(0,0,z),(2,.09,.12),'wood',c=c) for z in [.45,.97]])
    add('Sandbag_Barrier',[ellipsoid('Sandbag',(x+(0.17 if row%2 else 0),0,.12+row*.18),(.31,.22,.14),'canvas',c=c,segments=8,rings=5) for row in range(3) for x in [-.65,-.22,.21,.64]],'0.1m_grid')
    add('Concrete_Barrier',[mesh('Barrier',[(-.8,-.28,0),(.8,-.28,0),(.8,.28,0),(-.8,.28,0),(-.8,-.11,.85),(.8,-.11,.85),(.8,.11,.85),(-.8,.11,.85)],
          [(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)],'concrete',c=c)],'free')
    # Export each pivot at its authored origin, then arrange the native library for browsing.
    for i,(name,o) in enumerate(assets.items()):
        path='Settlement/Modules/'+name+'.glb';export(path,[o],False);register(name,'settlement_module',path,[o],snap=o['snap_contract'])
        o.location=(i%6*3.1,i//6*3.2,0)
    select(list(assets.values()));save('Settlement/Settlement_Modular_Library.blend')
    preview('Previews/Settlement_Modules.png',center=(7.6,7.8,.5),distance=12,ortho=25,size=(1600,1200))
    # A concrete/timber home is a real assembly of the same modules.
    for o in assets.values():o.hide_render=True;o.hide_set(True)
    prefab=[]
    def instance(name,loc,rz=0):
        src=assets[name];o=bpy.data.objects.new('Home__'+name,src.data);c.objects.link(o);o.location=loc;o.rotation_euler.z=rz;prefab.append(o);return o
    for x in [-1,1]:
        for y in [-1,1]:instance('Floor_Timber_2x2',(x,y,0))
    for y in [-1,1]:
        instance('Wall_Window_2m',(-2,y,0),pi/2);instance('Wall_Timber_2m',(2,y,0),pi/2)
    for x in [-1,1]:
        instance('Wall_Doorway_2m' if x==-1 else 'Wall_Window_2m',(x,-2,0))
        instance('Wall_Concrete_2m',(x,2,0))
    for x in [-2,2]:
        for y in [-2,2]:instance('Post_Timber_2p6m',(x,y,0))
    for y in [-1,1]:
        instance('Roof_Slope_L_2x2',(2,y,2.6));instance('Roof_Slope_R_2x2',(-2,y,2.6))
    instance('Roof_Gable_4m',(0,-2,2.6));instance('Roof_Gable_4m',(0,2,2.6))
    instance('Window_Frame_1x1',(1,-2,1.39))
    for window_y in [-1,1]:instance('Window_Frame_1x1',(-2,window_y,1.39),pi/2)
    instance('Door_Timber_0p92x2p05',(-1.46,-2.04,.04),-.20)
    instance('Table_Work',(.75,.7,.18));instance('Chair',(.75,-.02,.18));instance('Bed_Field',(-1.35,.75,.18));instance('Supply_Crate',(2.6,.8,0))
    instance('Barrel_Steel',(2.55,1.65,0));instance('Lamp_Utility',(.8,.7,.98));instance('Stairs_Concrete_3step',(-1,-2.55,-.36))
    export('Settlement/Home_Prefab.glb',prefab,False);register('Home_Prefab','settlement_prefab','Settlement/Home_Prefab.glb',prefab)
    select(prefab);save('Settlement/Home_Prefab.blend')
    preview('Previews/Home_Prefab.png',center=(0,0,1.4),distance=8,ortho=8,size=(1400,1100))
    print('SETTLEMENT_COMPLETE',len(assets),flush=True)

def make_nature():
    clean();c=coll('NATURE_ModularLibrary');assets={}
    def add(name,parts,kind='nature_module'):
        o=join(parts,name);o['asset_role']=kind;o['units']='meters';assets[name]=o;return o
    for variant in range(3):
        rg=random.Random(83+variant);height=3.3+variant*.55
        p=[tube('Trunk',[(0,0,0),(.07,0,height*.45),(-.07,.04,height*.83),(0,0,height)],[.20,.15,.095,.025],'bark',c=c,n=8)]
        for j in range(7):
            a=j*2.4;z=height*(.45+j*.055);end=Vector((cos(a)*(1.0+rg.random()*.3),sin(a)*(1.0+rg.random()*.3),z+.65))
            p.append(tube('Branch',[(.02,0,z),tuple(end)],[.075,.025],'bark',c=c,n=6))
            p.append(ellipsoid('Crown',tuple(end+Vector((0,0,.2))),(.90,.84,.72),'leaf' if j%2 else 'grass_dark',c=c,segments=9,rings=5))
        add('Tree_Broadleaf_%02d'%(variant+1),p)
    for variant in range(2):
        height=3.8+variant*1.1;p=[tube('Trunk',[(0,0,0),(0,0,height)],[.16,.035],'bark',c=c,n=8)]
        for j in range(5):
            z=.8+j*.52*height/4;radius=1.0*(1-j*.15)*(1+.2*variant);n=10;vs=[]
            for k in range(n):
                a=2*pi*k/n;vs.append((cos(a)*radius,sin(a)*radius,z+(.08 if k%2 else -.06)))
            vs.append((0,0,z+1.15*height/4));fs=[(k,(k+1)%n,n) for k in range(n)];fs.append(tuple(reversed(range(n))))
            p.append(mesh('NeedleCrown',vs,fs,'pine' if j%2 else 'grass_dark',c=c))
        add('Tree_Pine_%02d'%(variant+1),p)
    add('Tree_Dead',[tube('Trunk',[(0,0,0),(.08,0,1.7),(-.08,0,3.2)],[.19,.12,.025],'bark',c=c,n=7),
        tube('Branch',[(0,0,1.5),(.75,.15,2.2),(1.02,.2,2.55)],[.09,.045,.01],'bark',c=c,n=6),
        tube('Branch',[(.02,0,2.3),(-.6,-.3,2.8)],[.065,.014],'bark',c=c,n=6)])
    add('Tree_Stump',[tube('Stump',[(0,0,0),(0,0,.46)],[.30,.24],'bark',c=c,n=9),
        tube('CutTop',[(0,0,.46),(0,0,.465)],[.225,.225],'wood_light',c=c,n=9)])
    add('Log_Fallen',[tube('Log',[(-1,0,.22),(1,0,.22)],[.21,.18],'bark',c=c,n=10),
        tube('End',[(-1.004,0,.22),(-1.01,0,.22)],[.193,.193],'wood_light',c=c,n=10)])
    for variant in range(3):
        rg=random.Random(93+variant);p=[]
        for j in range(5+variant):
            a=j*2.4;center=(cos(a)*.3,sin(a)*.3,.35+rg.random()*.24)
            p.append(ellipsoid('Bush',center,(.43,.38,.35),'leaf' if j%2 else 'grass_dark',c=c,segments=8,rings=5))
        add('Bush_%02d'%(variant+1),p)
    for variant in range(3):
        rg=random.Random(60+variant);vs=[];fs=[]
        for j in range(19+variant*7):
            x=rg.uniform(-.46,.46);y=rg.uniform(-.46,.46);height=rg.uniform(.12,.40);a=rg.uniform(0,pi);dx=.025*cos(a);dy=.025*sin(a);off=len(vs)
            vs += [(x-dx,y-dy,0),(x+dx,y+dy,0),(x+dx*.5+.035,y+dy*.5,height*.68),(x+.045,y,height)]
            fs += [(off,off+1,off+2),(off,off+2,off+3)]
            # Crossed foliage keeps a readable tuft from the quarter-view camera.
            off=len(vs);vs += [(x+dy,y-dx,0),(x-dy,y+dx,0),(x-.03,y+.02,height*.85)];fs += [(off,off+1,off+2)]
        add('Grass_Patch_%02d'%(variant+1),[mesh('Grass',vs,fs,'grass' if variant%2==0 else 'grass_dark',c=c)])
    for variant in range(4):
        rg=random.Random(108+variant);o=ellipsoid('Rock',(0,0,.27+variant*.10),(.43+variant*.19,.35+variant*.14,.36+variant*.11),'rock',c=c,segments=8,rings=5)
        for v in o.data.vertices:
            v.co.x*=rg.uniform(.83,1.16);v.co.y*=rg.uniform(.86,1.14);v.co.z=max(.0,v.co.z*rg.uniform(.88,1.13))
        add('Rock_%02d'%(variant+1),[o])
    for kind,material in [('Soil','soil'),('Grass','grass'),('Rock','rock'),('Concrete','concrete')]:
        n=9;vs=[];fs=[]
        for j in range(n):
            for i in range(n):
                x=-1+2*i/(n-1);y=-1+2*j/(n-1)
                z=0 if kind=='Concrete' else .035*sin(pi*i/(n-1))*sin(pi*j/(n-1))*sin(i*1.4+j*1.9)
                vs.append((x,y,z))
        for j in range(n-1):
            for i in range(n-1):
                k=j*n+i;fs += [(k,k+1,k+n+1),(k,k+n+1,k+n)]
        o=mesh('Ground',vs,fs,material,c=c);o['tile_size_m']=2;o['edge_height_m']=0
        add('Terrain_'+kind+'_2x2',[o],'terrain_tile')
    add('Path_2x2',[box('Path',(0,0,-.025),(2,2,.05),'soil',c=c)]+[ellipsoid('Pebble',(RNG.uniform(-.8,.8),RNG.uniform(-.8,.8),.025),(.055,.045,.025),'rock',c=c,segments=6,rings=3) for _ in range(10)],'terrain_tile')
    add('Reed_Patch',[tube('Reed',[(RNG.uniform(-.25,.25),RNG.uniform(-.25,.25),0),(RNG.uniform(-.25,.25),RNG.uniform(-.25,.25),RNG.uniform(.5,.8))],[.012,.007],'grass',c=c,n=5) for _ in range(13)])
    for i,(name,o) in enumerate(assets.items()):
        path='Nature/Modules/'+name+'.glb';export(path,[o],False);register(name,o['asset_role'],path,[o])
        o.location=((i%6)*3.5,(i//6)*3.6,0)
    select(list(assets.values()));save('Nature/Nature_Modular_Library.blend')
    preview('Previews/Nature_Modules.png',center=(8.5,7,1.1),distance=14,ortho=27,size=(1600,1200))
    print('NATURE_COMPLETE',len(assets),flush=True)

def document():
    transitions={
      'schema_version':1,'fps':FPS,'applies_to':list(MANIFEST['rigs']),
      'locomotion':{'clip_type':'in_place','speed_mps_suggested':{'human_walk':.968,'human_run':2.303,'human_crouch_walk':.194,'tiger_walk':.738,'tiger_run':2.786,'wolf_walk':.601,'wolf_run':2.266,'fox_walk':.389,'fox_run':1.467,'deer_walk':.581,'deer_run':2.191},'synchronize':'Match cyclic normalized phase on gait changes; adjust speed or stride to limit foot sliding.'},
      'rules':[
        {'from':'Idle','to':'Walk','sequence':['Walk_Start','Walk'],'crossfade_seconds':.14,'exit_normalized':1,'note':'Walk_Start ends at Walk phase 0.'},
        {'from':'Walk','to':'Idle','sequence':['Walk_Stop','Idle'],'crossfade_seconds':.16,'entry_phase':0,'note':'Trigger stop near Walk phase 0; otherwise blend to stop with matching foot support.'},
        {'from':'Walk','to':'Run','sequence':['Run'],'crossfade_seconds':.18,'phase_sync':True},
        {'from':'Run','to':'Walk','sequence':['Walk'],'crossfade_seconds':.18,'phase_sync':True},
        {'from':'Idle','to':'Crouch','sequence':['Stand_To_Crouch','Crouch'],'crossfade_seconds':.10,'exit_normalized':1},
        {'from':'Crouch','to':'Idle','sequence':['Crouch_To_Stand','Idle'],'crossfade_seconds':.10,'exit_normalized':1},
        {'from':'Crouch','to':'Crouch_Walk','sequence':['Crouch_Walk'],'crossfade_seconds':.15},
        {'from':'Crouch_Walk','to':'Run','sequence':['Crouch_To_Stand','Run'],'crossfade_seconds':.18},
        {'from':'grounded','to':'Jump','sequence':['Jump'],'crossfade_seconds':.08,'exit_normalized':1,'note':'Full Jump includes authored vertical root displacement. Collision/physics is external.'},
        {'from':'grounded','to':'physics_jump','sequence':['Jump_Start','Jump_Air','Jump_Land'],'crossfade_seconds':.10,'note':'Alternative to full Jump. Jump_Air loops in place; enter Jump_Land on ground contact. Use game physics for vertical trajectory; suppress/extract authored root Z in Start/Land.'},
        {'from':'grounded','to':'Attack','sequence':['Attack','previous_grounded_state'],'crossfade_seconds':.08,'exit_normalized':.93,'note':'Human right-hand strike; tiger forepaw swipe/bite; wolf/fox bite; deer antler/head strike.'},
        {'from':'Idle','to':'turn_left','sequence':['Turn_L90','Idle'],'crossfade_seconds':.12,'root_yaw_degrees':90},
        {'from':'Idle','to':'turn_right','sequence':['Turn_R90','Idle'],'crossfade_seconds':.12,'root_yaw_degrees':-90}
      ],
      'turn_blend':{'reference':'Pose_Neutral','left':'Pose_Turn_L45','right':'Pose_Turn_R45','parameter_degrees':[-45,0,45],'interpretation':'Absolute local pose references; blend from neutral or calculate additive deltas relative to it. Feet remain in stance. Above 45 degrees, schedule a turn step.','root_rotation':'Turn_L90/R90 animate the root bone. Extract yaw into actor transform or let the root bone drive it; do not apply both.'},
      'events_normalized':{'Attack':{'windup_end':.12,'impact':.47,'recovery':.82},'Jump':{'takeoff':.20,'apex':.475,'landing':.76},'Walk_human':{'foot_L':0,'foot_R':.5},'Walk_quadruped':{'front_L':0,'hind_R':.75,'front_R':.5,'hind_L':.25}},
      'implementation_boundary':'Data and authored assets only. No engine code or animation graph is installed.'}
    (ROOT/'Documentation/animation_transitions.json').write_text(json.dumps(transitions,ensure_ascii=False,indent=2),encoding='utf8')
    readme="""# Frontier LowPoly Kit v1
Blender 5.2.2 LTS에서 제작한 전쟁 이후 숲·전초기지·민간 거주지용 로우폴리 리소스 초안입니다.

## 바로 열기
- Characters/Human_Modular_Library.blend: 7등신 인간 기본 골격과 교체 가능한 파츠 전체.
- Characters/Presets/*.glb: 주인공 지휘관, 정찰병, 주민, 작업자 조합 예시.
- Animals/*_Modular.blend 및 *.glb: 호랑이, 늑대, 여우, 사슴. 종별 골격과 분리된 신체 파츠.
- Settlement/Settlement_Modular_Library.blend: 건축과 집기 모듈.
- Settlement/Home_Prefab.blend: 같은 부품으로 조립한 주거 예시.
- Nature/Nature_Modular_Library.blend: 나무, 덤불, 풀, 바위, 지면 타일.
- Previews: Blender 렌더 미리보기.
- Materials/Textures: 타일 반복 가능한 256×256 BaseColor/Roughness PNG. 색상은 sRGB, Roughness는 Non-Color.
- Documentation/animation_transitions.json: 클립 조합, 전환 시간, 회전 기준 포즈, 이벤트와 루트 이동 처리.
- Documentation/asset_manifest.json: 전체 파일·메시 수·삼각형 수·골격·클립 목록.

## 조합 규약
1 Blender 단위 = 1m. Blender는 Z 위 / -Y 전방. GLB는 Y 위 / +Z 전방. GLB 내보내기에서 축을 변환합니다.
인간은 신장 1.82m, 머리 기준 0.26m인 7등신입니다. HEAD/FACE/HAIR/TORSO/ARM_L/ARM_R/PELVIS/LEG_L/LEG_R/FOOT/HAND 슬롯으로 조합합니다.
모든 인간 파츠는 HUM_7H_v1의 동일한 바인드 자세와 본 이름을 사용합니다. 하나의 골격에 같은 슬롯의 메시 하나씩 연결하고 웨이트를 보존하세요. Face와 Hair는 Head 위에 올리는 메시입니다.
의상 몸통과 좌우 소매는 같은 의상 종류를 사용합니다. TacticalVest는 FieldJacket용 겹옷입니다. Cap과 머리카락이 겹치면 머리카락을 숨기세요.
개별 모듈 GLB는 바인드 골격과 해당 파츠를 포함하며 애니메이션은 없습니다. 완성 조합 GLB와 각 원본 blend에 애니메이션이 있습니다.
동물은 BODY/NECK/HEAD/JAW/EARS/앞·뒷다리 네 개/TAIL과 사슴 ANTLERS를 분리했습니다.
서로 다른 종은 바인드 위치가 달라 직접 교환할 수 없습니다. 같은 종의 파츠만 직접 교환하고, 종을 넘는 변형은 리타기팅·리깅이 필요합니다.
앞다리는 팔꿈치·손목, 뒷다리는 앞으로 나온 무릎과 뒤로 꺾인 비절·발목을 별도 본으로 표현했습니다. 사슴은 갈라진 발굽을 사용합니다.
위험/중립/순함은 asset_manifest의 기본 분류이며, AI 행동은 포함하지 않습니다.

## 애니메이션
30fps. 모든 골격은 20개 Action을 포함합니다.
Pose_Neutral / Pose_Turn_L45 / Pose_Turn_R45 / Pose_Crouch
Idle / Walk / Run / Turn_L90 / Turn_R90 / Attack
Jump / Jump_Start / Jump_Air / Jump_Land
Crouch / Crouch_Walk / Stand_To_Crouch / Crouch_To_Stand / Walk_Start / Walk_Stop
걷기·뛰기는 제자리 순환입니다. root의 수평 이동은 없습니다. 게임 이동 속도는 스트라이드에 맞게 조정해야 합니다.
회전은 root 본의 회전을 포함합니다. 캐릭터 전체 회전과 중복 적용하지 마세요.
전체 Jump에는 root 본의 수직 이동이 포함됩니다. 물리 점프를 사용한다면 전체 Jump 대신 분할 클립을 연결하고 root Z를 추출·제거하세요.
Pose_*는 짧은 고정 자세입니다. 기본 중립 자세를 기준으로 좌우 45도 자세를 보간하거나 가산 레이어 차이를 계산합니다.
일반 인간은 오른손 공격입니다. 무기 메시·무기별 공격과 전투 판정은 포함하지 않았습니다.
블렌더 Action 편집기에서 골격을 선택하고 Action을 바꾸어 확인합니다. 원본은 A포즈/중립으로 저장되어 있습니다.
애니메이션은 절차적으로 키를 만든 1차 블로킹입니다. 실제 카메라와 게임 속도에서 접지·관절·클립 전환에 대한 최종 조정이 필요합니다.

## 건축과 자연
건축: 벽 폭 2m, 높이 2.6m, 바닥 2×2m. 기본 모듈 중심 X/Y, 바닥 Z=0. 벽은 X방향 길이이고 Y축 회전이 아니라 Z축 90도로 측벽을 만듭니다.
문은 힌지 로컬 원점. 창 프레임은 창 중심 원점. Roof_Slope는 낮은 처마 로컬 X=0에서 ±X 2m 방향으로 1.05m 상승합니다.
가구는 실제 사람 크기에 맞춘 독립 배치용. 콜라이더·문 스크립트·대화·아이템 기능은 포함하지 않습니다.
지면은 2×2m이고 외곽 높이는 0이므로 나란히 연결할 수 있습니다. 잔디는 양면 재질의 삼각형 잎입니다.
나무는 침엽수/활엽수/고사목, 그루터기와 통나무를 포함합니다. 텍스처는 GLB 안에 포함하고 blend에도 패킹했습니다.

## 범위와 검증
제작·저장은 Resource/Frontier_LowPoly_Kit_v1 안에서만 진행했습니다. 게임 코드·빌드·렌더러는 변경하지 않았습니다.
GLB는 표준 전달용이며 프로젝트 로더와 연결하는 작업은 별도입니다.
블렌더 메시·웨이트·클립, GLB의 skin/animation 채널 및 파일 재열기 결과는 Documentation/validation_report.json을 참고하세요.
_Source/author_kit.py는 Blender 내부 제작을 재현하는 원본이며 게임용 코드가 아닙니다.
"""
    (ROOT/'README_ko.md').write_text(readme,encoding='utf8')
    MANIFEST['status']='authored'
    (ROOT/'Documentation/asset_manifest.json').write_text(json.dumps(MANIFEST,ensure_ascii=False,indent=2),encoding='utf8')

def main():
    make_humans()
    for species in ANIMALS:make_animal(species)
    procedural_materials()
    make_settlement();make_nature()
    # A standalone packed material library.
    clean();c=coll('MATERIAL_Palette')
    for i,key in enumerate(PALETTE):
        o=box('Swatch_'+key,((i%8)*.8,(i//8)*.8,.08),(.7,.7,.16),key,c=c,bevel=.025);o['material_name']=key
    save('Materials/Frontier_Materials.blend')
    preview('Previews/Material_Palette.png',center=(2.8,1.7,0),distance=6,ortho=8,size=(1200,900))
    document();print('PACK_BUILD_COMPLETE',str(ROOT),flush=True)
if __name__=='__main__':
    try:main()
    except Exception:
        traceback.print_exc()
        (ROOT/'Documentation/build_error.txt').write_text(traceback.format_exc(),encoding='utf8')
        raise

