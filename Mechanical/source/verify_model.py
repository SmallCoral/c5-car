"""Read back the saved assembly; check poses, source identity and critical clearances."""
from pathlib import Path
import hashlib
import json
import math
import zipfile
import FreeCAD as App
import Part

M=Path(__file__).resolve().parents[1]
cfg=json.loads((M/'parameters.yaml').read_text(encoding='utf-8'))
meta=json.loads((M/'preview/assembly-meshes.json').read_text(encoding='utf-8'))
doc=App.openDocument(str(M/'source/CoralC5_Framework.FCStd'))
motion=doc.getObject('Motion')
invalid=[]
for p in meta['parts']:
    if not doc.getObject(p['name']).Shape.isValid():invalid.append(p['name'])
poses=[]
critical=[]
coverage=[]
if any(not math.isfinite(v) for p in meta['parts'] for v in p['placement']):
    raise RuntimeError('Nonfinite placement in delivered assembly')


def world(name):
    o=doc.getObject(name);s=o.Shape.copy();s.Placement=o.getGlobalPlacement();return s


def overlap(a,b,context):
    sa,sb=world(a),world(b)
    aa,bb=sa.optimalBoundingBox(),sb.optimalBoundingBox()
    if any(min(getattr(aa,k+'Max'),getattr(bb,k+'Max')) <=
           max(getattr(aa,k+'Min'),getattr(bb,k+'Min'))+1e-6 for k in 'XYZ'):return
    vol=sa.common(sb).Volume
    if vol>0.01:critical.append({'part_a':a,'part_b':b,'overlap_mm3':vol,'context':context})


for angle in (-6,0,6):
    print('Checking native pose',angle,flush=True)
    motion.CommandAngle=angle;doc.recompute()
    beam=doc.getObject('A31_Beam');rod=doc.getObject('A42_Rod')
    end=rod.getGlobalPlacement().multVec(App.Vector(cfg['beam']['rod_length'],cfg['beam']['rod_lateral_offset'],0))
    aim=beam.getGlobalPlacement().multVec(App.Vector(cfg['beam']['link_position'],cfg['beam']['link_plane_y']+cfg['beam']['rod_lateral_offset'],0))
    closure=(end-aim).Length
    beam_end=beam.getGlobalPlacement().multVec(App.Vector(cfg['beam']['length'],0,0))
    minimum_above_plate=beam_end.z-(cfg['chassis']['plate_bottom']+cfg['chassis']['thickness'])
    poses.append({'angle_deg':angle,'rod_closure_error_mm':closure,
                  'free_end_above_plate_mm':minimum_above_plate,
                  'pass':closure<1e-6 and minimum_above_plate>=50})
    # Conditional only: image horizontal axis aligned with X, no sensor crop,
    # using Sipeed's listed GC4653 lens; actual fitted lens is not confirmed.
    camera_inverse=doc.getObject('MaixcamHead').getGlobalPlacement().inverse()
    lens_z=-cfg['camera']['maixcam_body_envelope'][2]-11.1
    rays=[]
    for x in (0,cfg['beam']['length']):
        for y in (-13,13):
            for zz in (0,21):
                p=camera_inverse.multVec(beam.getGlobalPlacement().multVec(App.Vector(x,y,zz)))
                depth=lens_z-p.z
                rays.append((math.degrees(math.atan2(abs(p.x),depth)),math.degrees(math.atan2(abs(p.y),depth))))
    hh,vv=2*max(r[0] for r in rays),2*max(r[1] for r in rays)
    coverage.append({'angle_deg':angle,'required_full_horizontal_fov_deg':hh,
                     'required_full_vertical_fov_deg':vv,'conditional_gc4653_pass':hh<=81 and vv<=51})
    for moving in ('BallChannel','LinkPickup','ConnectingRod','RodEye0','RodEye60','CrankArm'):
        for fixed in ('CorePCB','CoreComponents','Motor99PCB','MotorExistingParts','BatteryEnvelope'):
            overlap(moving,fixed,'pose '+str(angle))
    overlap('CrankArm','RodEye0','offset linkage '+str(angle))
    overlap('LinkPickup','RodEye60','clevis '+str(angle))
