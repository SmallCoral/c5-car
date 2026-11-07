"""Apply the M2 extension to an explicitly saved pre-edit schematic baseline.

Never imports/runs build_schematic.py, never accesses a PCB. Existing component
UUIDs and positions are retained; only the A4 overview is reflowed.
Re-execution deliberately rebuilds from the saved baseline, not a live design.
"""
from pathlib import Path
import sys,copy,json,csv,math,hashlib
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT.parent/'Shared/design_tools'))
import schematic_primitives as h
from sexpr import parse,dump,node,nodes,prop,Atom
from pin_functions import parse_alternate_functions
from schematic_primitives import n,num,pt,s,xy,effects,property_,uid,Page,std,chip
BASE=ROOT/'docs/baseline-2026-11-06-m2'
if '--apply-saved-baseline' not in sys.argv:
 raise SystemExit('Historical M2 migration only. Use KiCad for subsequent edits; explicit --apply-saved-baseline and matching saved hashes are required to replay.')
previous=json.loads((ROOT/'docs/m2_design.json').read_text(encoding='utf-8'))
expected=previous.get('authored_source_hashes',{})
if not expected:raise SystemExit('No reviewed replay hashes; refusing to overwrite a live schematic.')
for rel,digest in expected.items():
 if not (ROOT/rel).is_file() or hashlib.sha256((ROOT/rel).read_bytes()).hexdigest()!=digest:
  raise SystemExit('Live design was edited; refusing to overwrite '+rel)
h.ROOT=ROOT;h.fpdir=ROOT/'Motor.pretty';h.md=ROOT/'Models'
root=parse((BASE/'CoralC5-Motor.kicad_sch').read_text(encoding='utf-8'))
h.ROOT_UUID=node(root,'uuid')[1]
oldlib=parse((BASE/'Motor.kicad_sym').read_text(encoding='utf-8'))
for z in nodes(oldlib,'symbol'):h.LIB['Motor:'+z[1]]=copy.deepcopy(z)
FP={key:'Motor:'+val for key,val in {
 'R':'Resistor_SMD__R_0603_1608Metric','C':'Capacitor_SMD__C_0603_1608Metric',
 'C0805':'Capacitor_SMD__C_0805_2012Metric','C1206':'Capacitor_SMD__C_1206_3216Metric',
 'L':'Inductor_SMD__L_Bourns_SRN6045TA','TSOT':'Package_TO_SOT_SMD__TSOT-23-6',
 'M2':'TerminalBlock_Phoenix__TerminalBlock_Phoenix_MKDS-1,5-2-5.08_1x02_P5.08mm_Horizontal',
 'PH2':'Connector_PinHeader_2.54mm__PinHeader_1x02_P2.54mm_Vertical'}.items()}
for count in [3,4,6,8,10]:
 FP['PH'+str(count)]=h.local_fp(f'Connector_PinHeader_2.54mm:PinHeader_1x{count:02}_P2.54mm_Vertical')
FP['LDO']=h.local_fp('Package_TO_SOT_SMD:SOT-23-5')
FP['BUTTON']=h.local_fp('Button_Switch_THT:SW_PUSH_6mm_H4.3mm')
# XT60PW library geometry. Its exact model is absent from this KiCad installation;
# retain a truthful missing-model exception instead of attaching a vertical XT60.
xtname='AMASS_XT60PW-M_1x02_P7.20mm_Horizontal'
xf=parse((h.K/'footprints/Connector_AMASS.pretty'/f'{xtname}.kicad_mod').read_text(encoding='utf-8'))
for z in nodes(xf,'model'):xf.remove(z)
(h.fpdir/(xtname+'.kicad_mod')).write_text(dump(xf)+'\n',encoding='utf-8')
FP['XT60']='Motor:'+xtname
FP['HOLE']=h.local_fp('MountingHole:MountingHole_2mm')
R=std('Device','R');C=std('Device','C');L=std('Device','L')
AP=std('Regulator_Switching','AP63205WU');LDO=std('Regulator_Linear','AP2112K-3.3')
SW=std('Switch','SW_Push');HOLE=std('Mechanical','MountingHole')
contract=list(csv.DictReader((ROOT.parent/'Shared/interface.csv').open(encoding='utf-8-sig')))
af=parse_alternate_functions((ROOT.parent/'Shared/sources/DS15137_STM32C5Axxx_Datasheet_Rev3.pdf.txt').read_text(encoding='utf-8-sig'))
roles=[]
def role(signal,gpio,function,direction,endpoint):
 row=next(x for x in contract if x['gpio']==gpio)
 a=af.get((gpio,function)) if function!='GPIO' else None
 assert function=='GPIO' or a is not None,(gpio,function)
 roles.append(dict(signal=signal,gpio=gpio,contact=row['contact'],mcu_pin=row['mcu_pin'],function=function,af=a,direction=direction,endpoint=endpoint))
