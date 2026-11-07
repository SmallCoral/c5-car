"""2026-11-06 schematic layout edit from the saved live-design baseline.

Only schematic coordinates, wires, labels and notes are edited. References,
symbol UUIDs, pin UUIDs, fields, footprint assignments and libraries are kept.
No board access; never invokes the earlier circuit-generation scripts.
"""
from pathlib import Path
import sys,copy,math,uuid,json,hashlib,csv
ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'docs/baseline-2026-11-06-layout'
sys.path.insert(0,str(ROOT.parent/'Shared/design_tools'))
from sexpr import parse,dump,node,nodes,prop,Atom
from schematic_primitives import n,num,pt,xy,effects

def uid(key):return str(uuid.uuid5(uuid.NAMESPACE_URL,'CoralC5-Motor-Tidy-20261106/'+key))
def coord(obj):return tuple(map(float,node(obj,'at')[1:3]))
def setat(obj,x,y,angle=None):
 a=node(obj,'at');a[1:3]=[num(x),num(y)]
 if angle is not None:
  if len(a)<4:a.append(num(angle))
  else:a[3]=num(angle)
def write(file,data):(ROOT/file).write_text(dump(data)+'\n',encoding='utf-8')
def title(data):
 node(node(data,'title_block'),'rev')[1]='M2.1'
 node(node(data,'title_block'),'date')[1]='2026-11-06'

