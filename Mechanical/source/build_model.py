"""Build the self-designed, articulated Coral-C5 mechanical framework.

Run with the portable FreeCAD Python. Parameters use the JSON subset of YAML.
All reference shapes are copied into this document; original boards and source
assemblies are never opened for writing.
"""
from pathlib import Path
import json
import math
import hashlib
import FreeCAD as App
import Part
import Import
from camera_support import build_cameras

M=Path(__file__).resolve().parents[1]
P=json.loads((M/'parameters.yaml').read_text(encoding='utf-8'))
C=P['chassis']; E=P['electronics']; B=P['beam']
V=App.Vector
COLORS={'dark':[.075,.105,.14],'panel':[.13,.19,.24], 'coral':[1,.30,.20],
        'cyan':[.10,.78,.78],'silver':[.66,.72,.77],'black':[.025,.035,.045],
        'green':[.025,.30,.22],'gold':[.82,.57,.16],'glass':[.025,.16,.23]}
doc=App.newDocument('CoralC5_Framework')
parts=[]; groups={}; repairs=[]


def group(name,label,parent=None):
    o=doc.addObject('App::Part',name);o.Label=label
    if parent: parent.addObject(o)
    groups[name]=o
    return o


def add(parent,name,label,shape,color='silver',role='concept',opacity=1):
    o=doc.addObject('Part::Feature',name);o.Label=label;o.Shape=shape
    parent.addObject(o)
    o.addProperty('App::PropertyString','ReferenceStatus','Reference');o.ReferenceStatus=role
    col=COLORS.get(color,color)
    parts.append({'name':o.Name,'label':label,'group':parent.Name,'color':col,'opacity':opacity,'role':role})
    return o


def box(x,y,z,dx,dy,dz): return Part.makeBox(dx,dy,dz,V(x,y,z))
def cyl(x,y,z,r,h,axis=(0,0,1)): return Part.makeCylinder(r,h,V(x,y,z),V(*axis))


def poly(points,z,h):
    pts=[V(x,y,z) for x,y in points];pts.append(pts[0])
    return Part.Face(Part.makePolygon(pts)).extrude(V(0,0,h))


def link_shape(a,b,r):
    a,b=V(*a),V(*b);delta=b-a
    return Part.makeCylinder(r,delta.Length,a,delta.normalize())


def read(name): return Part.read(str(M/'reference/reused-hardware'/(name+'.brep')))


def transformed(s,rotation,translation):
    s=s.copy();s.Placement=App.Placement(V(*translation),rotation).multiply(s.Placement)
    return s


chassis=group('A10_Chassis','A10 角切底盘 / 245×180')
drive=group('A11_Drive','A11 参考编码轮组 / 原比例')
caster=group('A12_Casters','A12 参考万向轮 / 齐平安装柱')
power=group('A15_Battery','A15 原参考电池定位框 / 高度待核')
electronics=group('A20_Electronics','A20 Core + Motor99 机械规划')
hmi=group('A25_HMI','A25 TFT / 按钮 / HC-08 空间预留')
gantry=group('A30_Gantry','A30 摆杆铰接支架')
beam=group('A31_Beam','A31 可动摆杆 / 钢球槽')
actuator=group('A40_Stepper','A40 42步进参考包络')
crank=group('A41_Crank','A41 可动曲柄')
rod=group('A42_Rod','A42 可动连杆')
sensor=group('A70_Sensor','A70 前置循迹安装架 / 预留')
cameras=group('A80_Cameras','A80 双摄支架 / MaixCAM检测与OV2640图传')

z=C['plate_bottom'];t=C['thickness'];top=z+t
hx=C['length']/2;hy=C['width']/2
outline=[(-hx,-60),(-110,-80),(-88,-hy),(88,-hy),(112,-72),(hx,-45),
         (hx,45),(112,72),(88,hy),(-88,hy),(-110,80),(-hx,60)]