for prefix,tx,rx,usart,j in [('BT','PA2','PA3',2,'J15'),('MAIX','PD8','PD9',3,'J18'),('VIDEO','PC6','PC7',6,'J19'),('STEP_UART','PB6','PB7',1,'J22')]:
 role(prefix+'_TX',tx,f'USART{usart}_TX','output',j+':3')
 role(prefix+'_RX',rx,f'USART{usart}_RX','input',j+':4')
for sig,gpio,func,pin in [('TFT_SCK','PA5','SPI1_SCK',3),('TFT_MOSI','PA7','SPI1_MOSI',4),('TFT_RES','PB0','GPIO',5),('TFT_DC','PB1','GPIO',6),('TFT_CS','PA4','GPIO',7),('TFT_BL','PB4','TIM3_CH1',8)]:
 role(sig,gpio,func,'output',f'J17:{pin}')
for i,gpio in enumerate(['PE10','PE11','PE12','PE13','PE14','PE15','PD10','PD11'],1):role(f'GRAY_CH{i}',gpio,'GPIO','input',f'J20:{i+2}')
role('GRAY_CLK','PD12','GPIO','output','J21:3');role('GRAY_DAT','PD13','GPIO','input','J21:4')
for sig,gpio,func,pin in [('STEP_PUL','PE9','TIM1_CH1',3),('STEP_DIR','PD14','GPIO',4),('STEP_nEN','PD15','GPIO',5)]:role(sig,gpio,func,'output',f'J22:{pin}')
# UART header is independent from the pulse header.
next(x for x in roles if x['signal']=='STEP_UART_TX')['endpoint']='J23:3'
next(x for x in roles if x['signal']=='STEP_UART_RX')['endpoint']='J23:4'
for i,gpio in enumerate(['PC4','PC5','PB2','PE7'],2):role(['KEY_MODE','KEY_NEXT','KEY_OK','KEY_START'][i-2],gpio,'GPIO','input',f'SW{i}:1')
oldroles=json.loads((BASE/'design_manifest.json').read_text(encoding='utf-8-sig'))['motor_pin_assignment']['entries']
assert len({r['gpio'] for r in roles+oldroles})==len(roles)+len(oldroles)
assert not {r['gpio'] for r in roles}&{'PA9','PA11','PA12','PA13','PA14','PH2','PE3','PD2'}
with (ROOT/'docs/m2_pin_assignment.csv').open('w',encoding='utf-8-sig',newline='') as f:
 w=csv.DictWriter(f,fieldnames=list(roles[0]));w.writeheader();w.writerows(roles)

def page(file,title,pageno):
 p=Page(file,title,pageno,paper='A4')
 node(node(p.data,'title_block'),'date')[1]='2026-11-06';node(node(p.data,'title_block'),'rev')[1]='M2'
 return p
def conn(p,ref,names,x,y,fp=None,value=None):
 key=chip('M2_'+ref,'J',value or ref,[(i,name,-10.16,(len(names)-1)*1.27-(i-1)*2.54,0,'passive') for i,name in enumerate(names,1)],fp or FP['PH'+str(len(names))],rect=(-5.08,len(names)*1.27,5.08,-len(names)*1.27))
 placed=p.place(key,ref,value or ref,x,y,fp or FP['PH'+str(len(names))],refpos=(x,y-len(names)*1.27-6.35),valpos=(x,y-len(names)*1.27-3.81))
 return {int(k):v for k,v in placed.items()}