class Layout:
 def __init__(self,file):
  self.file=file;self.d=parse((BASE/file).read_text(encoding='utf-8'));self.original=copy.deepcopy(self.d);title(self.d)
  self.lib={z[1]:z for z in nodes(node(self.d,'lib_symbols'),'symbol')}
  self.symbols={prop(z,'Reference')[2]:z for z in nodes(self.d,'symbol')}
  self.pool={};self.used=set();self.wires=[];self.attach=set();self.count=0
  for z in self.original[1:]:
   if isinstance(z,list) and z[0] in ['label','global_label','hierarchical_label','text']:
    self.pool.setdefault((z[0],z[1]),[]).append(z)
  for z in list(self.d[1:]):
   if isinstance(z,list) and z[0] in ['wire','junction','label','global_label','hierarchical_label','text','rectangle','polyline','no_connect']:self.d.remove(z)
 def reuse(self,tag,name):
  for z in self.pool.get((tag,name),[]):
   if node(z,'uuid')[1] not in self.used:
    self.used.add(node(z,'uuid')[1]);return copy.deepcopy(z)
  return None
 def move(self,ref,x,y,rot=None):
  z=self.symbols[ref];oldx,oldy=coord(z);x,y=pt(x,y)
  setat(z,x,y,rot)
  for fld in nodes(z,'property'):
   if node(fld,'at'):
    xx,yy=coord(fld);setat(fld,xx+x-oldx,yy+y-oldy)
  return self.pins(ref)
 def fields(self,ref,refat,valat):
  z=self.symbols[ref]
  for name,where in [('Reference',refat),('Value',valat)]:setat(prop(z,name),*pt(*where),0)
 def pins(self,ref):
  z=self.symbols[ref];x,y=coord(z);ang=float(node(z,'at')[3]);a=math.radians(ang);unit=int(node(z,'unit')[1]);result={}
  assert node(z,'mirror') is None,(self.file,ref)
  for sub in nodes(self.lib[node(z,'lib_id')[1]],'symbol'):
   uu=int(sub[1].rsplit('_',2)[1])
   if uu not in [0,unit]:continue
   for p in nodes(sub,'pin'):
    px,py=map(float,node(p,'at')[1:3]);result[node(p,'number')[1]]=pt(x+px*math.cos(a)-py*math.sin(a),y-px*math.sin(a)-py*math.cos(a))
  return result
 def wire(self,*points):
  pp=[pt(*p) for p in points]
  for a,b in zip(pp,pp[1:]):
   if a==b:continue
   assert a[0]==b[0] or a[1]==b[1],(self.file,a,b)
   self.wires.append((a,b))
 def label(self,name,where,global_=False,angle=None):
  tag='global_label' if global_ else 'label';angle=(180 if global_ else 0) if angle is None else angle
  z=self.reuse(tag,name)
  if z is None:
   self.count+=1;z=n(tag,name,xy('at',*pt(*where),num(angle)),effects(1.016),n('uuid',uid(self.file+f'/label/{name}/{self.count}')))
   if global_:z.insert(2,n('shape',Atom('bidirectional')))
  setat(z,*pt(*where),angle)
  node(z,'effects')[1:]=effects(1.016,('left' if angle==0 else 'right') if global_ else ('left bottom' if angle==0 else 'right bottom'))[1:]
  self.d.append(z);self.attach.add(pt(*where));return z
 def net(self,pin,name,dx=-17.78,global_=False):
  q=pt(pin[0]+dx,pin[1]);self.wire(pin,q);self.label(name,q,global_,0 if dx>0 and global_ else 180 if global_ else 0)
 def port(self,name,where,angle=180):
  z=self.reuse('hierarchical_label',name);assert z is not None,name
  setat(z,*pt(*where),angle);node(z,'effects')[1:]=effects(1.27,'left' if angle==0 else 'right')[1:]
  self.d.append(z);self.attach.add(pt(*where));return pt(*where)
 def note(self,text,x,y,source=None):
  z=self.reuse('text',source or text)
  if z is None:z=n('text',text,xy('at',*pt(x,y),num(0)),effects(2.54,'left top'),n('uuid',uid(self.file+'/note/'+text)))
  z[1]=text;setat(z,*pt(x,y),0);node(z,'effects')[1:]=effects(2.54,'left top')[1:];self.d.append(z)
 def frame(self,name,x1,y1,x2,y2):
  stroke=n('stroke',n('width',num(.254)),n('type',Atom('dash')))
  if x2>175.26 and y2>165.1:
   pp=[(x1,y1),(x2,y1),(x2,165.1),(175.26,165.1),(175.26,y2),(x1,y2),(x1,y1)]
   z=n('polyline',n('pts',*[xy('xy',*pt(*p)) for p in pp]),stroke,n('fill',n('type',Atom('none'))),n('uuid',uid(self.file+'/frame/'+name)))
  else:z=n('rectangle',xy('start',*pt(x1,y1)),xy('end',*pt(x2,y2)),stroke,n('fill',n('type',Atom('none'))),n('uuid',uid(self.file+'/frame/'+name)))
  self.d.append(z);self.note(name,x1+3.81,y1+3.81)
 def save(self):
  # Retain existing wire/junction UUIDs whenever the geometric segment survives.
  old={tuple(sorted(tuple(map(float,p[1:])) for p in node(w,'pts')[1:])):node(w,'uuid')[1] for w in nodes(self.original,'wire')}
  oldj={coord(j):node(j,'uuid')[1] for j in nodes(self.original,'junction')}
  cuts={p for w in self.wires for p in w}|self.attach
  for ref in self.symbols:cuts.update(self.pins(ref).values())
  segs=set()
  for a,b in self.wires:
   pp=sorted(p for p in cuts if (a[0]==b[0]==p[0] and min(a[1],b[1])<=p[1]<=max(a[1],b[1])) or (a[1]==b[1]==p[1] and min(a[0],b[0])<=p[0]<=max(a[0],b[0])))
   segs.update(zip(pp,pp[1:]))
  degree={}
  for a,b in sorted(segs):
   key=tuple(sorted((a,b)));self.d.append(n('wire',n('pts',xy('xy',*a),xy('xy',*b)),n('stroke',n('width',num(0)),n('type',Atom('default'))),n('uuid',old.get(key,uid(self.file+'/wire/'+str(key))))))
   for p in [a,b]:degree[p]=degree.get(p,0)+1
  for p,deg in degree.items():
   if deg>2:self.d.append(n('junction',xy('at',*p),n('diameter',num(0)),n('color',num(0),num(0),num(0),num(0)),n('uuid',oldj.get(p,uid(self.file+'/junction/'+str(p))))))
  write(self.file,self.d)