base=poly(outline,z,t)
# Four paired diagonal openings keep the center load paths intact.
for sign in (-1,1):
    for x in (-74,-48,-22):
        if sign==1 and x==-22:continue  # Solid load path under the camera mast foot.
        cut=box(x,sign*72-2,z-1,20,4,t+2)
        cut.rotate(V(x,sign*72,z),V(0,0,1),sign*24)
        base=base.cut(cut)
# Provisional motor fixing slots; verify the physical bracket before fabrication.
for x in (52,102):
    for y in (-79,79): base=base.cut(cyl(x,y,z-1,1.8,t+2))
for x in (-104,104):
    for y in (-57,57): base=base.cut(cyl(x,y,z-1,1.8,t+2))
# The original caster has four mounting axes; the rebuilt posts end below this plate.
for x in (-105.5,-81.5):
    for y in (-63,-33,33,63): base=base.cut(cyl(x,y,z-1,1.7,t+2))
for x in (-9,9):
    for y in (63,81):base=base.cut(cyl(x,y,z-1,1.7,t+2))
add(chassis,'MainPlate','主底板 / 切角轮廓与斜向减重孔',base,'dark')
outer=poly(outline,top,.8)
inner=poly([(x*.967,y*.967) for x,y in outline],top-.1,1.2)
add(chassis,'PerimeterAccent','珊瑚色周边护线 / 装饰件',outer.cut(inner),'coral')
for sign in (-1,1):
    pts=[(28,sign*61),(96,sign*61),(109,sign*73),(88,sign*84),(33,sign*84)]
    cover=poly(pts,top+1,2)
    for x in (45,57,69,81): cover=cover.cut(box(x,sign*72-1.8,top,5,3.6,5))
    if sign==-1:
        cover=cover.cut(box(B['stepper_center_xz'][0]-25,B['stepper_face_y']-9,top,50,13,5))
    add(chassis,'Wing'+str(sign).replace('-','N'),'侧翼护板 / 镂空通风',cover,'panel')
    stripe=box(35,sign*81-.6,top+3,49,1.2,.5)
    if sign==-1:
        stripe=stripe.cut(box(B['stepper_center_xz'][0]-25,B['stepper_face_y']-9,top,50,13,5))
    add(chassis,'WingLine'+str(sign).replace('-','N'),'侧翼青色识别条',stripe,'cyan')
for y in (-70,70):
    add(chassis,'RearRib'+str(y).replace('-','N'),'底板下加强筋',box(-109,y-2,z-6,70,4,6),'panel')

# Original R3X assemblies, rotated from front=-Y to front=+X and translated only.
rot=App.Rotation(V(0,0,1),90)
manifest=json.loads((M/'reference/reused-hardware/manifest.json').read_text(encoding='utf-8'))
refmeta={r['name']:r for r in manifest['parts']}
mount_trim=[]
for side,source_side in (('L','R'),('R','L')):
    w=refmeta['Rear'+source_side+'Wheel']['bbox_mm']
    oldcenterx=(w[0]+w[3])/2;oldcentery=(w[1]+w[4])/2
    targety=C['track_width']/2*(1 if side=='L' else -1)
    shift=(C['drive_axis_x']+oldcentery,targety-oldcenterx,0)
    names=['MotorBracket'+source_side,'Rear'+source_side+'Wheel','Rear'+source_side+'Hub']
    names += ['Motor'+source_side+'Part'+str(i) for i in (range(21,28) if source_side=='R' else range(14,21))]
    for name in names:
        s=transformed(read(name),rot,shift)
        if name.startswith('MotorBracket'):
            oldvol=s.Volume
            # Flat mounting face: no peg, screw tip or extra segment above the support plane.
            s=s.common(box(-200,-200,-10,400,400,z+10))
            mount_trim.append({'name':name,'removed_mm3':oldvol-s.Volume,'top_limit_mm':z})
        color='black' if name.endswith('Wheel') else ('panel' if name.endswith('Hub') else 'silver')
        add(drive,side+name,'参考轮组 '+side+' / '+name,s,color,'reference_geometry_1to1')