def end(p,pin,name,dx=-12.7):
 q=pt(pin[0]+dx,pin[1]);p.wire(pin,q)
 p.label(name,q,0 if dx<0 else 180)
def port(p,name,where,shape='bidirectional'):
 if name in {r['signal'] for r in roles}:return
 p.port(name,where,shape,angle=180)
 # Put the local alias at a separate endpoint, clear of the port text.
 q=pt(where[0]-60.96,where[1]);p.wire(q,where);p.label(name,q)
def title(p,t):p.note(t,15.24,13.97)
def finish(p):
 for z in nodes(p.data,'label'):
  if z[1] in {r['signal'] for r in roles}:
   z[0]=Atom('global_label');z.insert(2,n('shape',Atom('bidirectional')))
   node(z,'at')[3]=num((float(node(z,'at')[3])+180)%360)
   node(z,'effects')[1:]=effects(1.016,'left' if float(node(z,'at')[3])==0 else 'right')[1:]
 p.gnd((20.32,190.5));p.label('GND',(20.32,190.5));p.save();return p

# External power: separate from the Core 3V3 and USB-powered Core rail.
p=page('06_Module_Power.kicad_sch','外设独立五伏与三伏三供电',6);title(p,'外设电源：摄像头、灰度与人机界面')
port(p,'VM',(76.2,25.4),'input');p.label('VM',(20.32,40.64))
u=p.place(AP,'U6','AP63205WU-7',106.68,63.5,FP['TSOT'],refpos=(106.68,50.8),valpos=(106.68,53.34))
cin=p.place(C,'C50','10uF/25V X7R',55.88,62.23,FP['C1206'])
boot=p.place(C,'C51','100nF/50V',134.62,76.2,FP['C'])
ind=p.place(L,'L2','4.7uH Isat>=3.5A',158.75,60.96,FP['L'],rot=90)
co=p.place(C,'C52','22uF/10V X7R',187.96,72.39,FP['C1206'])
co2=p.place(C,'C53','22uF/10V X7R',220.98,72.39,FP['C1206'])
p.wire((20.32,40.64),(55.88,40.64),(55.88,cin['1'][1]));p.wire((55.88,40.64),(u['3'][0],40.64),u['3'])
p.wire((55.88,40.64),(76.2,40.64),(76.2,u['2'][1]),u['2'])
p.wire(u['6'],(boot['1'][0],u['6'][1]),boot['1'])
p.wire(u['5'],(149.86,u['5'][1]),(149.86,boot['2'][1]),boot['2'])
p.wire(u['5'],ind['1']);p.wire(ind['2'],(220.98,ind['2'][1]),co2['1']);p.wire((187.96,ind['2'][1]),co['1'])
p.wire(u['1'],(u['1'][0],91.44),(233.68,91.44),(233.68,ind['2'][1]),ind['2'])
p.wire(cin['2'],(cin['2'][0],99.06),(220.98,99.06),co2['2']);p.wire(u['4'],(u['4'][0],99.06));p.wire(co['2'],(co['2'][0],99.06));p.gnd((106.68,99.06))
p.port('V5_EXT',(254,60.96),'output',angle=180);p.wire((220.98,60.96),(241.3,60.96),(254,60.96));p.flag((241.3,60.96))
ld=p.place(LDO,'U7','AP2112K-3.3TRG1',109.22,137.16,FP['LDO'],refpos=(109.22,124.46),valpos=(109.22,127))
ci=p.place(C,'C54','1uF/10V X7R',55.88,143.51,FP['C'])
cl=p.place(C,'C55','1uF/10V X7R',175.26,143.51,FP['C'])
p.wire((30.48,134.62),ld['1']);p.label('V5_EXT',(30.48,134.62));p.wire((55.88,134.62),ci['1'])
p.wire(ld['3'],(76.2,ld['3'][1]),(76.2,134.62));p.wire(ld['5'],(241.3,ld['5'][1]));p.wire((175.26,ld['5'][1]),cl['1'])
p.port('V3V3_EXT',(241.3,134.62),'output',angle=180);p.nc(ld['4'])
p.wire(ci['2'],(55.88,160.02),(175.26,160.02),cl['2']);p.wire(ld['2'],(ld['2'][0],160.02));p.gnd((109.22,160.02))
p.note('五伏外设总预算 1.5A；芯片额定 2A，须验证散热与启动峰值。',17.78,108)
p.note('三伏三外设预算 0.15A，与核心板电源分开。\n四安保险保留，上游总容量须核算。',17.78,172.72)
finish(p)

