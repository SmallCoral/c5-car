"""One-time, guarded M2.2 edit for the user's confirmed ZDT_XS TTL kit.

Edits the saved live schematics and the one affected local symbol only.
No PCB operations, no earlier design generators.
"""
from pathlib import Path
import copy,csv,hashlib,json,sys,uuid
ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'docs/baseline-2026-11-06-zdt-ttl'
sys.path.insert(0,str(ROOT.parent/'Shared/design_tools'))
from sexpr import parse,dump,node,nodes,prop,Atom
from schematic_primitives import n,num,xy,effects

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def uid(s):return str(uuid.uuid5(uuid.NAMESPACE_URL,'CoralC5-ZDT-TTL-20261106/'+s))
def at(z):return tuple(map(float,node(z,'at')[1:3]))
def put(z,x,y):node(z,'at')[1:3]=[num(x),num(y)]
def save(path,d):path.write_text(dump(d)+'\n',encoding='utf-8')
def wire(d,a,b):
 assert a[0]==b[0] or a[1]==b[1]
 d.append(n('wire',n('pts',xy('xy',*a),xy('xy',*b)),n('stroke',n('width',num(0)),n('type',Atom('default'))),n('uuid',uid(str((a,b))))))

def main():
 if '--apply-saved-baseline' not in sys.argv:raise SystemExit('Historical edit only; explicit --apply-saved-baseline required.')
 files=[p.name for p in BASE.glob('*.kicad_sch')]+['Motor.kicad_sym']
 for f in files:
  if sha(ROOT/f)!=sha(BASE/f):raise SystemExit('Refusing to overwrite changes after baseline: '+f)
 if (ROOT/'docs/zdt_ttl_design.json').exists():raise SystemExit('ZDT edit already recorded; use KiCad for later edits.')
 fp='Motor:Connector_PinHeader_2.54mm__PinHeader_1x03_P2.54mm_Vertical'
 source='${KIPRJMOD}/../../../../Documentation/ZDT_XS系列第二代闭环步进电机资料/3. 说明书/ZDT_X42S第二代闭环步进电机用户手册V1.0.5_260527.pdf'
 def modify_lib(lib):
  prop(lib,'Value')[2]='ZDT_XS_TTL_3P';prop(lib,'Footprint')[2]=fp;prop(lib,'Datasheet')[2]=source
  rect=node(next(z for z in nodes(lib,'symbol') if z[1].endswith('_0_1')),'rectangle')
  node(rect,'start')[1:]=[num(-5.08),num(3.81)];node(rect,'end')[1:]=[num(5.08),num(-3.81)]
  sub=next(z for z in nodes(lib,'symbol') if z[1].endswith('_1_1'))
  for p in list(nodes(sub,'pin')):
   pn=int(node(p,'number')[1])
   if pn==1:sub.remove(p);continue
   k=pn-1;node(p,'number')[1]=str(k);node(p,'name')[1]=['GND','MCU_TX','MCU_RX'][k-1];node(p,'at')[2]=num(2.54-(k-1)*2.54)
 for file in files:
  path=ROOT/file;d=parse((BASE/file).read_text(encoding='utf-8'))
  if file.endswith('.kicad_sch'):node(node(d,'title_block'),'rev')[1]='M2.2'
  if file=='Motor.kicad_sym':modify_lib(next(z for z in nodes(d,'symbol') if z[1]=='M2_J23'))
  if file=='CoralC5-Motor.kicad_sch':
   z=next(z for z in nodes(d,'text') if z[1]=='HC-08\n外接步进驱动');z[1]='HC-08\nZDT_XS TTL 套件'
  if file=='09_Bluetooth_Stepper.kicad_sch':
   modify_lib(next(z for z in nodes(node(d,'lib_symbols'),'symbol') if z[1]=='Motor:M2_J23'))
   j=next(z for z in nodes(d,'symbol') if prop(z,'Reference')[2]=='J23')
   prop(j,'Value')[2]='ZDT_XS_TTL_3P';prop(j,'Footprint')[2]=fp;prop(j,'Datasheet')[2]=source
   for p in list(nodes(j,'pin')):
    if p[1]=='1':j.remove(p)
    else:p[1]=str(int(p[1])-1)
   # Remove only the old J23 3V3 branch and its four pin stubs.
   for w in list(nodes(d,'wire')):
    pts=[tuple(map(float,p[1:3])) for p in node(w,'pts')[1:]]
    if all(150<=p[0]<=180.34 and 110<=p[1]<=134 for p in pts):d.remove(w)
   for z in list(nodes(d,'label')):
    if z[1]=='GND' and at(z)==(162.56,128.27):put(z,162.56,127)
    if z[1]=='V3V3' and at(z)==(55.88,123.19):d.remove(z)
   port=next(z for z in nodes(d,'hierarchical_label') if z[1]=='V3V3');put(port,20.32,109.22)
   # Keep Core 3V3 on the pulse reference only, completely absent from J23.
   wire(d,(20.32,109.22),(55.88,109.22));wire(d,(55.88,109.22),(55.88,123.19))
   d.append(n('label','V3V3',xy('at',55.88,109.22,num(0)),effects(1.016,'left bottom'),n('uuid',uid('pulse-3v3-label'))))
   for sig,y in [('STEP_UART_TX',129.54),('STEP_UART_RX',132.08)]:
    z=next(z for z in nodes(d,'hierarchical_label') if z[1]==sig);put(z,162.56,y);wire(d,(162.56,y),(180.34,y))
   wire(d,(162.56,127),(180.34,127))
   z=next(z for z in nodes(d,'text') if z[1]=='外接步进驱动：脉冲或串口择一');z[1]='ZDT_XS 一体套件：TTL 主用，脉冲备用'
   z=next(z for z in nodes(d,'text') if z[1].startswith('电机须接外置驱动板'))
   z[1]='J22 脉冲备用；PLC 版 COM 不接，\n光耦版 COM 接三伏三，按共阳配置。\nEn=L 时上拉失能，默认 Hold 不受引脚控制。';put(z,17.78,171.45)
   d.append(n('text','TTL：TX 接 R/A/H，RX 接 T/B/L。\nJ24 接 V+、GND，输入最低十伏。\n厂商建议电源能力三安以上，\n本板四安总保护未提高。',xy('at',121.92,139.7,num(0)),effects(2.54,'left top'),n('uuid',uid('ttl-wiring-note'))))
  save(path,d)
 # This is a current interface contract, unlike the frozen initial M2 manifest.
 table=ROOT/'docs/m2_pin_assignment.csv';rows=list(csv.DictReader(table.open(encoding='utf-8-sig')))
 for row in rows:
  if row['signal']=='STEP_UART_TX':row['endpoint']='J23:2'
  if row['signal']=='STEP_UART_RX':row['endpoint']='J23:3'
 with table.open('w',encoding='utf-8-sig',newline='') as f:
  w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
 bom=ROOT/'docs/BOM_M2_SCHEMATIC_DRAFT.csv';rows=list(csv.DictReader(bom.open(encoding='utf-8-sig')))
 for row in rows:
  if row.get('reference')=='J23':
   row['value']='ZDT_XS_TTL_3P';row['footprint']=fp;row['datasheet']=source
 with bom.open('w',encoding='utf-8-sig',newline='') as f:
  w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
 record={'date':'2026-11-06','revision':'M2.2','user_confirmed':'ZDT_XS second-generation integrated closed-loop motor kit, TTL variant','baseline':str(BASE.relative_to(ROOT)),'manual':source,'manual_pages':[6,10,12,16,23,26],
  'J23':{'pins':{'1':'GND','2':'STEP_UART_TX','3':'STEP_UART_RX'},'footprint':fp,'driver_wires':{'1':'Gnd','2':'R/A/H (RX)','3':'T/B/L (TX)'}},
  'J24':{'pins':{'1':'VM','2':'GND'},'driver_wires':{'1':'V+','2':'Gnd'}},'J22':'6P pulse alternative; PLC COM unused, optocoupler COM=3V3; En=L required for nEN pull-up disable; default Hold ignores En',
  'intentional_pin_map':{'J23:1':None,'J23:2':'J23:1','J23:3':'J23:2','J23:4':'J23:3'},'supply_minimum_v':10,'manufacturer_recommended_source_a_minimum':3,'upstream_fuse_a_unchanged':4,'PCB_accessed':False,
  'source_sha256':{f:sha(ROOT/f) for f in files}}
 (ROOT/'docs/zdt_ttl_design.json').write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
 print('M2.2 saved; regenerate native netlist/ERC and verify before delivery.')
