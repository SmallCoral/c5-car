"""Refresh signed motion coordinates and placements without remeshing fixed geometry."""
from pathlib import Path
import json
import math
import FreeCAD as App
import Part
M=Path(__file__).resolve().parents[1]
P=json.loads((M/'parameters.yaml').read_text(encoding='utf-8'));B=P['beam'];C=P['chassis']
data=json.loads((M/'preview/assembly-meshes.json').read_text(encoding='utf-8'))
d=App.openDocument(str(M/'source/CoralC5_Framework.FCStd'));m=d.getObject('Motion')
if m.getTypeIdOfProperty('HingeX')!='App::PropertyDistance':
    m.removeProperty('HingeX');m.addProperty('App::PropertyDistance','HingeX','Geometry')
m.HingeX=B['hinge_x']
hz=C['plate_bottom']+C['thickness']+B['hinge_height_above_plate']
for name,pos,expr in [('A31_Beam',(B['hinge_x'],0,hz),'Motion.BeamAngle'),
    ('A41_Crank',(B['stepper_center_xz'][0],B['link_plane_y'],B['stepper_center_xz'][1]),'Motion.CrankAngle'),
    ('A42_Rod',(0,B['link_plane_y'],0),'Motion.RodAngle')]:
    g=d.getObject(name);g.setExpression('Placement.Rotation.Angle',None)
    if name=='A42_Rod':
        g.setExpression('Placement.Base.x',None);g.setExpression('Placement.Base.z',None)
    g.Placement=App.Placement(App.Vector(*pos),App.Rotation(App.Vector(0,-1,0),1))
    g.setExpression('Placement.Rotation.Angle',expr)
    if name=='A42_Rod':
        g.setExpression('Placement.Base.x','Motion.CrankTipX');g.setExpression('Placement.Base.z','Motion.CrankTipZ')
checks=[]
for angle in (-6,0,6):
    m.CommandAngle=angle;d.recompute()
    end=d.getObject('A42_Rod').getGlobalPlacement().multVec(App.Vector(B['rod_length'],B['rod_lateral_offset'],0))
    expected=d.getObject('A31_Beam').getGlobalPlacement().multVec(App.Vector(B['link_position'],B['link_plane_y']+B['rod_lateral_offset'],0))
    error=(end-expected).Length
    if not math.isfinite(error) or error>1e-6:raise RuntimeError('Motion refresh failed '+str(error))
    checks.append({'angle_deg':angle,'closure_mm':error})
m.CommandAngle=0;d.recompute();world=[]
for p in data['parts']:
    o=d.getObject(p['name']);p['placement']=list(o.getGlobalPlacement().Matrix.A)
    if not all(math.isfinite(v) for v in p['placement']):raise RuntimeError(p['name'])
    s=o.Shape.copy();s.Placement=o.getGlobalPlacement();world.append(s)
d.save()
Part.makeCompound(world).exportStep(str(M/'step/CoralC5_Framework.step'))
(M/'preview/assembly-meshes.json').write_text(json.dumps(data,ensure_ascii=False,separators=(',',':'),allow_nan=False),encoding='utf-8')
report=json.loads((M/'validation/model-build.json').read_text(encoding='utf-8'))
report['signed_hinge_coordinate_repair']={'property':'App::PropertyDistance','hinge_x_mm':float(m.HingeX),'poses':checks}
(M/'validation/model-build.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
App.closeDocument(d.Name);print(json.dumps(checks),flush=True)