p=page('07_TFT_Buttons.kicad_sch','中景园屏幕与四个操作按钮',7);title(p,'任务切换界面：屏幕与操作键')
j=conn(p,'J17',['GND','VCC_3V3','SCL','SDA','RES','DC','CS','BL'],85.09,64.77,value='TFT_ZJY180S0800TG01')
for i,net in enumerate(['GND','V3V3_EXT','TFT_SCK','TFT_MOSI','TFT_RES','TFT_DC','TFT_CS','TFT_BL'],1):end(p,j[i],net)
cp=p.place(C,'C56','100nF/10V',139.7,53.34,FP['C']);end(p,cp['1'],'V3V3_EXT');end(p,cp['2'],'GND')
for ref,net,x in [('R55','TFT_CS',121.92),('R56','TFT_RES',153.67)]:
 rr=p.place(R,ref,'10k',x,76.2,FP['R']);end(p,rr['1'],'V3V3_EXT');end(p,rr['2'],net)
for i,ref in enumerate(['H1','H2']):
 p.place(HOLE,ref,'TFT孔 D2.0',195.58+i*40.64,48.26,FP['HOLE'])
 node(p.components[(ref,1)]['symbol'],'in_bom')[1]=Atom('no')
p.note('仅供三伏三；背光已有驱动，可接定时器 PWM。\n八针顺序与屏幕资料一致，丝印由引脚一开始。',17.78,95.25)
p.note('两孔选用同一短边：按边距推算 30mm。\n模块孔径 2.0mm，外形 35×56mm。\nH1/H2 不接地；孔位与实物复核后定位。',164,66.04)
for i,sig in enumerate(['KEY_MODE','KEY_NEXT','KEY_OK','KEY_START']):
 x=40.64+i*62.23;sw=p.place(SW,f'SW{i+2}',['模式','下一项','确认','启动'][i],x,132.08,FP['BUTTON'])
 rr=p.place(R,f'R{50+i}','10k',x-12.7,113.03,FP['R'])
 cc=p.place(C,f'C{57+i}','100nF',x+16.51,144.78,FP['C'])
 p.wire(sw['1'],(x-12.7,sw['1'][1]),rr['2']);p.label(sig,(x-12.7,sw['1'][1]));end(p,rr['1'],'V3V3')
 p.wire((x-12.7,sw['1'][1]),(x-12.7,139.7),(x+16.51,139.7),cc['1']);p.wire(sw['2'],(x+25.4,sw['2'][1]),(x+25.4,153.67),(x+16.51,153.67),cc['2']);p.gnd((x+25.4,153.67))
p.note('按键低有效：十千欧上拉与约一毫秒滤波，软件仍须消抖。',17.78,181.61)
for i,name in enumerate(['V3V3_EXT','V3V3','TFT_SCK','TFT_MOSI','TFT_RES','TFT_DC','TFT_CS','TFT_BL','KEY_MODE','KEY_NEXT','KEY_OK','KEY_START']):
 port(p,name,(272.0,25.4+i*5.08))
finish(p)