def screen_keys():
 p=Layout('07_TFT_Buttons.kicad_sch')
 p.frame('屏幕接口、去耦与安装孔',12.7,19.05,281.94,96.52)
 j=p.move('J17',85.09,60.96);p.fields('J17',(85.09,41.91),(85.09,44.45))
 c=p.move('C56',125.73,52.07)
 cs=p.move('R55',43.18,44.45);rs=p.move('R56',27.94,44.45)
 v=p.port('V3V3_EXT',(31.75,30.48));p.wire(v,(31.75,35.56),(125.73,35.56),c['1']);p.wire((31.75,35.56),(27.94,35.56),rs['1']);p.wire((43.18,35.56),cs['1'])
 p.net(j['1'],'GND');p.net(j['2'],'V3V3_EXT')
 for pn,name in [('3','TFT_SCK'),('4','TFT_MOSI'),('6','TFT_DC'),('8','TFT_BL')]:p.net(j[pn],name,global_=True)
 p.wire(rs['2'],(27.94,j['5'][1]),j['5']);p.label('TFT_RES',(27.94,j['5'][1]),True)
 p.wire(cs['2'],(43.18,j['7'][1]),j['7']);p.label('TFT_CS',(43.18,j['7'][1]),True)
 p.move('#PWR208',125.73,66.04);p.wire(c['2'],(125.73,66.04))
 for ref,x in [('H1',167.64),('H2',208.28)]:p.move(ref,x,41.91)
 p.note('两孔均为 D2.0，不接地。\n按边距推算孔距 30mm，\n与实物复核后确定孔位。',161.29,55.88)
 p.note('三伏三供电，屏幕板已带背光驱动；八针顺序按模块资料。',17.78,83.82)
 p.frame('四个操作按键：低有效输入',12.7,104.14,281.94,161.29)
 v=p.port('V3V3',(20.32,118.11));p.wire(v,(214.63,118.11))
 for i,sig in enumerate(['KEY_MODE','KEY_NEXT','KEY_OK','KEY_START']):
  x=40.64+i*62.23;rr=p.move(f'R{50+i}',x-12.7,124.46);sw=p.move(f'SW{i+2}',x,139.7);cc=p.move(f'C{57+i}',x+16.51,147.32)
  p.wire((x-12.7,118.11),rr['1']);p.wire(rr['2'],(x-12.7,139.7),sw['1']);p.label(sig,(x-12.7,139.7),True)
  p.wire((x-12.7,139.7),(x-12.7,143.51),cc['1']);p.wire(sw['2'],(x+25.4,139.7),(x+25.4,156.21),(x+16.51,156.21),cc['2'])
  p.move(f'#PWR{204+i}',x+25.4,156.21)
 p.note('十千欧上拉与约一毫秒滤波，软件仍须消抖。\n安装孔在原理图中仅作约束，实际孔位由用户定位。',17.78,173.99)
 p.save()