motion.CommandAngle=0;doc.recompute()
for a,b in [('StepperMount','WingN1'),('StepperMount','Motor99PCB'),('StepperMount','BoardCarrier'),
            ('BatteryEnvelope','BoardCarrier'),('BatteryEnvelope','CorePCB'),
            ('BatteryEnvelope','HingeTower1'),('BatteryEnvelope','HingeTowerN1'),
            ('BatteryRim','HingeTower1'),('BatteryRim','HingeTowerN1'),
            ('MainPlate','LMotorBracketR'),('MainPlate','RMotorBracketL')]:
    if doc.getObject(a) and doc.getObject(b):overlap(a,b,'stationary')
for i in range(4):overlap('BatteryRim','CarrierPost'+str(i),'stationary')
for side in ('L','R'):
    for i in range(4):
        for rib in ('RearRibN70','RearRib70'):
            overlap(side+'CasterPost'+str(i),rib,'stationary')
for a in ('CameraMastFoot','CameraMastOuter'):
    for b in ('BatteryEnvelope','BatteryRim','BoardCarrier','HC08Space','CorePCB'):overlap(a,b,'camera mast')
for a in ('MaixcamBody','StreamCameraPCB'):
    overlap(a,'CameraBoom','camera head')
mounts=[]
limit=cfg['chassis']['plate_bottom']
for p in meta['parts']:
    if p['role']=='flat_top_mount' or 'MotorBracket' in p['name']:
        maximum=world(p['name']).optimalBoundingBox().ZMax
        mounts.append({'name':p['name'],'top_mm':maximum,'limit_mm':limit,'pass':maximum<=limit+1e-5})
core=world('CorePCB').optimalBoundingBox();motor=world('Motor99PCB').optimalBoundingBox()
source=json.loads((M/'reference/reused-hardware/manifest.json').read_text(encoding='utf-8'))
source_checks=[]
for r in source['source_files']:
    p=Path(r['path']);source_checks.append({'path':str(p),'unchanged':hashlib.sha256(p.read_bytes()).hexdigest()==r['sha256']})
bay=json.loads((M/'reference/reused-hardware/battery-bay-source.json').read_text(encoding='utf-8'))
source_checks.append({'path':bay['supplied_step'],'unchanged':hashlib.sha256(Path(bay['supplied_step']).read_bytes()).hexdigest()==bay['supplied_sha256']})
source_checks.append({'path':bay['native_source'],'unchanged':hashlib.sha256(Path(bay['native_source']).read_bytes()).hexdigest()==bay['native_sha256']})
with zipfile.ZipFile(M/'source/CoralC5_Framework.FCStd') as z:
    gui=z.read('GuiDocument.xml').decode('utf-8')
    colors=sum(1 for p in meta['parts'] if 'name="'+p['name']+'"' in gui)
report={'date':'2026-11-05','native_readback':True,'parts':len(meta['parts']),
        'invalid_breps':invalid,'native_poses':poses,'critical_interferences':critical,
        'camera_coverage_conditional':coverage,
        'mount_top_checks':mounts,'source_files':source_checks,'native_color_records':colors,
        'motor_pcb_xy_mm':[motor.XLength,motor.YLength],
        'core_board_height_mm':core.ZLength,'reference_limits':['Battery thickness is a layout assumption.',
            'Mounting holes, clamps, load capacity, printability and tolerances are not production validated.',
            'Only listed structural interferences are checked; this is not an exhaustive assembly DRC.']}
boxes=[world(p['name']).optimalBoundingBox() for p in meta['parts']]
report['overall_envelope_at_zero_mm']=[
    max(getattr(b,k+'Max') for b in boxes)-min(getattr(b,k+'Min') for b in boxes) for k in 'XYZ']
report['pass']=not invalid and all(x['pass'] for x in poses+mounts) and not critical and all(x['unchanged'] for x in source_checks) and abs(motor.XLength-99)<1e-4 and abs(motor.YLength-99)<1e-4 and colors==len(meta['parts'])
(M/'validation/mechanical-check.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
App.closeDocument(doc.Name)
print(json.dumps(report,ensure_ascii=False),flush=True)
if not report['pass']:raise SystemExit(1)
