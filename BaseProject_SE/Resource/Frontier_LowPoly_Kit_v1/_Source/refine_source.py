from pathlib import Path
p=Path(__file__).with_name('author_kit.py')
s=p.read_text(encoding='utf-8-sig')
def change(old,new):
    global s
    if old not in s:raise RuntimeError('Patch target not found: '+old[:100])
    s=s.replace(old,new)
change('from mathutils import Vector, Quaternion','from mathutils import Vector, Quaternion, noise')
change("FPS=30","FPS=30\nbpy.context.preferences.filepaths.save_version=0")
change("if angry:brow.rotation_euler.y=sign*-.18","""if angry:
                center=Vector((sign*.041,-.09,1.745));rotation=qy(sign*-.18)
                for vertex in brow.data.vertices:vertex.co=center+rotation@(vertex.co-center)""")
change("            add('Sleeve_'+variant+'_'+side,p,variant=='FieldJacket','ARM_'+side)","""            p.append(ellipsoid('Shoulder',(sign*.197,0,1.425),(.091,.086,.085),material,'upper_arm.'+side,c,10,5))
            add('Sleeve_'+variant+'_'+side,p,variant=='FieldJacket','ARM_'+side)""")
change("for off in [-.014,.014]:vs.append((xx,yy+off*(.7+.3*cos(j)),zz))","""for off in [-.014,.014]:
                        origin=Vector((0,yy+off*(.7+.3*cos(j)),h*.86))
                        direction=Vector((sg*sin(angle),0,cos(angle))).normalized()
                        hit,location,normal,face_index=body.ray_cast(origin,direction)
                        vs.append(tuple(location+normal*.0018) if hit else (xx,origin.y,zz))""")
change("for name,o in modules.items():export('Animals/'+species+'_Modules/'+name+'.glb',[r,o],False)","""for name,o in modules.items():
        module_path='Animals/'+species+'_Modules/'+name+'.glb'
        export(module_path,[r,o],False)
        register(species+'_'+name,'animal_module',module_path,[o],rig=cfg['family'],slot=o['slot'])""")
# The native standing skeleton is retained; preview framing includes the deer's antlers.
change("center=(0,0,h*.67),distance=3,ortho=max(L+cfg['tail']+.6,h*2)","center=(0,0,h*(.88 if deer else .67)),distance=3,ortho=max(L+cfg['tail']+.6,h*(2.5 if deer else 2))")
change("floor=box('Preview_Ground',(center[0],center[1],-.055),(60,60,.1),'charcoal');added.append(floor)","""floor=box('Preview_Ground',(center[0],center[1],-.11),(100,100,.1),'charcoal');added.append(floor)
    d=bpy.data.lights.new('Preview_Sun','SUN');d.energy=1.2;d.angle=.15
    o=bpy.data.objects.new('Preview_Sun',d);sc.collection.objects.link(o);o.rotation_euler=(.35,-.45,-.4);added.append(o)""")
change("low=sum(sin(2*pi*(a*u+b*v)+c)*s for a,b,c,s in waves)*.045","""torus=Vector((cos(2*pi*u)*(2+cos(2*pi*v)),sin(2*pi*u)*(2+cos(2*pi*v)),sin(2*pi*v)))
                low=noise.noise(torus*4)*.065+noise.noise(torus*27)*.018""")
change("stain=sin(2*pi*(u*4-v*3))**10*.075 if kind=='steel' else 0","stain=max(0,noise.noise(torus*3)-.16)*.16 if kind=='steel' else 0")
change("    add('Roof_Flat_2x2',","""    add('Roof_Gable_4m',[mesh('Gable',[(-2,-.09,0),(2,-.09,0),(0,-.09,1.05),(-2,.09,0),(2,.09,0),(0,.09,1.05)],
      [(0,2,1),(3,4,5),(0,1,4,3),(1,2,5,4),(2,0,3,5)],'wood',c=c)],'2m_grid')
    add('Roof_Flat_2x2',""")
change("    instance('Door_Timber_0p92x2p05',","""    instance('Roof_Gable_4m',(0,-2,2.6));instance('Roof_Gable_4m',(0,2,2.6))
    instance('Window_Frame_1x1',(1,-2,1.39))
    for window_y in [-1,1]:instance('Window_Frame_1x1',(-2,window_y,1.39),pi/2)
    instance('Door_Timber_0p92x2p05',""")
change("def save(path):\n    bpy.ops.wm.save_as_mainfile", """def save(path):
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
    bpy.ops.wm.save_as_mainfile""")
p.write_text(s,encoding='utf8')
print('SOURCE_REFINED')