p=page('08_Grayscale_Cameras.kicad_sch','八路灰度与双摄像头模块接口',8);title(p,'底部八路灰度与两路独立相机模块')
j=conn(p,'J20',['5V','GND']+[f'CH{i}' for i in range(1,9)],69.85,60.96,value='灰度并行_辅助板')
for i,net in enumerate(['V5_EXT','GND']+[f'GRAY_CH{i}' for i in range(1,9)],1):end(p,j[i],net)
j=conn(p,'J21',['5V','GND','CLK','DAT'],156.21,48.26,value='灰度同步串行_选用')
for i,net in enumerate(['V5_EXT','GND','GRAY_CLK','GRAY_DAT'],1):end(p,j[i],net)
p.note('辅助板已处理灰度；并行、同步串行择一接线。\nCH1～CH8 与 DAT 使用无上下拉输入，共地。\n五伏为本体与辅助板整套供电，逻辑按三伏三。',98,69.85)
for x,jref,prefix,value in [(67.31,'J18','MAIX','MaixCAM_检测'),(205.74,'J19','VIDEO','图传处理板')]:
 j=conn(p,jref,['5V','GND','MCU_TX','MCU_RX'],x,127,value=value)
 for i,net in enumerate(['V5_EXT','GND',prefix+'_TX',prefix+'_RX'],1):end(p,j[i],net)
 c=p.place(C,'C'+('61' if jref=='J18' else '62'),'10uF/10V',x+35.56,127,FP['C0805']);end(p,c['1'],'V5_EXT');end(p,c['2'],'GND')
p.note('相机口为处理模块的电源与串口，不是裸 OV2640。\nMCU_TX 接模块 RX，MCU_RX 接模块 TX；均为三伏三逻辑。',17.78,158.75)
for i,name in enumerate(['V5_EXT']+[f'GRAY_CH{k}' for k in range(1,9)]+['GRAY_CLK','GRAY_DAT','MAIX_TX','MAIX_RX','VIDEO_TX','VIDEO_RX']):port(p,name,(274.32,26.67+i*5.08))
finish(p)

p=page('09_Bluetooth_Stepper.kicad_sch','四脚蓝牙与外接步进驱动接口',9);title(p,'蓝牙通讯与摆杆外置步进驱动')
j=conn(p,'J15',['VBT','GND','MCU_TX','MCU_RX'],67.31,48.26,value='HC-08_四脚接口')
for i,net in enumerate(['VBT','GND','BT_TX','BT_RX'],1):end(p,j[i],net)
j=conn(p,'J16',['3V3_EXT','VBT','5V_EXT'],167.64,48.26,value='蓝牙电源选择')
for i,net in enumerate(['V3V3_EXT','VBT','V5_EXT'],1):end(p,j[i],net)
p.note('J16 默认装 1—2 跳帽，选三伏三。\n仅确认带稳压底板后才可选 2—3 的五伏。\nJ15 按自定义针序制线，不猜模块正反方向。',17.78,73.66)
j=conn(p,'J22',['3V3_REF','GND','STEP','DIR','nEN','GND'],63.5,127,value='步进_脉冲控制')
for i,net in enumerate(['V3V3','GND','STEP_PUL','STEP_DIR','STEP_nEN','GND'],1):end(p,j[i],net)
j=conn(p,'J23',['3V3_REF','GND','MCU_TX','MCU_RX'],172.72,124.46,value='步进_UART控制')
for i,net in enumerate(['V3V3','GND','STEP_UART_TX','STEP_UART_RX'],1):end(p,j[i],net)
j=conn(p,'J24',['VM','GND'],250.19,127,fp=FP['M2'],value='步进驱动电源')
end(p,j[1],'VM');end(p,j[2],'GND')
# Boot default disables an active-low-enable external driver. Adapter must match
# the external driver; this port does not directly drive an optocoupler input.
rr=p.place(R,'R54','10k',99.06,130.81,FP['R']);end(p,rr['1'],'V3V3');end(p,rr['2'],'STEP_nEN')
for ref,net,x in [('R57','STEP_PUL',99.06),('R58','STEP_DIR',130.81)]:
 rr=p.place(R,ref,'10k',x,104.14,FP['R']);end(p,rr['1'],net);end(p,rr['2'],'GND')
p.note('预留外接驱动板，不能直接接电机六针线圈。\n脉冲或串口选一种；三伏三参考脚不供电机。\n光耦驱动须外置电平转换，ENA 极性按驱动匹配。',17.78,158.75)
for i,name in enumerate(['V5_EXT','V3V3_EXT','V3V3','VM','BT_TX','BT_RX','STEP_UART_TX','STEP_UART_RX','STEP_PUL','STEP_DIR','STEP_nEN']):port(p,name,(275.59,25.4+i*5.08))
finish(p)

