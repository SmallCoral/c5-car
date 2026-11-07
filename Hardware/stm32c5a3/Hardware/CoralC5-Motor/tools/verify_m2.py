"""Native-netlist audit of M2; preserves the real pre-edit contract. No PCB access."""
from pathlib import Path
import sys,csv,json,hashlib,re,copy,subprocess,xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parents[1];DOC=ROOT/'docs';BASE=DOC/'baseline-2026-11-06-m2'
sys.path.insert(0,str(ROOT.parent/'Shared/design_tools'))
from sexpr import parse,node,nodes,prop
from pin_functions import parse_alternate_functions
from revision_contract import Revisions,checks as revision_checks

def netlist(path):
 tree=ET.parse(path);by_pin={};by_name={}
 for nn in tree.findall('.//nets/net'):
  pins=frozenset(z.attrib['ref']+':'+z.attrib['pin'] for z in nn.findall('node'))
  by_name[nn.attrib['name']]=pins
  for pin in pins:
   assert pin not in by_pin,pin
   by_pin[pin]=pins
 return by_pin,by_name

def main():
 checks=[]
 rev=Revisions(ROOT)
 def check(name,ok,detail=None):checks.append(dict(name=name,pass_=bool(ok),detail=detail))
 actual,names=netlist(DOC/'netlist.xml');old,_=netlist(BASE/'motor.net.xml');core,_=netlist(DOC/'core_netlist.xml')
 def group(name,pins):
  expected=frozenset(pins.split());found=actual.get(next(iter(expected)),frozenset())
  check(name,found==expected,dict(missing=sorted(expected-found),unexpected=sorted(found-expected)))
 def joined(name,*pins):check(name,all(p in actual.get(pins[0],()) for p in pins),pins)
 # Compare all original physical circuits, permitting only the connector's sleeve
 # renumbering and additions to existing rails. This detects merges and omissions.
 def map_old(pin):return None if pin=='J1:2' else rev.map_pin('J1:2' if pin=='J1:3' else pin)
 old_pins={p for p in map(map_old,old) if p and not p.startswith('#')}
 seen=set()
 for pin,members in old.items():
  expected=frozenset(q for q in map(map_old,members) if q and not q.startswith('#'))
  if not expected or expected in seen:continue
  seen.add(expected);found=actual.get(next(iter(expected)),frozenset())&old_pins
  check('baseline circuit '+min(expected),found==expected,dict(missing=sorted(expected-found),unexpected=sorted(found-expected)))
 for file in ['04_Left_Motor.kicad_sch','05_Right_Motor.kicad_sch']:
  if (DOC/'layout_edit.json').exists():
   old=parse((BASE/file).read_text(encoding='utf-8'));new=parse((ROOT/file).read_text(encoding='utf-8'))
   if rev.passives:
    for z in nodes(old,'symbol'):
     if prop(z,'Reference')[2] in rev.passives['changes']:prop(z,'Footprint')[2]=rev.passives['changes'][prop(z,'Reference')[2]]['footprint']
   check(file+' circuit objects preserved across presentation edit',all(nodes(old,t)==nodes(new,t) for t in ['symbol','wire','junction','no_connect','label','global_label','hierarchical_label']))
  else:check(file+' byte preserved',(ROOT/file).read_bytes()==(BASE/file).read_bytes())
 schemas={p.name:parse(p.read_text(encoding='utf-8')) for p in ROOT.glob('*.kicad_sch')}
 check('authorized sheet count',len(schemas)==9+len(rev.vision.get('new_sheets',{}))-len(rev.m26.get('removed_sheets',[])))
 components={};allpins=set();nc=[]
 for file,d in schemas.items():
  expected_paper=rev.m26.get('paper',{}).get(file,rev.vision.get('paper',{}).get(file,'A3' if file=='CoralC5-Motor.kicad_sch' and (DOC/'layout_edit.json').exists() else 'A4'))
  check(file+' paper',node(d,'paper')[1]==expected_paper)
  for z in nodes(d,'text'):check(file+' 100mil Chinese note',list(map(float,node(node(node(z,'effects'),'font'),'size')[1:]))==[2.54,2.54])
  nc.extend((file,tuple(map(float,node(z,'at')[1:3]))) for z in nodes(d,'no_connect'))
  for z in nodes(d,'symbol'):
   ref=prop(z,'Reference')[2]
   if ref.startswith('#'):continue
   key=(ref,int(node(z,'unit')[1]));components[key]=(file,z)
   allpins.update(ref+':'+p[1] for p in nodes(z,'pin'))
 # Position/reference/UUID audit against the user's saved design, not a generator.
 for file in ['02_Power.kicad_sch','03_Core_Interface.kicad_sch','04_Left_Motor.kicad_sch','05_Right_Motor.kicad_sch']:
  for z in nodes(parse((BASE/file).read_text(encoding='utf-8')),'symbol'):
   ref=prop(z,'Reference')[2]
   if ref.startswith('#'):continue
   key=(ref,int(node(z,'unit')[1]));now=components.get(key)
   check(ref+' original UUID/unit/pose retained',bool(now) and node(now[1],'uuid')==node(z,'uuid') and node(now[1],'at')==node(z,'at'))
 contract=list(csv.DictReader((ROOT.parent/'Shared/interface.csv').open(encoding='utf-8-sig')))
 bycontact={r['contact']:r for r in contract}
 check('98 contact contract',len(contract)==98 and len(bycontact)==98)
 lib=next(x for x in nodes(node(schemas['03_Core_Interface.kicad_sch'],'lib_symbols'),'symbol') if x[1].endswith('CoralC5_Streamline_Socket'))
 lp={node(p,'number')[1]:node(p,'name')[1] for sub in nodes(lib,'symbol') for p in nodes(sub,'pin')}
 contact_report=[]
 m2=list(csv.DictReader((DOC/'m2_pin_assignment.csv').open(encoding='utf-8-sig')))
 oldroles=json.loads((BASE/'design_manifest.json').read_text(encoding='utf-8-sig'))['motor_pin_assignment']['entries']
 assignments=oldroles+m2
 reserved={'PA9','PA11','PA12','PA13','PA14','PH2','PE3','PD2'}
 expected_roles=rev.m26.get('gpio_total',42 if rev.gray else 44)
 check(str(expected_roles)+' unique GPIO roles',len(assignments)==expected_roles and len({r['gpio'] for r in assignments})==expected_roles)
 check('reserved Core/USB/SWD pins not assigned',not reserved&{r['gpio'] for r in assignments})
 functions=parse_alternate_functions((ROOT.parent/'Shared/sources/DS15137_STM32C5Axxx_Datasheet_Rev3.pdf.txt').read_text(encoding='utf-8-sig'))
 for r in contract:
  contact=r['contact'];cn=core.get('J13:'+contact,frozenset());mn=actual.get('J14:'+contact,frozenset())
  check(contact+' socket pin exists',contact in lp and 'J14:'+contact in allpins)
  if r['mcu_pin']:check(contact+' Core native MCU pin','U1:'+r['mcu_pin'] in cn)
  if r['gpio']:check(contact+' Motor symbol GPIO name',lp[contact].endswith('/_'+r['gpio']))
  if contact in {'A13','A14','B45','B46'}:
   check(contact+' unbonded NC on both boards',len(cn)==len(mn)==1 and 'NC' in lp[contact])
  if r['net'] in ['GND','+3V3','+5V_SYS']:
   same=[x['contact'] for x in contract if x['net']==r['net']]
   check(contact+' matching power contacts',all('J13:'+c in cn and 'J14:'+c in mn for c in same))
  assigned=next((z for z in assignments if z['contact']==contact),None)
  contact_report.append(dict(contact=contact,gpio=r['gpio'],mcu_pin=r['mcu_pin'],core_native_pins=' '.join(sorted(cn)),motor_native_pins=' '.join(sorted(mn)),role=assigned['signal'] if assigned else ('connected' if len(mn)>1 else 'unused NC')))
 for r in assignments:
  check(r['signal']+' contact contract',bycontact[r['contact']]['gpio']==r['gpio'])
  value=r.get('af');value=None if value in [None,'','None'] else int(value)
  if value is not None:check(r['signal']+' datasheet AF',functions.get((r['gpio'],r['function']))==value)
  joined(r['signal']+' reaches endpoint','J14:'+r['contact'],r['endpoint'])
 # Independently specified connector pin contracts.
 for ref,power,tx,rx in [('J15','V3V3_EXT' if rev.jumpers else 'VBT','BT_TX','BT_RX')]+([] if rev.vision else [('J18','V5_EXT','MAIX_TX','MAIX_RX'),('J19','V5_EXT','VIDEO_TX','VIDEO_RX')])+([] if rev.zdt else [('J23','V3V3','STEP_UART_TX','STEP_UART_RX')]):
  for pin,net in enumerate([power,'GND',tx,rx],1):check(ref+':'+str(pin)+' expected net',ref+':'+str(pin) in next((p for name,p in names.items() if name.split('/')[-1]==net),frozenset()),net)
 extra={'TFT_CS':['R55:2'],'TFT_RES':['R56:2'],'STEP_PUL':['R57:1'],'STEP_DIR':['R58:1'],'STEP_nEN':['R54:2']}
 if rev.vision:extra.update(LPI_TX=['R79:2'],VIDEO_TX=['R80:2'])
 if rev.m26:extra.update(STEP_UART_TX=['R88:2'])
 for r in m2:
  expected=['J14:'+r['contact'],r['endpoint']]+extra.get(r['signal'],[])
  if r['signal'].startswith('KEY_'):
   idx=['KEY_MODE','KEY_NEXT','KEY_OK','KEY_START'].index(r['signal']);expected += [f'R{50+idx}:2',f'C{57+idx}:1']
  group(r['signal']+' exact membership',' '.join(expected))
 group('XT60 positive before fuse','J1:1 F1:1');joined('XT60 negative is ground','J1:2','J14:A1')
 j1=components[('J1',1)][1]
 check('XT60 procurement metadata',prop(j1,'MPN')[2]=='XT60PW-M' and prop(j1,'LCSC') is None)
 group('auxiliary bootstrap','U6:6 C51:1');group('auxiliary switch','U6:5 C51:2 L2:1')
 group('independent peripheral 3V3','U7:5 C55:1 C56:1 J17:2 R55:1 R56:1 '+('J15:1' if rev.jumpers else 'J16:1'))
 if rev.m26:group('independent peripheral 5V','L2:2 C52:1 C53:1 U7:1 U7:3 C54:1 J21:2 R59:2 C63:2 U10:4 C72:1 C78:1 J27:1 R70:1')
 elif rev.vision:group('independent peripheral 5V','L2:2 C52:1 C53:1 U7:1 U7:3 C54:1 J21:2 R59:2 C63:2 U10:5 C72:1 J27:1 R70:1')
 else:group('independent peripheral 5V','U6:1 L2:2 C52:1 C53:1 U7:1 U7:3 C54:1 C61:1 C62:1 J18:1 J19:1 '+('J21:2' if rev.gray else 'J20:1 J21:1')+('' if rev.jumpers else ' J16:3'))
 if rev.jumpers:joined('BT fixed 3V3 supply','J15:1','U7:5')
 else:group('BT selected supply','J16:2 J15:1')
 if rev.power:
  group('ZDT protected step supply','F2:2 Q6:2' if rev.m26 else 'F2:2 J23:1 J24:1');joined('step fuse from protected VM','F2:1','Q2:2','U6:3')
 else:joined('external driver protected VM','J24:1','Q2:5','U6:3')
 for pin in ['J17:1',*(['J21:1'] if rev.gray else ['J20:2','J21:2']),'J22:2','J22:6','J23:2','J24:2','R57:2','R58:2']:
  if pin.split(':')[0] not in rev.m26.get('removed_references',[]):joined(pin+' ground',pin,'J1:2')
 check('peripheral 3V3 not joined to Core',actual['U7:5']!=actual['J14:A47'])
 check('peripheral 5V not joined to Core USB rail',actual['U7:1']!=actual['J14:A48'])
 for contact in set(bycontact)-{z['contact'] for z in assignments}-{'A1','A49','B1','B49','A47','B47','A48','B48','A46','B43'}:
  check(contact+' remains single unused finger',len(actual.get('J14:'+contact,()))==1)
 resources=[];missing_models=[]
 for ref in sorted({key[0] for key in components}):
  syms=[z for (r,u),(file,z) in components.items() if r==ref];fp=prop(syms[0],'Footprint')[2]
  check(ref+' local footprint',fp.startswith('Motor:'))
  path=ROOT/'Motor.pretty'/(fp.split(':',1)[1]+'.kicad_mod');check(ref+' footprint exists',path.is_file())
  if not path.is_file():continue
  f=parse(path.read_text(encoding='utf-8'));pads={p[1] for p in nodes(f,'pad') if p[1]};pins={p[1] for z in syms for p in nodes(z,'pin')}
  check(ref+' symbol/pad set',pins==pads,dict(pins=sorted(pins),pads=sorted(pads)))
  models=nodes(f,'model');mechanical=ref in ['H1','H2']
  if not models and not mechanical:missing_models.append(ref)
  for model in models:check(ref+' portable model',model[1].startswith('${KIPRJMOD}/Models/') and (ROOT/model[1].replace('${KIPRJMOD}/','')).is_file(),model[1])
  resources.append(dict(reference=ref,footprint=fp,models=[m[1] for m in models],mechanical_hole=mechanical))
 check('accurate XT60 model gap explicitly declared',missing_models==['J1'],missing_models)
 for ref in ['H1','H2']:
  file,z=components[(ref,1)];f=parse((ROOT/'Motor.pretty'/(prop(z,'Footprint')[2].split(':')[1]+'.kicad_mod')).read_text())
  check(ref+' 2mm non-plated drill',any(p[2]=='np_thru_hole' and float(node(p,'drill')[1])==2 for p in nodes(f,'pad')))
  check(ref+' excluded from electrical BOM',node(z,'in_bom')[1]=='no')
 mdata=json.loads((DOC/'m2_design.json').read_text(encoding='utf-8'))
 check('TFT two-hole pitch derived from 35mm PCB and 2.5mm edge offsets',mdata['mounting_holes']['center_distance_mm']==35-2*2.5 and mdata['mounting_holes']['edge_offset_mm']==2.5)
 erc=json.loads((DOC/'erc.json').read_text(encoding='utf-8'));violations=[v for sh in erc['sheets'] for v in sh['violations']]
 check('native ERC zero violations',not violations,violations)
 project=json.loads((ROOT/'CoralC5-Motor.kicad_pro').read_text(encoding='utf-8'));check('no ERC exclusions',not project.get('erc',{}).get('erc_exclusions'))
 if (DOC/'layout_edit.json').exists():
  from verify_layout_edit import run
  for entry in run(ROOT):check(entry['name'],entry['pass'],entry['detail'])
 for entry in revision_checks(ROOT,actual,schemas):check(entry['name'],entry['pass'],entry['detail'])
 for checkrow in checks:checkrow['pass']=checkrow.pop('pass_')
 with (DOC/'core_motor_98_contact_audit.csv').open('w',encoding='utf-8-sig',newline='') as f:
  w=csv.DictWriter(f,fieldnames=list(contact_report[0]));w.writeheader();w.writerows(contact_report)
 result=dict(date='2026-11-06',kicad_version=erc['kicad_version'],source_commit='4cf36a2ce5a0d5bb6b469a9a82320fcfbcc38c1f',checks=len(checks),failed=[c for c in checks if not c['pass']],native_erc_errors=sum(v['severity']=='error' for v in violations),native_erc_warnings=sum(v['severity']=='warning' for v in violations),physical_references=len(resources),model_bound_references=sum(bool(r['models']) for r in resources),mechanical_holes=2,missing_exact_models=missing_models,resources=resources,artifacts_sha256={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in ROOT.glob('*.kicad_sch')},results=checks,limitations=['J1 exact horizontal XT60PW-M model is not available; no vertical substitute attached','PCB not opened or modified; no PCB DRC or schematic/PCB parity performed','TFT 30mm pitch is calculated from drawing offsets; confirm the actual module before PCB placement','HC-08 carrier version and external module mating pin orders must be checked against the actual hardware','4A upstream protection and startup/thermal/driver-level compatibility require physical validation'])
 result['pass']=not result['failed'];(DOC/'verification.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
 if rev.power:
  result['revision']='M2.4' if rev.jumpers else 'M2.3'
  result['limitations'][-1]='Current estimates are design budgets, not measured consumption; 12A/4A time-delay fuses, MOS startup/SOA/thermal behavior, battery fault-current interruption and XH 3A harness loading require validation'
  result['limitations'].append('SW1 and F1/F2 use explicitly generic dimensioned 3D envelopes, not exact manufacturer assembly models')
  (DOC/'verification.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
 if rev.bt_pinout:
  result['limitations'][3]='HC-08 pin order is confirmed against the user photo; final connector mounting orientation, fit and other external module mating orders still require physical validation'
  (DOC/'verification.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
 if rev.buttons:
  result['limitations'].append('SW2-SW5 use a generic 6x6x4.3mm SMD reference footprint and model; actual unbranded lead span, contact pairs and ratings are not measured or manufacturer-confirmed')
 if rev.vision:
  result['revision']='M2.5'
  result['unbound_models']=missing_models
  result['generic_model_references']=sorted(set(rev.vision['generic_models']+['SW1','F2','SW2','SW3','SW4','SW5']))
  result['missing_exact_models']=sorted(set(missing_models+result['generic_model_references']))
  result['limitations'][4]='Current budget is 10.6765A at 10V, not a measurement; F1 is 15A/85V very-fast and F2 remains 4A/125V time-delay. Inrush, battery fault interruption, MOS SOA/temperature and harness loading require physical validation'
  result['limitations'].extend(['Lichee separate 5V/3A output and Maix 2A charging-inclusive budget require real startup, load and cable-drop tests','Maix six-pin 5V and USB VBUS are common inside the module: remove J27 before ordinary powered USB debug; the battery may keep the module alive','J25 is a two-wire supply terminal, not a USB-C source controller: use a correctly wired 3A power lead with appropriate CC handling','Lichee UART1 header RX has onboard 1/2 divider and TX is 1.8V; LV1T buffer inputs have no positive clamp, but the part is not advertised as output Ioff isolated. Keep UART pins in the specified direction'])
 if rev.m26:
  result['date']='2026-11-07';result['revision']='M2.6'
  result['generic_model_references']=sorted((set(result['generic_model_references'])-set(rev.m26['removed_references']))|{'U10'})
  result['missing_exact_models']=sorted(set(missing_models+result['generic_model_references']))
  result['limitations']=[x for x in result['limitations'] if 'Lichee' not in x and 'J25' not in x and '10.6765A' not in x]
  result['limitations'].extend(['No PCB file was opened or modified in this schematic-only revision; user will update and route PCB before its separate review','Current budget is 8.9118A at 10V and is not a measurement; 15A F1 and 4A F2 remain. Startup, MOS SOA, temperature and motor regeneration during shutoff require validation','U10 uses SOIC-8 with exposed ground pad; its attached stock SOIC body is a generic package substitute, not manufacturer EP geometry','SGM2531C 215k current-limit formula gives 2.2315A nominal. The 5 percent headline is only specified at 1A/25C; this is not a guaranteed 2A minimum threshold','Maix 5.2079V nominal OVP threshold includes reference/resistor/leakage spread; it is a cutoff, not a precision output clamp. Verify loaded voltage and charging startup','STEP_PWR_EN is PD12/B30, active high with hardware default off. Stop motion before cutting power; wait for driver startup before UART commands. Turn-off is not an instantaneous discharge or a safety-rated emergency stop'])
 try:
  current_commit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,stderr=subprocess.DEVNULL).decode().strip()
 except (OSError,subprocess.CalledProcessError):current_commit='unavailable'
 result['baseline_source_commit']=result['source_commit'];result['source_commit']=current_commit
 (DOC/'verification.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
 print(json.dumps({k:v for k,v in result.items() if k not in ['results','resources','artifacts_sha256']},ensure_ascii=False,indent=2))
 return 0 if result['pass'] else 1

if __name__=='__main__':raise SystemExit(main())
