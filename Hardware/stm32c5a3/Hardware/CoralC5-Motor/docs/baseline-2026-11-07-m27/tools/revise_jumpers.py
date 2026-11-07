"""Guarded live edit: J10 jumper header and fixed-3V3 Bluetooth, no PCB access."""
from pathlib import Path
import copy,csv,hashlib,json,sys
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'docs/baseline-2026-11-06-jumpers'
sys.path.insert(0,str(ROOT.parent/'Shared/design_tools'))
from sexpr import parse,dump,node,nodes,prop
from schematic_primitives import n,num,xy,effects

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def at(z):return tuple(map(float,node(z,'at')[1:3]))
def save(p,d):p.write_text(dump(d)+'\n',encoding='utf-8')
def main():
 if '--apply-saved-baseline' not in sys.argv:raise SystemExit('Historical edit only; explicit --apply-saved-baseline required.')
 if (ROOT/'docs/jumpers_revision.json').exists():raise SystemExit('Already applied; use KiCad for further edits.')
 for p in [*BASE.glob('*.kicad_sch'),BASE/'Motor.kicad_sym']:
  if sha(ROOT/p.name)!=sha(p):raise SystemExit('Refusing edits after saved baseline: '+p.name)
 header='Motor:Connector_PinHeader_2.54mm__PinHeader_1x02_P2.54mm_Vertical'
 j10lib=None;removed_uuid=None;removed_port_uuid=None
 for p in ROOT.glob('*.kicad_sch'):
  d=parse(p.read_text(encoding='utf-8'));node(node(d,'title_block'),'rev')[1]='M2.4'
  if p.name=='03_Core_Interface.kicad_sch':
   z=next(z for z in nodes(d,'symbol') if prop(z,'Reference')[2]=='J10');prop(z,'Footprint')[2]=header;j10lib=node(z,'lib_id')[1]
   lib=next(z for z in nodes(node(d,'lib_symbols'),'symbol') if z[1]==j10lib);prop(lib,'Footprint')[2]=header
   z=next(z for z in nodes(d,'text') if z[1].startswith('J10装短路帽'));z[1]='J10 两针跳帽，也可接常闭停机开关。\n拔帽或复位时默认停止左右轮。'
  if p.name=='09_Bluetooth_Stepper.kicad_sch':
   j=next(z for z in nodes(d,'symbol') if prop(z,'Reference')[2]=='J16');removed_uuid=node(j,'uuid')[1];d.remove(j)
   # Preserve the user's current placement. Only replace the Bluetooth power
   # selection circuit; all its ground and TX/RX stubs keep their objects.
   vbt=next(z for z in nodes(d,'label') if z[1]=='VBT');vbtat=at(vbt);d.remove(vbt)
   port=next(z for z in nodes(d,'hierarchical_label') if z[1]=='V3V3_EXT')
   node(port,'at')[1:3]=[num(vbtat[0]),num(vbtat[1])]
   p5=next(z for z in nodes(d,'hierarchical_label') if z[1]=='V5_EXT');removed_port_uuid=node(p5,'uuid')[1];d.remove(p5)
   # Live J15 and its four unchanged pin wire endpoints are identified by the
   # actual symbol position, not the earlier generated layout coordinates.
   j15=next(z for z in nodes(d,'symbol') if prop(z,'Reference')[2]=='J15');x,y=at(j15)
   assert float(node(j15,'at')[3])==0 and not node(j15,'mirror')
   lib=next(z for z in nodes(node(d,'lib_symbols'),'symbol') if z[1]==node(j15,'lib_id')[1])
   pins={node(q,'number')[1]:(round(x+float(node(q,'at')[1]),4),round(y-float(node(q,'at')[2]),4)) for sub in nodes(lib,'symbol') for q in nodes(sub,'pin')}
   for sub in nodes(lib,'symbol'):
    for q in nodes(sub,'pin'):
     if node(q,'number')[1]=='1':node(q,'name')[1]='VCC_3V3'
   keep={pins['2'],pins['3'],pins['4']}
   for w in list(nodes(d,'wire')):
    pp=[tuple(map(float,q[1:3])) for q in node(w,'pts')[1:]]
    if all(q[1]<90 for q in pp) and not keep.intersection(pp):
     # Keep the existing two-segment VBT->J15 route. The hierarchical 3V3
     # port now terminates on it, and the J16 branch is removed.
     allowed=[(vbtat,(vbtat[0],pins['1'][1])),((vbtat[0],pins['1'][1]),pins['1'])]
     if not any(set(pp)==set(pair) for pair in allowed):d.remove(w)
   for z in nodes(d,'text'):
    if z[1]=='HC-08 蓝牙与供电选择':z[1]='HC-08 蓝牙：固定三伏三供电'
    if z[1].startswith('J15 为 2.54mm 四位排母'):
     z[1]='J15 四位排母，固定三伏三。\n1=3V3、2=GND、3=TX、4=RX。\n不保留五伏切换或选择跳帽。';node(z,'at')[1:3]=[num(142.24),num(43.18)]
  if p.name=='CoralC5-Motor.kicad_sch':
   sh=next(s for s in nodes(d,'sheet') if prop(s,'Sheetfile')[2]=='09_Bluetooth_Stepper.kicad_sch');pin=next(z for z in nodes(sh,'pin') if z[1]=='V5_EXT');point=at(pin)
   ends={point}
   for w in list(nodes(d,'wire')):
    pp=[tuple(map(float,q[1:3])) for q in node(w,'pts')[1:]]
    if point in pp:d.remove(w);ends.update(pp)
   for z in list(nodes(d,'label')):
    if z[1]=='V5_EXT' and at(z) in ends:d.remove(z)
   sh.remove(pin)
  save(p,d)
 # Synchronize cached defaults even on pages where this generic connector is
 # only an unused cached definition; do not move any live components.
 for p in ROOT.glob('*.kicad_sch'):
  d=parse(p.read_text(encoding='utf-8'));dirty=False
  for z in nodes(node(d,'lib_symbols'),'symbol'):
   if z[1]==j10lib and prop(z,'Footprint')[2]!=header:prop(z,'Footprint')[2]=header;dirty=True
  if dirty:save(p,d)
 # Synchronize only the two affected library definitions.
 p=ROOT/'Motor.kicad_sym';d=parse(p.read_text(encoding='utf-8'))
 z=next(z for z in nodes(d,'symbol') if z[1]==j10lib.split(':')[1]);prop(z,'Footprint')[2]=header
 z=next(z for z in nodes(d,'symbol') if z[1]=='M2_J15')
 for sub in nodes(z,'symbol'):
  for q in nodes(sub,'pin'):
   if node(q,'number')[1]=='1':node(q,'name')[1]='VCC_3V3'
 save(p,d)
 path=ROOT/'docs/connector_revision.json';d=json.loads(path.read_text(encoding='utf-8'));entry=d['changes']['J10'];entry['old_footprint']=entry['footprint'];entry['footprint']=header;entry['type']='2P 2.54mm male jumper header, not XH'
 d['unchanged_exceptions'].pop('J16',None);d['removed_references']=['J16'];d['requested']+='; latest: J10 jumper header, Bluetooth fixed 3V3 and J16 removed'
 path.write_text(json.dumps(d,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
 bom=ROOT/'docs/BOM_M2_SCHEMATIC_DRAFT.csv';rows=[r for r in csv.DictReader(bom.open(encoding='utf-8-sig')) if r['reference']!='J16']
 for row in rows:
  if row['reference']=='J10':row['footprint']=header
  row['status']='M2.4 schematic draft; not production release'
 with bom.open('w',encoding='utf-8-sig',newline='') as f:
  w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
 pro=ROOT/'CoralC5-Motor.kicad_pro';original=json.loads((ROOT/'docs/baseline-2026-11-06-layout'/pro.name).read_text(encoding='utf-8'));live=json.loads(pro.read_text(encoding='utf-8'));oldrev=original['board']['ipc2581']['sch_revision'];liverev=live['board']['ipc2581']['sch_revision'];original['board']['ipc2581']['sch_revision']=liverev
 assert original==live,'Unreviewed project settings differ; retain and audit rather than overwrite'
 data={'date':'2026-11-06','revision':'M2.4','baseline':str(BASE.relative_to(ROOT)),'J10':{'footprint':header,'function':'2P jumper enables MCU control of left/right wheel nSLEEP only; open disables both wheels'},'bluetooth_fixed_supply':'V3V3_EXT','removed_references':['J16'],'removed_symbol_uuid':removed_uuid,'removed_hierarchical_ports':{'09_Bluetooth_Stepper.kicad_sch':['V5_EXT']},'removed_hierarchical_port_uuid':removed_port_uuid,'removed_net':'VBT','J15_pin1_name':'VCC_3V3','preserved_project_metadata':{'path':['board','ipc2581','sch_revision'],'original_layout_baseline_value':oldrev,'preexisting_followup_value':liverev,'note':'Project file not written by this edit'},'project_sha256':sha(pro),'source_sha256':{p.name:sha(p) for p in ROOT.glob('*.kicad_sch')},'pcb_accessed':False}
 (ROOT/'docs/jumpers_revision.json').write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf-8');print('M2.4 saved: J10 2P jumper, J16 removed, HC-08 fixed to peripheral 3V3.')
if __name__=='__main__':main()