def gray_cameras():
 p=Layout('08_Grayscale_Cameras.kicad_sch')
 p.frame('感为八路灰度与辅助板',12.7,19.05,281.94,97.79)
 j=p.move('J20',69.85,53.34);s=p.move('J21',147.32,45.72)
 p.fields('J20',(69.85,29.21),(69.85,31.75));p.fields('J21',(147.32,29.21),(147.32,31.75))
 v=p.port('V5_EXT',(30.48,36.83));p.wire(v,(129.54,36.83),(129.54,s['1'][1]),s['1']);p.wire((53.34,36.83),(53.34,j['1'][1]),j['1'])
 p.wire(j['2'],(20.32,j['2'][1]),(20.32,85.09),(91.44,85.09),(91.44,s['2'][1]),s['2']);p.move('#PWR209',20.32,85.09);p.label('GND',(20.32,85.09))
 for k in range(1,9):p.net(j[str(k+2)],f'GRAY_CH{k}',global_=True)
 for pn,name in [('3','GRAY_CLK'),('4','GRAY_DAT')]:p.net(s[pn],name,global_=True)
 p.note('并行或同步串行择一接线。\nCH1～CH8、DAT 无上下拉输入，\nCLK 为主控输出，逻辑三伏三。\n五伏接整套组件电源入口。',180.34,40.64)
 for ref,cap,prefix,x,groundx,x1,x2,name in [('J18','C61','MAIX',64.77,20.32,12.7,142.24,'MaixCAM 检测模块'),('J19','C62','VIDEO',198.12,157.48,154.94,281.94,'独立图传处理模块')]:
  p.frame(name,x1,105.41,x2,161.29)
  j=p.move(ref,x,129.54);c=p.move(cap,x+40.64,129.54)
  p.fields(ref,(x,118.11),(x,120.65))
  rail=pt(x-19.05,116.84);p.wire(j['1'],(rail[0],j['1'][1]),rail,(c['1'][0],rail[1]),c['1']);p.label('V5_EXT',rail)
  p.wire(j['2'],(groundx,j['2'][1]),(groundx,144.78),(c['2'][0],144.78),c['2']);p.label('GND',(groundx,144.78))
  p.net(j['3'],prefix+'_TX',global_=True);p.net(j['4'],prefix+'_RX',global_=True)
  p.note('五伏供电，串口三伏三。\nMCU_TX 接模块 RX，反向同理。',x1+3.81,149.86)
 p.note('相机口连接处理模块；裸 OV2640 不能直接接此串口。\n线束针序与模块版本须按实物核对。',17.78,173.99)
 p.save()

def bluetooth_stepper():
 p=Layout('09_Bluetooth_Stepper.kicad_sch')
 p.frame('HC-08 蓝牙与供电选择',12.7,19.05,281.94,87.63)
 j=p.move('J15',67.31,48.26);s=p.move('J16',121.92,48.26)
 p.fields('J15',(67.31,36.83),(67.31,39.37));p.fields('J16',(121.92,36.83),(121.92,39.37))
 p.wire(j['1'],(43.18,j['1'][1]),(43.18,30.48),(93.98,30.48),(93.98,s['2'][1]),s['2'])
 p.label('VBT',(43.18,30.48))
 v=p.port('V3V3_EXT',(172.72,34.29),0);p.wire(v,(104.14,34.29),(104.14,s['1'][1]),s['1'])
 v=p.port('V5_EXT',(172.72,73.66),0);p.wire(v,(99.06,73.66),(99.06,s['3'][1]),s['3'])
 p.net(j['2'],'GND');p.net(j['3'],'BT_TX',global_=True);p.net(j['4'],'BT_RX',global_=True)
 p.note('默认 1—2 跳帽选择三伏三。\n确认稳压载板后才可选五伏。\n四脚线束按本板针序制作。',195.58,41.91)
 p.frame('外接步进驱动：脉冲或串口择一',12.7,95.25,281.94,161.29)
 j=p.move('J22',83.82,129.54);u=p.move('J23',190.5,129.54);pow=p.move('J24',254,127)
 for ref,x,yy in [('J22',83.82,114.3),('J23',190.5,118.11),('J24',254,118.11)]:p.fields(ref,(x,yy),(x,yy+2.54))
 p.net(j['1'],'V3V3');p.net(j['2'],'GND');p.wire(j['6'],(j['6'][0],139.7));p.label('GND',(j['6'][0],139.7))
 for ref,sig,pn,x in [('R57','STEP_PUL','3',33.02),('R58','STEP_DIR','4',50.8),('R54','STEP_nEN','5',68.58)]:
  rr=p.move(ref,x,146.05,180 if ref=='R54' else 0);offset=6.35 if ref=='R54' else 2.54;p.fields(ref,(x+offset,144.78),(x+offset,147.32))
  top=rr['2' if ref=='R54' else '1'];p.wire(j[pn],(x,j[pn][1]),top);p.label(sig,(x,j[pn][1]),True)
  if ref=='R54':p.net(rr['1'],'V3V3',dx=12.7)
  else:p.wire(rr['2'],(x,153.67))
 p.wire((33.02,153.67),(50.8,153.67));p.move('#PWR210',50.8,153.67);p.label('GND',(33.02,153.67))
 v=p.port('V3V3',(165.1,111.76));p.wire(v,(170.18,111.76),(170.18,u['1'][1]),u['1'])
 p.net(u['2'],'GND');p.net(u['3'],'STEP_UART_TX',global_=True);p.net(u['4'],'STEP_UART_RX',global_=True)
 v=p.port('VM',(226.06,109.22));p.wire(v,(233.68,109.22),(233.68,pow['1'][1]),pow['1']);p.net(pow['2'],'GND')
 p.note('电机须接外置驱动板，不能接控制排针。\n光耦型输入须适配电平与有效极性。\nCore 三伏三仅供电平参考与小负载。',17.78,171.45)
 p.save()