def upgrade_8p():
 """User's final request: one 8P connector, only VM/GND/TX/RX wired."""
 recordpath=ROOT/'docs/zdt_ttl_design.json';record=json.loads(recordpath.read_text(encoding='utf-8'))
 if record['J23']['pins']!={'1':'GND','2':'STEP_UART_TX','3':'STEP_UART_RX'}:raise SystemExit('Expected the recorded temporary 3P edit only.')
 for f,h in record['source_sha256'].items():
  if sha(ROOT/f)!=h:raise SystemExit('Refusing to overwrite later manual edits in '+f)
 snapshot=BASE/'temporary-3p';snapshot.mkdir(exist_ok=True)
 for f in ['09_Bluetooth_Stepper.kicad_sch','Motor.kicad_sym','docs/zdt_ttl_design.json','docs/m2_pin_assignment.csv','docs/BOM_M2_SCHEMATIC_DRAFT.csv','docs/erc.json']:
  (snapshot/Path(f).name).write_bytes((ROOT/f).read_bytes())
 fp='Motor:Connector_PinHeader_2.54mm__PinHeader_1x08_P2.54mm_Vertical'
 def library_8p(lib):
  prop(lib,'Value')[2]='ZDT_XS_TTL_8P';prop(lib,'Footprint')[2]=fp
  rect=node(next(z for z in nodes(lib,'symbol') if z[1].endswith('_0_1')),'rectangle')
  node(rect,'start')[2]=num(10.16);node(rect,'end')[2]=num(-10.16)
  sub=next(z for z in nodes(lib,'symbol') if z[1].endswith('_1_1'));sample=copy.deepcopy(nodes(sub,'pin')[0])
  for p in list(nodes(sub,'pin')):sub.remove(p)
  for k,name in enumerate(['VM_V+','GND','COM_NC','EN_NC','STEP_NC','DIR_NC','R/A/H_RX','T/B/L_TX'],1):
   p=copy.deepcopy(sample);node(p,'number')[1]=str(k);node(p,'name')[1]=name;node(p,'at')[2]=num(8.89-(k-1)*2.54);sub.append(p)
 for f in ['Motor.kicad_sym','09_Bluetooth_Stepper.kicad_sch']:
  d=parse((ROOT/f).read_text(encoding='utf-8'))
  lib=next(z for z in nodes(d if f.endswith('.kicad_sym') else node(d,'lib_symbols'),'symbol') if z[1] in ['M2_J23','Motor:M2_J23']);library_8p(lib)
  if f.endswith('.kicad_sch'):
   j=next(z for z in nodes(d,'symbol') if prop(z,'Reference')[2]=='J23');prop(j,'Value')[2]='ZDT_XS_TTL_8P';prop(j,'Footprint')[2]=fp
   put(prop(j,'Reference'),190.5,110.49);put(prop(j,'Value'),190.5,113.03)
   oldpins={p[1]:node(p,'uuid')[1] for p in nodes(j,'pin')}
   for p in list(nodes(j,'pin')):j.remove(p)
   for k in range(1,9):
    old={'2':'1','7':'2','8':'3'}.get(str(k));pu=oldpins[old] if old else uid('J23/8P/pin/'+str(k));j.append(n('pin',str(k),n('uuid',pu)))
   for w in list(nodes(d,'wire')):
    pts=[tuple(map(float,p[1:3])) for p in node(w,'pts')[1:]]
    if all(160<=p[0]<=180.34 and 126<=p[1]<=133 for p in pts):d.remove(w)
   g=next(z for z in nodes(d,'label') if z[1]=='GND' and at(z)==(162.56,127));put(g,162.56,123.19)
   wire(d,(162.56,123.19),(180.34,123.19));wire(d,(162.56,120.65),(180.34,120.65))
   d.append(n('label','VM',xy('at',162.56,120.65,num(0)),effects(1.016,'left bottom'),n('uuid',uid('J23/vm-label'))))
   for sig,y in [('STEP_UART_TX',135.89),('STEP_UART_RX',138.43)]:
    z=next(z for z in nodes(d,'hierarchical_label') if z[1]==sig);put(z,162.56,y);wire(d,(162.56,y),(180.34,y))
   for k in range(3,7):d.append(n('no_connect',xy('at',180.34,round(120.65+(k-1)*2.54,2)),n('uuid',uid('J23/unused/'+str(k)))))
   z=next(z for z in nodes(d,'text') if z[1].startswith('TTL：TX 接'))
   z[1]='J23 八针仅接 1、2、7、8 脚。\nTX 接 R/A/H，RX 接 T/B/L。\n电源最低十伏，建议三安以上。\n本板四安总保护未提高。';put(z,121.92,146.05)
  save(ROOT/f,d)
 table=ROOT/'docs/m2_pin_assignment.csv';rows=list(csv.DictReader(table.open(encoding='utf-8-sig')))
 for row in rows:
  if row['signal']=='STEP_UART_TX':row['endpoint']='J23:7'
  if row['signal']=='STEP_UART_RX':row['endpoint']='J23:8'
 with table.open('w',encoding='utf-8-sig',newline='') as f:
  w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
 bom=ROOT/'docs/BOM_M2_SCHEMATIC_DRAFT.csv';rows=list(csv.DictReader(bom.open(encoding='utf-8-sig')))
 for row in rows:
  if row['reference']=='J23':row['value']='ZDT_XS_TTL_8P';row['footprint']=fp;row['datasheet']=record['manual']
 with bom.open('w',encoding='utf-8-sig',newline='') as f:
  w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
 record['J23']={'pins':{'1':'VM','2':'GND','3':'NC','4':'NC','5':'NC','6':'NC','7':'STEP_UART_TX','8':'STEP_UART_RX'},'footprint':fp,'driver_wires':{'1':'V+','2':'Gnd','3':'Com (unused)','4':'En (unused)','5':'Stp (unused)','6':'Dir (unused)','7':'R/A/H (RX)','8':'T/B/L (TX)'},'active_wires':4,'pitch_mm':2.54}
 record['intentional_pin_map']={'J23:1':None,'J23:2':'J23:2','J23:3':'J23:7','J23:4':'J23:8'}
 record['new_pin_membership']={'J23:1':'VM','J23:3':'NC','J23:4':'NC','J23:5':'NC','J23:6':'NC'}
 record['user_final_request']='8P plug-in interface; use only the required wires'
 record['source_sha256']={f:sha(ROOT/f) for f in record['source_sha256']}
 recordpath.write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
 print('Final J23 8P saved; pins 1/2/7/8 wired, pins 3-6 intentionally NC.')
if __name__=='__main__':
 if '--upgrade-to-8p' in sys.argv:upgrade_8p()
 else:main()