def write(file,d): (ROOT/file).write_text(dump(d)+'\n',encoding='utf-8')
def update_title(d):
 node(node(d,'title_block'),'date')[1]='2026-11-06';node(node(d,'title_block'),'rev')[1]='M2'
def wire(d,a,b,key):d.append(n('wire',n('pts',xy('xy',*a),xy('xy',*b)),n('stroke',n('width',num(0)),n('type',Atom('default'))),n('uuid',uid(key))))
def label(d,name,at,angle=0):d.append(n('label',name,xy('at',*at,num(angle)),effects(1.016,'left bottom' if angle==0 else 'right bottom'),n('uuid',uid(str(at)+name))))
# Replace J1 alone. The new symbol keeps the two existing wire endpoints.
d=parse((BASE/'02_Power.kicad_sch').read_text(encoding='utf-8'));update_title(d)
key=chip('XT60PW_M_Power','J','XT60PW-M',[(1,'BAT+',7.62,2.54,180,'passive'),(2,'GND',7.62,-2.54,180,'passive')],FP['XT60'],rect=(-5.08,5.08,2.54,-5.08),datasheet='https://www.china-amass.com/biao/289.html')
j=next(z for z in nodes(d,'symbol') if prop(z,'Reference')[2]=='J1');node(j,'lib_id')[1]=key
prop(j,'Value')[2]='XT60PW-M';prop(j,'Footprint')[2]=FP['XT60'];prop(j,'Datasheet')[2]='https://www.china-amass.com/biao/289.html'
if prop(j,'MPN'):prop(j,'MPN')[2]='XT60PW-M'
if prop(j,'LCSC'):j.remove(prop(j,'LCSC'))
for z in nodes(j,'pin'):
 if z[1]=='2':j.remove(z)
 elif z[1]=='3':z[1]='2'
for z in nodes(d,'no_connect'):
 if tuple(map(float,node(z,'at')[1:3]))==(33.02,34.29):d.remove(z)
emb=copy.deepcopy(h.LIB[key]);emb[1]=key;node(d,'lib_symbols').append(emb)
for z in nodes(d,'text'):
 if 'DC' in z[1] or '插座' in z[1]:z[1]=z[1].replace('DC005','XT60PW-M').replace('DC-005','XT60PW-M')
write('02_Power.kicad_sch',d)

# Extend J14 by removing NC marks only on newly allocated contacts. Existing
# motor routing, all reference numbers and symbol positions remain untouched.
d=parse((BASE/'03_Core_Interface.kicad_sch').read_text(encoding='utf-8'));update_title(d)
lib=next(z for z in nodes(node(d,'lib_symbols'),'symbol') if z[1].endswith('CoralC5_Streamline_Socket'))
pins={}
for inst in nodes(d,'symbol'):
 if prop(inst,'Reference')[2]!='J14':continue
 unit=int(node(inst,'unit')[1]);x,y,a=map(float,node(inst,'at')[1:4]);assert a==0
 for sub in nodes(lib,'symbol'):
  if not sub[1].endswith(f'_{unit}_1'):continue
  for pn in nodes(sub,'pin'):
   px,py,pa=map(float,node(pn,'at')[1:]);pins[node(pn,'number')[1]]=(pt(x+px,y-py),int(pa))
oldaliases={'AUX_UART_TX','AUX_UART_RX','AUX_ADC','AUX_GPIO','AUX_I2C_SCL','AUX_I2C_SDA'}
for z in nodes(d,'label'):
 if z[1] in oldaliases:z[1]=next(r['signal'] for r in roles if r['contact']=={'AUX_UART_TX':'A2','AUX_UART_RX':'B2','AUX_ADC':'B3','AUX_GPIO':'B4','AUX_I2C_SCL':'A30','AUX_I2C_SDA':'A29'}[z[1]])
for r in roles:
 pos,angle=pins[r['contact']]
 for z in list(nodes(d,'no_connect')):
  if tuple(map(float,node(z,'at')[1:3]))==pos:d.remove(z)
 if r['contact'] not in ['A2','B2','B3','B4','A30','A29']:
  q=pt(pos[0]+(-8.89 if angle==0 else 8.89),pos[1]);wire(d,pos,q,'j14-'+r['contact']);label(d,r['signal'],q,0 if angle==0 else 180)
