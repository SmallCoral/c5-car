"""Simplify only preview meshes; native CAD and STEP geometry stay unchanged."""
from pathlib import Path
import json
import FreeCAD as App
import Mesh

M=Path(__file__).resolve().parents[1]
p=M/'preview/assembly-meshes.json';d=json.loads(p.read_text(encoding='utf-8'));changes=[]
for part in d['parts']:
    old=len(part['triangles'])
    if old<80000:continue
    target=70000 if part['name'].endswith('Wheel') else 60000
    mesh=Mesh.Mesh(([App.Vector(*v) for v in part['vertices']],[tuple(f) for f in part['triangles']]))
    mesh.decimate(.08,1-target/old)
    verts,faces=mesh.Topology
    part['vertices']=[[round(v.x,5),round(v.y,5),round(v.z,5)] for v in verts]
    part['triangles']=[list(f) for f in faces]
    changes.append({'name':part['name'],'triangles_before':old,'triangles_after':len(faces),'requested_error_tolerance_mm':.08})
p.write_text(json.dumps(d,ensure_ascii=False,separators=(',',':')),encoding='utf-8')
(M/'validation/preview-mesh-reduction.json').write_text(json.dumps({'native_cad_unchanged':True,'changes':changes},ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(json.dumps(changes),flush=True)
