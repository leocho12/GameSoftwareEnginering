import bpy, json, struct, math, hashlib, traceback
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
manifest=json.loads((ROOT/'Documentation/asset_manifest.json').read_text(encoding='utf8'))
expected={a['name'] for a in manifest['animations']['HUM_7H_v1']}
report={'blender_version':bpy.app.version_string,'checks':[],'errors':[],'warnings':[],
 'scope':'Local Blender files and GLB structure, weights and animation round-trip only. Game import/playback is not tested.'}
def fail(file,message):report['errors'].append({'file':str(file),'message':message})
def check_glb(path):
    data=path.read_bytes();magic,version,length=struct.unpack_from('<III',data,0)
    assert magic==0x46546C67 and version==2 and length==len(data)
    offset=12;doc=None;binary=b''
    while offset<len(data):
        n,t=struct.unpack_from('<II',data,offset);chunk=data[offset+8:offset+8+n];offset+=8+n
        if t==0x4E4F534A:doc=json.loads(chunk)
        if t==0x004E4942:binary=chunk
    assert doc is not None
    def accessor(index):
        ac=doc['accessors'][index]
        if 'bufferView' not in ac:return []
        view=doc['bufferViews'][ac['bufferView']]
        typ={5120:'b',5121:'B',5122:'h',5123:'H',5125:'I',5126:'f'}[ac['componentType']]
        count={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4,'MAT2':4,'MAT3':9,'MAT4':16}[ac['type']]
        fmt='<'+typ*count;size=struct.calcsize(fmt);stride=view.get('byteStride',size);start=view.get('byteOffset',0)+ac.get('byteOffset',0)
        assert start+max(0,ac['count']-1)*stride+size<=len(binary)
        rows=[struct.unpack_from(fmt,binary,start+i*stride) for i in range(ac['count'])]
        if ac.get('normalized'):
            divisor={5120:127,5121:255,5122:32767,5123:65535}.get(ac['componentType'],1)
            rows=[tuple(max(-1,v/divisor) for v in row) for row in rows]
        return rows
    for i,ac in enumerate(doc.get('accessors',[])):
        if ac['componentType']==5126:
            assert all(math.isfinite(v) for row in accessor(i) for v in row),'Non-finite accessor'
    skins=doc.get('skins',[])
    mesh_nodes={n['mesh']:n for n in doc.get('nodes',[]) if 'mesh' in n}
    for mi,me in enumerate(doc.get('meshes',[])):
        for primitive in me['primitives']:
            attrs=primitive['attributes'];assert 'POSITION' in attrs
            if 'WEIGHTS_0' in attrs:
                assert 'JOINTS_0' in attrs
                vals=accessor(attrs['WEIGHTS_0']);joints=accessor(attrs['JOINTS_0'])
                for row in vals:assert abs(sum(row)-1)<.002,'Skin weights do not sum to one'
                node=mesh_nodes[mi];assert 'skin' in node
                joint_count=len(skins[node['skin']]['joints'])
                assert all(j<joint_count for row in joints for j in row),'Joint index outside skin'
    animated=(path.parent.name=='Presets') or path.parent==ROOT/'Animals'
    names=[a.get('name') for a in doc.get('animations',[])]
    if animated:
        assert set(names)==expected,('Missing animation clips',names)
        assert skins,'Animated asset has no skin'
    elif names:fail(path.relative_to(ROOT),'Unexpected animation on a standalone module')
    loops=[]
    for animation in doc.get('animations',[]):
        clip=animation['name'];isloop=clip in ['Idle','Walk','Run','Crouch','Crouch_Walk','Jump_Air']
        changed=False;duration=0
        for channel in animation['channels']:
            sample=animation['samplers'][channel['sampler']]
            times=accessor(sample['input']);values=accessor(sample['output'])
            assert len(times)==len(values)
            flat=[v[0] for v in times]
            assert all(x<y for x,y in zip(flat,flat[1:])),'Non-monotonic animation time'
            duration=max(duration,max(flat,default=0))
            if len(values)>1:
                if any(abs(a-b)>1e-5 for a,b in zip(values[0],values[len(values)//2])):changed=True
                if isloop:
                    first,last=values[0],values[-1]
                    if channel['target']['path']=='rotation':
                        error=min(sum((x-y)**2 for x,y in zip(first,last)),sum((x+y)**2 for x,y in zip(first,last)))
                    else:error=sum((x-y)**2 for x,y in zip(first,last))
                    assert error<1e-5,('Loop endpoints differ',clip,channel['target'])
        if clip in ['Walk','Run','Attack','Jump','Turn_L90','Turn_R90']:assert changed,('Motion has no change',clip)
        if isloop:loops.append(clip)
        assert duration>0
    return {'file':str(path.relative_to(ROOT)).replace('\\','/'),'format':'GLB','meshes':len(doc.get('meshes',[])),'skins':len(skins),'clips':names,'loop_endpoints_checked':loops,'sha256':hashlib.sha256(data).hexdigest(),'result':'pass'}

for path in sorted(ROOT.rglob('*.glb')):
    try:report['checks'].append(check_glb(path))
    except Exception as e:fail(path.relative_to(ROOT),repr(e))
print('GLB_STRUCTURE_CHECKED',len(report['checks']),len(report['errors']),flush=True)
blend_files=sorted(ROOT.rglob('*.blend'))
for path in blend_files:
    try:
        bpy.ops.wm.open_mainfile(filepath=str(path))
        rigs=[o for o in bpy.context.scene.objects if o.type=='ARMATURE']
        source_actions={a.name for a in bpy.data.actions}
        row={'file':str(path.relative_to(ROOT)).replace('\\','/'),'format':'BLEND','rigs':[],'meshes':0,'result':'pass'}
        for r in rigs:
            assert source_actions==expected,('Native action library mismatch',source_actions)
            bind={b.name:[round(v,7) for row in b.matrix_local for v in row] for b in r.data.bones}
            signature=hashlib.sha256(json.dumps(bind,sort_keys=True).encode()).hexdigest()
            meshes=[o for o in bpy.context.scene.objects if o.type=='MESH' and o.parent==r];minimum=1;maximum=0;zero=0
            for o in meshes:
                assert any(m.type=='ARMATURE' and m.object==r for m in o.modifiers),('Missing armature modifier',o.name)
                for v in o.data.vertices:
                    total=sum(g.weight for g in v.groups if o.vertex_groups[g.group].name in r.data.bones)
                    minimum=min(minimum,total);maximum=max(maximum,total)
                    if total<.99:zero+=1
                    assert abs(total-1)<.005,('Unnormalized vertex',o.name,v.index,total)
                assert all(math.isfinite(c) for v in o.data.vertices for c in v.co)
            row['rigs'].append({'name':r.name,'bones':len(r.data.bones),'clips':len(source_actions),'skin_weight_range':[round(minimum,6),round(maximum,6)],'unweighted_vertices':zero,'bind_signature':signature})
        row['meshes']=sum(o.type=='MESH' for o in bpy.context.scene.objects)
        assert row['meshes']>0
        for o in bpy.context.scene.objects:
            if o.type=='MESH':
                assert all(math.isfinite(c) for v in o.data.vertices for c in v.co)
                degenerate=sum(p.area<1e-11 for p in o.data.polygons)
                if degenerate:report['warnings'].append({'file':row['file'],'object':o.name,'degenerate_faces':degenerate})
        report['checks'].append(row)
    except Exception as e:fail(path.relative_to(ROOT),repr(e))

# Re-import every delivered standalone GLB into Blender. This checks a second reader
# rather than assuming an exporter-produced byte stream will load.
for path in sorted(ROOT.rglob('*.glb')):
    try:
        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.ops.import_scene.gltf(filepath=str(path),import_pack_images=True)
        meshes=[o for o in bpy.context.scene.objects if o.type=='MESH']
        assert meshes,'Round-trip has no meshes'
        animated=(path.parent.name=='Presets') or path.parent==ROOT/'Animals'
        if animated:
            imported={a.name for a in bpy.data.actions}
            assert expected.issubset(imported),('Round-trip missing clips',expected-imported)
            assert any(o.type=='ARMATURE' for o in bpy.context.scene.objects),'Round-trip lost skeleton'
        report['checks'].append({'file':str(path.relative_to(ROOT)).replace('\\','/'),'format':'BLENDER_GLB_IMPORT','meshes':len(meshes),'result':'pass'})
    except Exception as e:fail(path.relative_to(ROOT),'Round-trip: '+repr(e))
contacts=json.loads((ROOT/'Documentation/contact_checks.json').read_text(encoding='utf8'))
report['stance_contact_checks']=contacts
for family in contacts:
    for row in family['stance_contacts']:
        if row['stance_ankle_height_max_error_m']>.025:
            report['warnings'].append({'rig':family['rig'],'clip':row['clip'],'contact_error_m':row['stance_ankle_height_max_error_m']})
report['status']='passed' if not report['errors'] else 'failed'
report['summary']={'glb_files':len(list(ROOT.rglob('*.glb'))),'blend_files':len(blend_files),'checks':len(report['checks']),'errors':len(report['errors']),'warnings':len(report['warnings'])}
(ROOT/'Documentation/validation_report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
print('VALIDATION_COMPLETE',json.dumps(report['summary']),flush=True)
if report['errors']:raise RuntimeError(str(report['errors']))