# Two original caster mechanisms. Their spacers are rebuilt as flat-ended hollow posts.
for side,ty in (('L',48),('R',-48)):
    cr=App.Rotation(V(0,0,1),-90)
    shift=(-3.5,ty-57,0)
    for n in ('CasterLAxle','CasterLWheel','CasterLSwivel'):
        add(caster,side+n,'参考万向轮 '+side+' / '+n,transformed(read(n),cr,shift),
            'black' if n.endswith('Wheel') else 'silver','reference_geometry_1to1')
    for i,(px,py) in enumerate([(-102,-72),(-78,-72),(-102,-42),(-78,-42)]):
        # Same four mounting axes as the reference caster, mapped by Rz(-90).
        x,y=px-3.5,ty-57-py
        post=cyl(x,y,34,3,z-34).cut(cyl(x,y,33,1.6,z-32))
        add(caster,side+'CasterPost'+str(i),'万向轮齐平隔柱 / 无顶部短凸段',post,'gold','flat_top_mount')

# Exact 91 x 53 battery locating rim identified by the user in EdgeMind_Assembly.step.
bat=P['battery'];bx,by=bat['center_xy']
bay=transformed(read('BatteryBay'),App.Rotation(V(0,0,1),90),(bx,by,top))
add(power,'BatteryRim','原装配电池定位凸框 / 91×53×3',bay,'coral','supplied_assembly_geometry')
add(power,'BatteryEnvelope','电池占位 / 87×49 / 高度25待实测',
    box(bx-43.5,by-24.5,top+.2,87,49,bat['height_placeholder']),'panel','height_unmeasured',.88)
for px in (bx-28,bx+28):
    band=box(px-2,by-26,top+24.8,4,52,1)
    add(power,'BatteryStrap'+str(int(px)).replace('-','N'),'电池固定带参考',band,'black')

# Carrier has external clamp pads: no fictional PCB mounting holes are introduced.
ex,ey=E['motor_board_center_xy'];ez=E['board_bottom']
tray=box(ex-54.5,ey-54.5,ez-2,109,109,2).cut(box(ex-43,ey-43,ez-3,86,86,4))
add(electronics,'BoardCarrier','Motor99 外夹托架 / 孔位未分配到PCB',tray,'panel')
for i,(dx,dy) in enumerate(((-51,-51),(-51,51),(51,-51),(51,51))):
    post=cyl(ex+dx,ey+dy,top,3,ez-2-top).cut(cyl(ex+dx,ey+dy,top-1,1.6,ez-top))
    add(electronics,'CarrierPost'+str(i),'电控托架齐平支柱',post,'gold')

motor=Part.read(str(M/'reference/CoralC5-Motor99-planning.step'))
motor_pcb=[];motor_components=[]
for s in motor.Solids:
    b=s.optimalBoundingBox()
    (motor_pcb if b.XLength>98 and b.YLength>98 and b.ZLength<2 else motor_components).append(s)
if not motor_pcb: raise RuntimeError('Expected the virtual 99 mm Motor board body')
for name,shapes,color in (('Motor99PCB',motor_pcb,'green'),('MotorExistingParts',motor_components,'silver')):
    add(electronics,name,'Motor99 规划包络' if name=='Motor99PCB' else '原PCB包络内53件参考 / 不含板外器件',
        transformed(Part.makeCompound(shapes),App.Rotation(),(ex,ey,ez)),color,'virtual_outline_current_component_positions')