# New nets use global names at J14 and overview sheet pins. This keeps the
# manually placed stop circuit and all existing component positions untouched.
for z in nodes(d,'label'):
 if z[1] in {r['signal'] for r in roles}:
  z[0]=Atom('global_label');z.insert(2,n('shape',Atom('bidirectional')))
  node(z,'at')[3]=num((float(node(z,'at')[3])+180)%360)
  node(z,'effects')[1:]=effects(1.016,'left' if float(node(z,'at')[3])==0 else 'right')[1:]
# J9 was manually deleted before M2; reset has no expansion-board load.
for z in list(nodes(d,'label')):
 if z[1]=='NRST':d.remove(z)
nrpos=pins['A12'][0]
for w in list(nodes(d,'wire')):
 if nrpos in [tuple(map(float,pp[1:])) for pp in node(w,'pts')[1:]]:d.remove(w)
d.append(n('no_connect',xy('at',*nrpos),n('uuid',uid('unused-reset'))))
write('03_Core_Interface.kicad_sch',d)

# Reflow overview only; preserve original sheet and sheet-pin UUIDs.
update_title(root);node(root,'paper')[1]='A4'
for z in list(root[1:]):
 if isinstance(z,list) and z[0] in ['wire','label','junction','text','rectangle','polyline']:root.remove(z)
node(node(root,'title_block'),'title')[1]='Coral-C5 小车扩展板 M2 总览'
root.append(n('text','小车扩展板 M2：外设使用独立电源\nMCU_TX 接外设 RX，所有逻辑三伏三\n新增信号用全局网络名连接，针序见接口页',xy('at',17.78,15.24,num(0)),effects(2.54,'left top'),n('uuid',uid('root-notes'))))
sheet_by_file={prop(z,'Sheetfile')[2]:z for z in nodes(root,'sheet')}
positions={'03_Core_Interface.kicad_sch':(20.32,40.64,60.96,92.71),'02_Power.kicad_sch':(120.65,99.06,62.23,25.4),'04_Left_Motor.kicad_sch':(120.65,40.64,62.23,43.18),'05_Right_Motor.kicad_sch':(215.9,40.64,60.96,43.18),
 '06_Module_Power.kicad_sch':(215.9,99.06,60.96,25.4),'07_TFT_Buttons.kicad_sch':(33.02,151.13,60.96,25.4),'08_Grayscale_Cameras.kicad_sch':(109.22,143.51,60.96,20.32),'09_Bluetooth_Stepper.kicad_sch':(215.9,139.7,60.96,21.59)}
for p in h.PAGES:
 sh=n('sheet',xy('at',0,0),xy('size',70,50),n('stroke',n('width',num(.254)),n('type',Atom('default'))),n('fill',n('color',num(0),num(0),num(0),num(0))),n('uuid',p.sheetuuid),property_('Sheetname',p.title,0,0),property_('Sheetfile',p.file,0,0))
 root.append(sh);sheet_by_file[p.file]=sh
 # Root names are readable, sheet pin names encode the actual electrical ports.
 for z in nodes(p.data,'hierarchical_label'):
  sh.append(n('pin',z[1],Atom('bidirectional'),xy('at',0,0,num(180)),effects(1.016,'right'),n('uuid',uid(p.file+'/sheetpin/'+z[1]))))
for file,sh in sheet_by_file.items():
 x,y,w,ht=positions[file];node(sh,'at')[1:]=[num(x),num(y)];node(sh,'size')[1:]=[num(w),num(ht)]
 for pn,yy in [('Sheetname',y-2.54),('Sheetfile',y+ht+2.54)]:
  pp=prop(sh,pn);node(pp,'at')[1:]=[num(x),num(yy),num(0)];node(pp,'effects')[1:]=effects(1.27,'left')[1:]
 ppins=nodes(sh,'pin')
 for i,pin in enumerate(ppins):
  right=file=='03_Core_Interface.kicad_sch';at=pt(x+w if right else x,y+6.35+i*3.81)
  node(pin,'at')[1:]=[num(at[0]),num(at[1]),num(0 if right else 180)]
  node(pin,'effects')[1:]=effects(1.016,'right' if right else 'left')[1:]
  ep=pt(at[0]+(5.08 if right else -5.08),at[1]);wire(root,at,ep,'root-wire/'+file+'/'+pin[1]);label(root,pin[1],ep,0 if right else 180)
