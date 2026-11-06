"""Check an intentional schematic relayout against its complete live baseline."""
from pathlib import Path
import json,hashlib,xml.etree.ElementTree as ET
from sexpr import parse,node,nodes,prop

def net_partitions(path):
 return {frozenset((p.attrib['ref'],p.attrib['pin']) for p in n.findall('node')) for n in ET.parse(path).findall('.//nets/net')}

def run(root):
 doc=root/'docs';base=doc/'baseline-2026-11-06-layout';checks=[]
 def check(name,ok,detail=None):checks.append({'name':name,'pass':bool(ok),'detail':detail})
 before=net_partitions(base/'netlist.xml');after=net_partitions(doc/'netlist.xml')
 check('layout: complete native pin/net partition unchanged',before==after,{'removed':[sorted(x) for x in before-after],'added':[sorted(x) for x in after-before]})
 moved=[];symbol_count=0;physical=0
 for path in sorted(root.glob('*.kicad_sch')):
  old=parse((base/path.name).read_text(encoding='utf-8'));new=parse(path.read_text(encoding='utf-8'))
  def objects(d):return {node(z,'uuid')[1]:z for z in nodes(d,'symbol')}
  oo,nn=objects(old),objects(new);check('layout: '+path.name+' symbol UUID set unchanged',oo.keys()==nn.keys())
  changed=[]
  for key,z in oo.items():
   zz=nn.get(key)
   if zz is None:continue
   symbol_count+=1
   ref=prop(z,'Reference')[2];physical+=not ref.startswith('#')
   if node(z,'at')!=node(zz,'at'):moved.append(ref)
   # Coordinates and label positions may change; electrical identity may not.
   fields=lambda a:{p[1]:p[2] for p in nodes(a,'property')}
   pins=lambda a:sorted((p[1],node(p,'uuid')[1]) for p in nodes(a,'pin'))
   tags=['lib_id','unit','in_bom','on_board','dnp','instances']
   if fields(z)!=fields(zz) or pins(z)!=pins(zz) or any(node(z,t)!=node(zz,t) for t in tags):changed.append(ref)
  check('layout: '+path.name+' component identity/pins/fields unchanged',not changed,changed)
  check('layout: '+path.name+' cached symbols unchanged',node(old,'lib_symbols')==node(new,'lib_symbols'))
  a={(z[1],node(z,'shape')[1],node(z,'uuid')[1]) for z in nodes(old,'hierarchical_label')};b={(z[1],node(z,'shape')[1],node(z,'uuid')[1]) for z in nodes(new,'hierarchical_label')}
  check('layout: '+path.name+' hierarchy interface retained',a==b)
 for name in ['CoralC5-Motor.kicad_pro','Motor.kicad_sym','sym-lib-table','fp-lib-table']:
  check('layout: '+name+' unmodified',(root/name).read_bytes()==(base/name).read_bytes())
 result={'date':'2026-11-06','revision':'M2.1','pass':all(c['pass'] for c in checks),'checks':checks,'baseline':'baseline-2026-11-06-layout','native_nets':len(after),'symbol_instances':symbol_count,'physical_symbol_instances':physical,'moved_instances':moved,'pcb_accessed':False,'sha256':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in root.glob('*.kicad_sch')}}
 (doc/'layout_verification.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
 return checks