core=Part.read(str(M/'reference/CoralC5-Core-current.step'))
core_rot=App.Rotation(V(0,0,1),180).multiply(App.Rotation(V(1,1,1),120))
socket=(ex-33.75,ey+4.875,ez+1.6)
core_origin=(socket[0]+.8,socket[1]+.5,socket[2]+E['core_bottom_above_motor_top'])
core_pcb=[];core_parts=[]
for s in core.Solids:
    b=s.optimalBoundingBox()
    (core_pcb if b.XLength>69 and b.YLength>50 and b.ZLength<2 else core_parts).append(s)
for name,shapes,color in (('CorePCB',core_pcb,'green'),('CoreComponents',core_parts,'black')):
    add(electronics,name,'真实核心板板形' if name=='CorePCB' else '真实核心板已导出的元件',
        transformed(Part.makeCompound(shapes),core_rot,core_origin),color,'current_core_pcb_export')
gold=[]
for p in json.loads((M/'reference/core-edge-pads.json').read_text(encoding='utf-8'))['pads']:
    if not p['number']:continue
    x=p['xy'][0]-90.95;y=85.5-p['xy'][1];w,h=p['size']
    gold.append(box(x-w/2,y-h/2,1.52 if p['front'] else -.025,w,h,.025))
add(electronics,'CoreGoldContacts','98触点金手指 / 实际焊盘位置',
    transformed(Part.makeCompound(gold),core_rot,core_origin),'gold','current_core_pad_geometry')

# A tilted 1.8-inch display console, four button spaces, and an HC-08 volume.
tx,ty=E['tft_center_xy'];console=group('DisplayConsole','任务操作台 / 屏幕参考外形',hmi)
console.Placement=App.Placement(V(tx,ty,top+29),App.Rotation(V(0,1,0),-17))
add(console,'DisplayTray','TFT托盘参考',box(-30,-19,-2,60,38,2),'panel')
add(console,'TFTModule','ZJY180S0800TG01 / 35×56×3.5',box(-28,-17.5,0,56,35,3.5),'black','datasheet_envelope')
add(console,'TFTActive','TFT有效显示区域 / 35.04×28.03',box(-17.52,-14.015,3.5,35.04,28.03,.06),'glass','datasheet_active_area')
for i,(length,y) in enumerate(((25,8),(19,2),(23,-5))):
    add(console,'ScreenLine'+str(i),'任务界面示意条',box(-12,y,3.57,length,1.1,.03),'cyan')
for x in (tx-22,tx+22):add(hmi,'ConsoleLeg'+str(x).replace('-','N'),'操作台支撑',link_shape((x,ty,top),(x,ty,top+26),3),'silver')
for i in range(E['button_reservations']):
    y=-58+i*13
    add(hmi,'Button'+str(i),'任务按钮预留 '+str(i+1),cyl(-111,y,top+2,4,3),'coral' if i==0 else 'dark','button_count_provisional')
add(hmi,'HC08Space','四脚HC-08安装空间 / 40×25×12待核',box(-63,59,top+1,40,25,12),'cyan','reserved_envelope',.3)

hinge_x=B['hinge_x'];hinge_z=top+B['hinge_height_above_plate']
for side in (-1,1):
    points=[(hinge_x-7,top),(hinge_x+11,top),(hinge_x+4,hinge_z+5),(hinge_x-4,hinge_z+5)]
    # Cross section in XZ, extruded across Y.
    wire=Part.makePolygon([V(x,side*21-3,zv) for x,zv in points]+[V(points[0][0],side*21-3,points[0][1])])
    tower=Part.Face(wire).extrude(V(0,6,0)).cut(cyl(hinge_x,side*21-4,hinge_z,2.6,9,(0,1,0)))
    add(gantry,'HingeTower'+str(side).replace('-','N'),'端铰支架 / 青色加劲',tower,'panel')
    add(gantry,'HingeAccent'+str(side).replace('-','N'),'铰架装饰筋',
        link_shape((hinge_x+6,side*24,top+8),(hinge_x,side*24,hinge_z-8),1.2),'cyan')
add(gantry,'HingePin','摆杆铰轴参考',cyl(hinge_x,-27,hinge_z,2.5,54,(0,1,0)),'silver')