for z in nodes(root,'label'):
 if z[1] in {r['signal'] for r in roles}:
  z[0]=Atom('global_label');z.insert(2,n('shape',Atom('bidirectional')))
# Seven short real connections between adjacent Core and left motor blocks.
direct={p[1] for p in nodes(sheet_by_file['04_Left_Motor.kicad_sch'],'pin')[:7]}
for z in list(nodes(root,'label')):
 if z[1] in direct:root.remove(z)
for w in list(nodes(root,'wire')):
 if any(node(w,'uuid')[1]==uid('root-wire/'+file+'/'+sig) for file in ['03_Core_Interface.kicad_sch','04_Left_Motor.kicad_sch'] for sig in direct):root.remove(w)
for sig in direct:
 a=next(p for p in nodes(sheet_by_file['03_Core_Interface.kicad_sch'],'pin') if p[1]==sig)
 b=next(p for p in nodes(sheet_by_file['04_Left_Motor.kicad_sch'],'pin') if p[1]==sig)
 wire(root,tuple(map(float,node(a,'at')[1:3])),tuple(map(float,node(b,'at')[1:3])),'direct/'+sig)
# MOTOR_RUN also serves the right channel.
at=tuple(map(float,node(next(p for p in nodes(sheet_by_file['05_Right_Motor.kicad_sch'],'pin') if p[1]=='MOTOR_RUN'),'at')[1:3]))
label(root,'MOTOR_RUN',(at[0]-5.08,at[1]),180)
mid=tuple(map(float,node(next(p for p in nodes(sheet_by_file['03_Core_Interface.kicad_sch'],'pin') if p[1]=='MOTOR_RUN'),'at')[1:3]))
label(root,'MOTOR_RUN',mid)
for text,x,y in [('98 触点\n13 路电机\n31 路新增',27.94,60.96),('MG513＋编码器',148.59,58.42),('MG513＋编码器',243.84,58.42),('XT60 入口',149.86,111.76),('五伏 / 三伏三',242.57,111.76),('SPI1＋四键',60.96,165.1),('八路灰度＋双摄',125.73,154.94),('蓝牙串口\n步进脉冲 / 串口',241.3,147.32)]:
 root.append(n('text',text,xy('at',x,y,num(0)),effects(2.54,'left top'),n('uuid',uid('overview/'+text+str(x)))))
write('CoralC5-Motor.kicad_sch',root)
# Save local symbol library without altering pre-existing definitions.
alllib=n('kicad_symbol_lib',n('version',num(20241209)),n('generator','kicad_symbol_editor'))
alllib += [copy.deepcopy(v) for v in h.LIB.values()]
(ROOT/'Motor.kicad_sym').write_text(dump(alllib)+'\n',encoding='utf-8')
(ROOT/'docs/m2_design.json').write_text(json.dumps({'date':'2026-11-06','roles':roles,'new_parts':h.PARTS,'footprints':h.FPMETA,'model_missing':['J1: exact XT60PW-M horizontal model'],'mounting_holes':{'references':['H1','H2'],'diameter_mm':2,'center_distance_mm':30,'edge_offset_mm':2.5,'derivation':'35 - 2 * 2.5 = 30mm; confirm actual module before PCB placement','source':'ZJY180S0800TG01 Rev A p5; two holes on one short side'},'all_sheets_paper':'A4'},ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
metadata=json.loads((ROOT/'docs/m2_design.json').read_text(encoding='utf-8'))
paths=list(ROOT.glob('*.kicad_sch'))+[ROOT/'Motor.kicad_sym']+list((ROOT/'Motor.pretty').glob('*.kicad_mod'))+list((ROOT/'Models').glob('*'))
metadata['authored_source_hashes']={p.relative_to(ROOT).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in paths if p.is_file()}
(ROOT/'docs/m2_design.json').write_text(json.dumps(metadata,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print('M2 extension written:',len(roles),'new signals, 9 sheets; no PCB accessed')