def auxiliary_power():
 # Preserve the existing power circuit geometry; eliminate the floating alias
 # stub and group the two fully wired regulator circuits with the same frames.
 p=Layout('06_Module_Power.kicad_sch')
 p.d=copy.deepcopy(p.original);title(p.d)
 for z in list(nodes(p.d,'text')):p.d.remove(z)
 for z in list(nodes(p.d,'label')):
  if z[1] in ['VM','GND'] and (coord(z)[1] in [25.4,40.64,190.5]):p.d.remove(z)
 for z in list(nodes(p.d,'wire')):
  if all(float(t[2])==25.4 for t in node(z,'pts')[1:]):p.d.remove(z)
 port=next(z for z in nodes(p.d,'hierarchical_label') if z[1]=='VM');setat(port,20.32,40.64,180);node(port,'effects')[1:]=effects(1.27,'right')[1:]
 z=next(z for z in nodes(p.d,'symbol') if prop(z,'Reference')[2]=='#PWR203');old=coord(z);setat(z,55.88,99.06)
 for pr in nodes(z,'property'):
  if node(pr,'at'):
   x,y=coord(pr);setat(pr,x+55.88-old[0],y+99.06-old[1])
 p.frame('独立五伏降压：相机与灰度供电',12.7,19.05,281.94,113.03)
 p.note('外设总预算 1.5A；芯片额定 2A，须验证启动峰值与散热。',17.78,106.68)
 p.frame('独立三伏三：屏幕与蓝牙供电',12.7,120.65,281.94,186.69)
 p.note('外设预算 0.15A，与 Core 电源分开。\n原四安保护保留，上游总容量须核算。',17.78,173.99)
 write(p.file,p.d)

def consistent_existing():
 for file in ['CoralC5-Motor.kicad_sch','02_Power.kicad_sch','03_Core_Interface.kicad_sch','04_Left_Motor.kicad_sch','05_Right_Motor.kicad_sch']:
  d=parse((BASE/file).read_text(encoding='utf-8'));title(d)
  for z in nodes(d,'rectangle')+nodes(d,'polyline'):
   st=node(z,'stroke')
   if st:node(st,'width')[1]=num(.254)
  write(file,d)

def overview_signal_groups():
 return {
  '07_TFT_Buttons.kicad_sch':['TFT_SCK','TFT_MOSI','TFT_RES','TFT_DC','TFT_CS','TFT_BL','KEY_MODE','KEY_NEXT','KEY_OK','KEY_START'],
  '08_Grayscale_Cameras.kicad_sch':[f'GRAY_CH{i}' for i in range(1,9)]+['GRAY_CLK','GRAY_DAT','MAIX_TX','MAIX_RX','VIDEO_TX','VIDEO_RX'],
  '09_Bluetooth_Stepper.kicad_sch':['BT_TX','BT_RX','STEP_PUL','STEP_DIR','STEP_nEN','STEP_UART_TX','STEP_UART_RX'],
 }