# All beam shapes are local to the actual hinge and driven by native expressions.
track=box(0,-13,0,B['length'],26,3)
track=track.fuse(box(0,-13,3,B['length'],3,13)).fuse(box(0,10,3,B['length'],3,13))
lug=box(-5,-10,-6,13,20,9).cut(cyl(0,-11,0,2.6,22,(0,1,0)))
add(beam,'BallChannel','280mm球槽 / 端铰结构',track.fuse(lug),'silver')
for side in (-1,1):
    add(beam,'BeamTrim'+str(side).replace('-','N'),'摆杆侧边识别条',box(8,side*13-.6,7,264,1.2,3),'coral')
add(beam,'BeamEndStop','球槽端部软挡参考',box(276,-10,3,4,20,10),'black')
pickup=box(B['link_position']-5,-40,-5,13,30,10)
rod_y=B['link_plane_y']+B['rod_lateral_offset']
pickup=pickup.cut(box(B['link_position']-6,rod_y-3.5,-6,10.5,7,12))
pickup=pickup.cut(cyl(B['link_position'],-41,0,1.6,33,(0,1,0)))
add(beam,'LinkPickup','摆杆连杆叉耳 / 销轴铰接',pickup,'panel')
add(beam,'BeamLinkPin','摆杆连杆连接销',cyl(B['link_position'],-41,0,1.5,16,(0,1,0)),'silver')
ball=add(beam,'SteelBall','钢球 / 18mm候选',Part.makeSphere(B['display_ball_diameter']/2,V(0,0,0)),'silver','contest_size_candidate')
ball.Placement.Base=V(140,0,3+B['display_ball_diameter']/2)

# Reference stepper package: model dimension is not a measured manufacturer part.
sx,sz=B['stepper_center_xz'];fy=B['stepper_face_y']
add(actuator,'StepperHousing','42BYGH34-401A 参考包络 / 待实测',box(sx-21,fy-34,sz-21,42,34,42),'black','unmeasured_envelope')
add(actuator,'StepperFront','42步进前盖参考',box(sx-21,fy-3,sz-21,42,3,42),'silver','unmeasured_envelope')
add(actuator,'StepperShaft','步进轴参考',cyl(sx,fy,sz,2.5,23,(0,1,0)),'silver','unmeasured_envelope')
foot=box(sx-24,fy-8,top,48,11,3).common(poly(outline,top,3))
front=box(sx-24,fy,top+3,48,3,sz+24-top-3)
front=front.cut(cyl(sx,fy-1,sz,11.3,5,(0,1,0)))
front=front.cut(box(sx-16,fy-1,top+10,32,5,sz-25-top-10))
for dx in (-15.5,15.5):
    for dz in (-15.5,15.5):
        slot=cyl(sx+dx,fy-1,sz+dz-1,1.7,5,(0,1,0))
        slot=slot.fuse(cyl(sx+dx,fy-1,sz+dz+1,1.7,5,(0,1,0)))
        slot=slot.fuse(box(sx+dx-1.7,fy-1,sz+dz-1,3.4,5,2))
        front=front.cut(slot)
