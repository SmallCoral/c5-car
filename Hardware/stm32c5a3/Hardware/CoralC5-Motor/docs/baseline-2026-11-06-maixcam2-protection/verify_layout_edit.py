"""Check an intentional schematic relayout against its complete live baseline."""
from pathlib import Path
import csv,json,hashlib,xml.etree.ElementTree as ET
from sexpr import parse,node,nodes,prop
from revision_contract import Revisions

def net_partitions(path):
 return {frozenset((p.attrib['ref'],p.attrib['pin']) for p in n.findall('node')) for n in ET.parse(path).findall('.//nets/net')}

def run(root):
 doc=root/'docs';base=doc/'baseline-2026-11-06-layout';checks=[]
 rev=Revisions(root)
 def check(name,ok,detail=None):checks.append({'name':name,'pass':bool(ok),'detail':detail})
 before=net_partitions(base/'netlist.xml');after=net_partitions(doc/'netlist.xml')
 if rev.active:before=rev.expected_partitions(before)
 check('layout: complete native pin/net partition matches baseline plus explicit revision contract',before==after,{'removed':[sorted(x) for x in before-after],'added':[sorted(x) for x in after-before]})
 assignments=list(csv.DictReader((doc/'m2_pin_assignment.csv').open(encoding='utf-8-sig')))
 signals={r['signal'] for r in assignments};schemas={};oldschemas={}
 moved=[];symbol_count=0;physical=0
 for path in sorted(root.glob('*.kicad_sch')):
  old=parse((base/path.name).read_text(encoding='utf-8'));new=parse(path.read_text(encoding='utf-8'))
  schemas[path.name]=new;oldschemas[path.name]=old
  def objects(d):return {node(z,'uuid')[1]:z for z in nodes(d,'symbol')}
  oo,nn=objects(old),objects(new)
  removed={prop(oo[k],'Reference')[2] for k in oo.keys()-nn.keys()};added={prop(nn[k],'Reference')[2] for k in nn.keys()-oo.keys()}
  er={'#PWR003'} if rev.power and path.name=='02_Power.kicad_sch' else {'J16'} if rev.jumpers and path.name=='09_Bluetooth_Stepper.kicad_sch' else set();ea={'F2'} if rev.power and path.name=='09_Bluetooth_Stepper.kicad_sch' else set()
  check('layout: '+path.name+' symbol UUIDs retained except explicit addition/removal',removed==er and added==ea,{'removed':sorted(removed),'added':sorted(added)})
  symbol_count+=len(nn);physical+=sum(not prop(z,'Reference')[2].startswith('#') for z in nn.values())
  changed=[]
  for key,z in oo.items():
   zz=nn.get(key)
   if zz is None:continue
   ref=prop(z,'Reference')[2]
   if node(z,'at')!=node(zz,'at'):moved.append(ref)
   # Coordinates and label positions may change; electrical identity may not.
   fields=lambda a:{p[1]:p[2] for p in nodes(a,'property')}
   pins=lambda a:sorted((p[1],node(p,'uuid')[1]) for p in nodes(a,'pin'))
   tags=['unit','in_bom','on_board','dnp','instances']
   if rev.expected_fields(z)!=fields(zz) or not rev.pin_identity(z,zz) or node(zz,'lib_id')[1]!=rev.expected_libid(z) or any(node(z,t)!=node(zz,t) for t in tags):changed.append(ref)
  check('layout: '+path.name+' component identities match explicit change contract',not changed,changed)
  extra=None
  if rev.power and path.name=='09_Bluetooth_Stepper.kicad_sch':extra=next(z for z in nodes(node(oldschemas['02_Power.kicad_sch'],'lib_symbols'),'symbol') if z[1]=='Motor:Device__Fuse')
  check('layout: '+path.name+' cached symbols match explicit change contract',rev.library_compatible(node(old,'lib_symbols'),node(new,'lib_symbols'),True,extra))
  a={(z[1],node(z,'shape')[1],node(z,'uuid')[1]) for z in nodes(old,'hierarchical_label')};b={(z[1],node(z,'shape')[1],node(z,'uuid')[1]) for z in nodes(new,'hierarchical_label')}
  if rev.gray and path.name in ['03_Core_Interface.kicad_sch','08_Grayscale_Cameras.kicad_sch']:a={x for x in a if x[0] not in rev.gray['removed_signals']}
  if rev.jumpers and path.name=='09_Bluetooth_Stepper.kicad_sch':a={x for x in a if x[0]!='V5_EXT'}
  converted={}
  for z in nodes(old,'global_label'):
   if z[1] in signals:converted.setdefault(z[1],set()).add((z[1],node(z,'shape')[1],node(z,'uuid')[1]))
  extra=b-a
  # Redundant bias-resistor global aliases were replaced by actual wires in
  # this relayout. Retain one existing label UUID for each signal, not two
  # identical hierarchical ports on the same sheet.
  check('layout: '+path.name+' original ports retained and unique GPIO ports retain existing label UUIDs',a<=b and {z[0] for z in extra}==converted.keys() and len(extra)==len(converted) and all(z in converted.get(z[0],set()) for z in extra))
  check('layout: '+path.name+' new GPIO signals use hierarchy, not global bypass',not signals&{z[1] for z in nodes(new,'global_label')})
 # Verify the overview's visible wires themselves, independently of native
 # connectivity, so globals cannot conceal a disconnected diagram again.
 overview=schemas['CoralC5-Motor.kicad_sch'];oldview=oldschemas['CoralC5-Motor.kicad_sch']
 sheets={prop(z,'Sheetfile')[2]:z for z in nodes(overview,'sheet')}
 old_sheets={prop(z,'Sheetfile')[2]:z for z in nodes(oldview,'sheet')}
 check('overview: hierarchy sheet UUIDs retained',all(node(sheets[f],'uuid')==node(s,'uuid') for f,s in old_sheets.items()) and sheets.keys()==old_sheets.keys())
 at=lambda z:tuple(map(float,node(z,'at')[1:3]))
 pins={(f,p[1]):at(p) for f,s in sheets.items() for p in nodes(s,'pin')}
 wirepts=[tuple(tuple(map(float,p[1:3])) for p in node(w,'pts')[1:]) for w in nodes(overview,'wire')]
 cuts={p for seg in wirepts for p in seg}|set(pins.values())|{at(z) for z in nodes(overview,'junction')};graph={p:set() for p in cuts}
 for a,b in wirepts:
  pp=sorted(p for p in cuts if (a[0]==b[0]==p[0] and min(a[1],b[1])<=p[1]<=max(a[1],b[1])) or (a[1]==b[1]==p[1] and min(a[0],b[0])<=p[0]<=max(a[0],b[0])))
  for p,q in zip(pp,pp[1:]):graph[p].add(q);graph[q].add(p)
 def reachable(p,q):
  todo=[p];seen=set()
  while todo:
   now=todo.pop()
   if now==q:return True
   if now in seen:continue
   seen.add(now);todo.extend(graph[now]-seen)
  return False
 for f,s in sheets.items():
  op={(p[1],p[2],node(p,'uuid')[1]) for p in nodes(old_sheets[f],'pin')};np={(p[1],p[2],node(p,'uuid')[1]) for p in nodes(s,'pin')}
  if rev.jumpers and f=='09_Bluetooth_Stepper.kicad_sch':op={p for p in op if p[0]!='V5_EXT'}
  check('overview: '+f+' existing pin UUIDs retained',op<=np)
  check('overview: '+f+' pins exactly match child hierarchical labels',{p[1] for p in nodes(s,'pin')}=={p[1] for p in nodes(schemas[f],'hierarchical_label')})
  for p in nodes(s,'pin'):check('overview: '+f+':'+p[1]+' has an actual wire',bool(graph[at(p)]))
 for row in assignments:
  ref=row['endpoint'].split(':')[0]
  f='07_TFT_Buttons.kicad_sch' if ref in {'J17','SW2','SW3','SW4','SW5'} else '08_Grayscale_Cameras.kicad_sch' if ref in {'J18','J19','J20','J21'} else '09_Bluetooth_Stepper.kicad_sch'
  sig=row['signal'];check('overview: '+sig+' continuously wired from Core to module',reachable(pins[('03_Core_Interface.kicad_sch',sig)],pins[(f,sig)]))
 for f,prefix in [('04_Left_Motor.kicad_sch','L'),('05_Right_Motor.kicad_sch','R')]:
  for sig in [prefix+'_PWM',prefix+'_DIR','MOTOR_RUN',prefix+'_nFAULT',prefix+'_ISENSE',prefix+'_ENC_A',prefix+'_ENC_B']:
   check('overview: '+f+':'+sig+' continuously wired from Core',reachable(pins[('03_Core_Interface.kicad_sch',sig)],pins[(f,sig)]))
 for name in ['CoralC5-Motor.kicad_pro','sym-lib-table','fp-lib-table']:
  if name=='CoralC5-Motor.kicad_pro' and rev.jumpers:
   original=json.loads((base/name).read_text(encoding='utf-8'));current=json.loads((root/name).read_text(encoding='utf-8'))
   original['board']['ipc2581']['sch_revision']=rev.jumpers['preserved_project_metadata']['preexisting_followup_value']
   check('layout: project settings retained including preexisting export revision metadata',original==current and hashlib.sha256((root/name).read_bytes()).hexdigest()==rev.jumpers['project_sha256'])
  else:check('layout: '+name+' unmodified',(root/name).read_bytes()==(base/name).read_bytes())
 check('layout: local symbol library matches explicit change contract',rev.library_compatible(parse((base/'Motor.kicad_sym').read_text(encoding='utf-8')),parse((root/'Motor.kicad_sym').read_text(encoding='utf-8'))))
 result={'date':'2026-11-06','revision':'M2.4' if rev.jumpers else 'M2.3' if rev.power else 'M2.2' if rev.active else 'M2.1','pass':all(c['pass'] for c in checks),'checks':checks,'baseline':'baseline-2026-11-06-layout','native_nets':len(after),'symbol_instances':symbol_count,'physical_symbol_instances':physical,'moved_instances':moved,'pcb_accessed':False,'sha256':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in root.glob('*.kicad_sch')}}
 (doc/'layout_verification.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
 return checks