def overview_power_flow():
 # One actual wire per signal: the overview must show every module connection.
 # A3 accommodates all 44 GPIO roles without shrinking text or inventing buses.
 file='CoralC5-Motor.kicad_sch';d=parse((ROOT/file).read_text(encoding='utf-8'));node(d,'paper')[1]='A3'
 sheets={prop(z,'Sheetfile')[2]:z for z in nodes(d,'sheet')}
 oldtexts=list(nodes(d,'text'))
 for z in list(d[1:]):
  if isinstance(z,list) and z[0] in ['wire','junction','label','text']:d.remove(z)
 core=sheets['03_Core_Interface.kicad_sch'];left=sheets['04_Left_Motor.kicad_sch'];right=sheets['05_Right_Motor.kicad_sch'];power=sheets['02_Power.kicad_sch'];aux=sheets['06_Module_Power.kicad_sch']
 def box(sh,x,y,w,h):
  setat(sh,x,y);node(sh,'size')[1:]=[num(w),num(h)]
  setat(prop(sh,'Sheetname'),x,y-2.54,0);setat(prop(sh,'Sheetfile'),x,y+h+2.54,0)
 def pin(sh,name,x,y,side):
  z=next((p for p in nodes(sh,'pin') if p[1]==name),None)
  if z is None:
   z=n('pin',name,Atom('bidirectional'),xy('at',x,y,num(side)),n('uuid',uid('overview/pin/'+prop(sh,'Sheetfile')[2]+'/'+name)),effects(1.016));sh.append(z)
  setat(z,x,y,side);node(z,'effects')[1:]=effects(1.016,'right' if side==0 else 'left')[1:];return coord(z)
 def wire(*points):
  for a,b in zip(points,points[1:]):
   assert a!=b and (a[0]==b[0] or a[1]==b[1]),(a,b)
   d.append(n('wire',n('pts',xy('xy',*a),xy('xy',*b)),n('stroke',n('width',num(0)),n('type',Atom('default'))),n('uuid',uid('overview/wire/'+str((a,b))))))
 def label(name,where):d.append(n('label',name,xy('at',*where,num(0)),effects(1.016,'left bottom'),n('uuid',uid('overview/label/'+name+'/'+str(where)))))
 def stub(sh,name,x,y):
  p=pin(sh,name,x,y,180);q=pt(x-25.4,y);wire(q,p);label(name,q)
 def note(text,x,y):
  z=copy.deepcopy(oldtexts.pop(0));z[1]=text;setat(z,x,y,0);node(z,'effects')[1:]=effects(2.54,'left top')[1:];d.append(z)
 box(core,165.1,40.64,86.36,205.74)
 group_y=[40.64,109.22,199.39];group_h=[55.88,77.47,60.96]
 for (name,signals),y,h in zip(overview_signal_groups().items(),group_y,group_h):
  sh=sheets[name];box(sh,38.1,y,71.12,h)
  for i,sig in enumerate(signals):
   yy=round(y+6.35+i*3.81,2);a=pin(sh,sig,109.22,yy,0);b=pin(core,sig,165.1,yy,180);wire(a,b)
 box(left,312.42,40.64,83.82,55.88);box(right,312.42,109.22,83.82,55.88)
 leftsignals=['L_PWM','L_DIR','MOTOR_RUN','L_nFAULT','L_ISENSE','L_ENC_A','L_ENC_B']
 rightsignals=['R_PWM','R_DIR','MOTOR_RUN','R_nFAULT','R_ISENSE','R_ENC_A','R_ENC_B']
 for i,sig in enumerate(leftsignals):
  yy=round(46.99+i*3.81,2);a=pin(core,sig,251.46,yy,0);b=pin(left,sig,312.42,yy,180)
  if sig=='MOTOR_RUN':wire(a,(285.75,yy),b)
  else:wire(a,b)
 for i,sig in enumerate(rightsignals):
  yy=round(115.57+i*3.81,2);b=pin(right,sig,312.42,yy,180)
  if sig=='MOTOR_RUN':wire((285.75,54.61),(285.75,yy),b)
  else:wire(pin(core,sig,251.46,yy,0),b)
 d.append(n('junction',xy('at',285.75,54.61),n('diameter',num(0)),n('color',num(0),num(0),num(0),num(0)),n('uuid',uid('overview/motor-run-junction'))))
 for sh,y in [(left,77.47),(right,146.05)]:stub(sh,'VM',312.42,y);stub(sh,'V3V3',312.42,round(y+3.81,2))
 stub(sheets['07_TFT_Buttons.kicad_sch'],'V3V3_EXT',38.1,87.63);stub(sheets['07_TFT_Buttons.kicad_sch'],'V3V3',38.1,91.44)
 stub(sheets['08_Grayscale_Cameras.kicad_sch'],'V5_EXT',38.1,176.53)
 for name,y in [('V5_EXT',236.22),('V3V3_EXT',240.03),('V3V3',243.84),('VM',247.65)]:stub(sheets['09_Bluetooth_Stepper.kicad_sch'],name,38.1,y)
 box(power,312.42,179.07,83.82,25.4);box(aux,312.42,218.44,83.82,25.4)
 wire(pin(core,'V5_SYS',251.46,185.42,0),pin(power,'V5_SYS',312.42,185.42,180));label('V5_SYS',(270.51,185.42))
 a=pin(core,'V3V3',251.46,193.04,0);wire(a,(276.86,193.04));label('V3V3',(276.86,193.04))
 wire(pin(power,'VM',312.42,196.85,180),(293.37,196.85),(293.37,224.79),pin(aux,'VM',312.42,224.79,180));label('VM',(293.37,210.82))
 stub(aux,'V5_EXT',312.42,228.6);stub(aux,'V3V3_EXT',312.42,236.22)
 note('小车扩展板 M2：信号逐路层次连接\nMCU_TX 接模块 RX，所有逻辑三伏三\n公共电源按同名网络连接，完整针序见功能页',17.78,15.24)
 note('98 触点\n44 路 GPIO\n硬件停机回路',208.28,87.63)
 note('SPI1 与四键\n双孔安装',43.18,57.15)
 note('八路灰度\nMaixCAM 检测\n独立图传',43.18,130.81)
 note('HC-08\n外接步进驱动',43.18,212.09)
 note('MG513\n左电机与编码器',353.06,54.61)
 note('MG513\n右电机与编码器',353.06,123.19)
 note('XT60 入口\n保护、开关与五伏',340.36,186.69)
 note('独立外设电源\n五伏 / 三伏三',340.36,226.06)
 write(file,d)