mount=foot.fuse(front)
for x in (sx-18,sx+18):mount=mount.cut(cyl(x,fy-3,top-1,1.7,5))
add(actuator,'StepperMount','步进L形支架 / 31mm孔距可调槽待实测',mount,'panel','provisional_mounting_pattern')
arm=box(-4,-2,-4,B['crank_radius']+8,4,8)
arm=arm.cut(cyl(0,-3,0,2.55,6,(0,1,0)))
arm=arm.cut(cyl(B['crank_radius'],-3,0,1.6,6,(0,1,0)))
add(crank,'CrankArm','20mm曲柄 / 可调方案参考',arm,'coral')
add(crank,'CrankHub','曲柄轴套',cyl(0,-3,0,5,6,(0,1,0)).cut(cyl(0,-4,0,2.55,8,(0,1,0))),'panel')
add(crank,'CrankLinkPin','曲柄连杆连接销 / 错层避让',cyl(B['crank_radius'],-4,0,1.5,14,(0,1,0)),'silver')
offset=B['rod_lateral_offset']
add(rod,'ConnectingRod','60mm连杆 / 错层6mm',cyl(3.2,offset,0,2.5,B['rod_length']-6.4,(1,0,0)),'silver')
for x in (0,B['rod_length']):
    eye=cyl(x,offset-3,0,4,6,(0,1,0)).cut(cyl(x,offset-4,0,1.6,8,(0,1,0)))
    add(rod,'RodEye'+str(int(x)),'连杆销接环 / 配合间隙待实测',eye,'panel')

motion=doc.addObject('App::FeaturePython','Motion');motion.Label='运动参数 / CommandAngle可调±6°'
motion.addProperty('App::PropertyFloatConstraint','CommandAngle','Motion');motion.CommandAngle=(B['initial_angle_deg'],-6,6,.1)
motion.addProperty('App::PropertyAngle','BeamAngle','Motion');motion.setExpression('BeamAngle','CommandAngle * 1 deg')
for name,value in (('HingeX',hinge_x),('HingeZ',hinge_z),('AttachDistance',B['link_position']),('StepperX',sx),('StepperZ',sz),('CrankRadius',B['crank_radius']),('RodLength',B['rod_length']),('BallPosition',140)):
    motion.addProperty('App::PropertyDistance',name,'Geometry');setattr(motion,name,value)
for name in ('AttachX','AttachZ','CrankTipX','CrankTipZ'):
    motion.addProperty('App::PropertyLength',name,'Calculated')
for name in ('CrankAngle','RodAngle'):motion.addProperty('App::PropertyAngle',name,'Calculated')
motion.setExpression('AttachX','HingeX + AttachDistance*cos(BeamAngle)')
motion.setExpression('AttachZ','HingeZ + AttachDistance*sin(BeamAngle)')
d2='((AttachX-StepperX)^2+(AttachZ-StepperZ)^2)'
motion.setExpression('CrankAngle','atan2(AttachZ-StepperZ; AttachX-StepperX)-acos(('+d2+'+CrankRadius^2-RodLength^2)/(2*CrankRadius*sqrt('+d2+')))')
motion.setExpression('CrankTipX','StepperX+CrankRadius*cos(CrankAngle)')
motion.setExpression('CrankTipZ','StepperZ+CrankRadius*sin(CrankAngle)')
motion.setExpression('RodAngle','atan2(AttachZ-CrankTipZ; AttachX-CrankTipX)')
beam.Placement=App.Placement(V(hinge_x,0,hinge_z),App.Rotation(V(0,-1,0),1))
beam.setExpression('Placement.Rotation.Angle','Motion.BeamAngle')
crank.Placement=App.Placement(V(sx,B['link_plane_y'],sz),App.Rotation(V(0,-1,0),1))
crank.setExpression('Placement.Rotation.Angle','Motion.CrankAngle')
rod.Placement=App.Placement(V(0,B['link_plane_y'],0),App.Rotation(V(0,-1,0),1))
rod.setExpression('Placement.Base.x','Motion.CrankTipX');rod.setExpression('Placement.Base.z','Motion.CrankTipZ')
rod.setExpression('Placement.Rotation.Angle','Motion.RodAngle')
ball.setExpression('Placement.Base.x','Motion.BallPosition')

add(sensor,'LineSensorBracket','前置循迹板安装托 / 预留',box(118,-43,22,24,86,3),'panel')
for y in (-32,32):add(sensor,'SensorArm'+str(y).replace('-','N'),'可调循迹架支撑',link_shape((104,y,top),(124,y,25),2),'silver')
for i in range(12):add(sensor,'LineProbe'+str(i),'循迹探头位置参考',box(126,-34+i*6,20,6,3,2),'black','reserved_envelope')

build_cameras(P,cameras,group,add,box,cyl,link_shape)

doc.recompute()
invalid=[o.Name for o in doc.Objects if 'Invalid' in o.State or 'Error' in o.State]
if invalid:raise RuntimeError('Invalid expression objects: '+str(invalid))
for angle in (-6,0,6):
    motion.CommandAngle=angle;doc.recompute()
    end=rod.Placement.multVec(V(B['rod_length'],B['rod_lateral_offset'],0))
    expected=V(float(motion.AttachX),B['link_plane_y']+B['rod_lateral_offset'],float(motion.AttachZ))
    error=(end-expected).Length
    if not math.isfinite(error) or error>1e-6:raise RuntimeError('Link closure error')
motion.CommandAngle=B['initial_angle_deg'];doc.recompute()

for meta in parts:
    print('Meshing',meta['name'],flush=True)
    o=doc.getObject(meta['name']);s=o.Shape
    if not s.isValid():
        before=s.Volume
        fixed=s.copy();fixed.fix(1e-6,1e-7,1e-3)
        method='OCC tolerance/topology fix, max 0.001 mm'
        if not fixed.isValid():
            repaired=M/'reference'/('repaired-'+o.Name+'.step')
            s.exportStep(str(repaired));fixed=Part.read(str(repaired))
            method='STEP write/read topology healing'
        repairs.append({'name':o.Name,'method':method,'valid_after':fixed.isValid(),
                        'volume_before_mm3':before,'volume_after_mm3':fixed.Volume})
        if not fixed.isValid():raise RuntimeError('Invalid BRep after repair '+o.Name)
        o.Shape=fixed;s=o.Shape
    meta['volume_mm3']=s.Volume
    bb=s.optimalBoundingBox();meta['local_bounds']=[bb.XMin,bb.YMin,bb.ZMin,bb.XMax,bb.YMax,bb.ZMax]
    meta['placement']=list(o.getGlobalPlacement().Matrix.A)
    if not all(math.isfinite(x) for x in meta['placement']):raise RuntimeError('Nonfinite placement '+o.Name)
    # Preview meshes use local shape coordinates and the full occurrence transform.
    local=s.copy();local.Placement=App.Placement()
    verts,triangles=local.tessellate(.28)
    meta['vertices']=[[round(v.x,5),round(v.y,5),round(v.z,5)] for v in verts]
    meta['triangles']=[list(x) for x in triangles]

doc.saveAs(str(M/'source/CoralC5_Framework.FCStd'))
# Global BRep compound avoids exporter ambiguity about nested component placements.
world=[]
for meta in parts:
    o=doc.getObject(meta['name']);s=o.Shape.copy();s.Placement=o.getGlobalPlacement();world.append(s)
assembly=Part.makeCompound(world)
assembly.exportStep(str(M/'step/CoralC5_Framework.step'))
(M/'preview/assembly-meshes.json').write_text(json.dumps({'units':'mm','parts':parts,'parameters':P},ensure_ascii=False,separators=(',',':')),encoding='utf-8')
(M/'validation/model-build.json').write_text(json.dumps({'date':'2026-11-05','freecad_version':App.Version(),'parts':len(parts),'all_breps_valid':True,'native_expressions_valid':not invalid,'rod_closure_checked_deg':[-6,0,6],'reference_topology_repairs':repairs,'mount_tops':mount_trim,'source_boards_modified':False,'notes':['Model framework, not a production mechanical release.','Motor99 includes 53 in-envelope current components; 22 layout components remain omitted.','Core STEP exporter retains code 2 for three VRML-only models.']},ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
App.closeDocument(doc.Name)
print(json.dumps({'parts':len(parts),'native':'source/CoralC5_Framework.FCStd','step':'step/CoralC5_Framework.step'},ensure_ascii=False),flush=True)