def hierarchical_new_signals():
 # Convert existing GPIO global-label objects in place: same attachment and UUID.
 # Their new sheet pins are actually wired on the overview, so these signals no
 # longer rely on an invisible global connection between Core and peripherals.
 groups=overview_signal_groups();groups['03_Core_Interface.kicad_sch']=[r['signal'] for r in csv.DictReader((ROOT/'docs/m2_pin_assignment.csv').open(encoding='utf-8-sig'))]
 for file,signals in groups.items():
  d=parse((ROOT/file).read_text(encoding='utf-8'));found=[]
  for z in nodes(d,'global_label'):
   if z[1] not in signals:continue
   z[0]=Atom('hierarchical_label');found.append(z[1])
   for p in list(nodes(z,'property')):z.remove(p)
  assert sorted(found)==sorted(signals),(file,found)
  write(file,d)

def main():
 if '--apply-current-baseline' not in sys.argv:raise SystemExit('Historical layout edit only; explicit --apply-current-baseline required. Use KiCad for subsequent edits.')
 previous=ROOT/'docs/layout_edit.json'
 if previous.exists():
  expected=json.loads(previous.read_text(encoding='utf-8'))['source_sha256']
 else:expected={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in BASE.glob('*.kicad_sch')}
 for file,digest in expected.items():
  if hashlib.sha256((ROOT/file).read_bytes()).hexdigest()!=digest:raise SystemExit('Refusing to overwrite later manual edits in '+file)
 consistent_existing();overview_power_flow();auxiliary_power();screen_keys();gray_cameras();bluetooth_stepper();hierarchical_new_signals()
 record={'date':'2026-11-06','revision':'M2.1','baseline':'docs/baseline-2026-11-06-layout','scope':'Schematic presentation only; no PCB access','source_sha256':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in ROOT.glob('*.kicad_sch')}}
 (ROOT/'docs/layout_edit.json').write_text(json.dumps(record,indent=2)+'\n',encoding='utf-8')
 print('A3 wired overview and eight A4 functional pages tidied; electrical verification required.')
if __name__=='__main__':main()
