"""Explicit, bounded changes authorized after the M2.1 presentation baseline."""
import copy,csv,json
from sexpr import node,nodes,prop,parse

class Revisions:
 def __init__(self,root):
  self.root=root;doc=root/'docs'
  def read(name):
   p=doc/name;return json.loads(p.read_text(encoding='utf-8')) if p.exists() else {}
  self.zdt=read('zdt_ttl_design.json');self.connectors=read('connector_revision.json');self.power=read('power_revision.json');self.passives=read('passive_0603_revision.json');self.jumpers=read('jumpers_revision.json')
  self.active=bool(self.zdt or self.connectors or self.power or self.passives or self.jumpers)
  self.pinmap={}
  self.pinmap.update(self.zdt.get('intentional_pin_map',{}));self.pinmap.update(self.power.get('pin_map_from_m2',{}))
  if self.jumpers:self.pinmap.update({f'J16:{k}':None for k in range(1,4)})
 def map_pin(self,p):return self.pinmap.get(p,p)
 def expected_fields(self,z):
  values={p[1]:p[2] for p in nodes(z,'property')};ref=values['Reference']
  if ref=='J23' and self.zdt:values.update(Value='ZDT_XS_TTL_8P',Footprint=self.zdt['J23']['footprint'],Datasheet=self.zdt['manual'])
  if ref in self.connectors.get('changes',{}):values['Footprint']=self.connectors['changes'][ref]['footprint']
  for k,v in self.power.get('changes',{}).get(ref,{}).items():
   if k!='lib_id':values[k]=v
  if ref in self.passives.get('changes',{}):values['Footprint']=self.passives['changes'][ref]['footprint']
  return values
 def expected_libid(self,z):return self.power.get('changes',{}).get(prop(z,'Reference')[2],{}).get('lib_id',node(z,'lib_id')[1])
 def pin_identity(self,old,new):
  ref=prop(old,'Reference')[2];oo={p[1]:node(p,'uuid')[1] for p in nodes(old,'pin')};nn={p[1]:node(p,'uuid')[1] for p in nodes(new,'pin')}
  if self.zdt and ref=='J23':return set(nn)==set(map(str,range(1,9))) and all(nn[b]==oo[a] for a,b in [('2','2'),('3','7'),('4','8')])
  if self.power and ref in ['Q1','Q2']:return nn=={b:oo[a] for a,b in [('1','3'),('4','1'),('5','2')]}
  if self.power and ref=='SW1':return nn=={p:u for p,u in oo.items() if p in ['1','2','3']}
  return oo==nn
 def expected_partitions(self,parts):
  result=set()
  for part in parts:
   mapped={self.map_pin(ref+':'+pin) for ref,pin in part};mapped.discard(None)
   if mapped:result.add(frozenset(tuple(p.split(':',1)) for p in mapped))
  if self.zdt:
   for k in range(3,7):result.add(frozenset([('J23',str(k))]))
  if self.power:
   main=next(p for p in result if ('U6','3') in p);result.remove(main)
   result.add((main-{('J24','1')})|{('F2','1')})
   result.add(frozenset([('F2','2'),('J23','1'),('J24','1')]))
  elif self.zdt:
   main=next(p for p in result if ('J24','1') in p);result.remove(main);result.add(main|{('J23','1')})
  if self.jumpers:
   bt=next(p for p in result if ('J15','1') in p);rail=next(p for p in result if ('U7','5') in p)
   result.remove(bt);result.remove(rail);result.add(bt|rail)
  return result
 def library_compatible(self,old,new,cache=False,extra_fuse=None):
  def symbols(d):return {z[1]:copy.deepcopy(z) for z in nodes(d,'symbol')}
  oo,nn=symbols(old),symbols(new);prefix='Motor:' if cache else ''
  if self.jumpers and prefix+'M2_J15' in oo:
   for sub in nodes(oo[prefix+'M2_J15'],'symbol'):
    for p in nodes(sub,'pin'):
     if node(p,'number')[1]=='1':node(p,'name')[1]='VCC_3V3'
  if self.zdt:
   oo.pop(prefix+'M2_J23',None);nn.pop(prefix+'M2_J23',None)
  if self.power:
   for name in ['AO4407A','AO4407A_Mirrored','Switch__SW_SPDT']:oo.pop(prefix+name,None)
   for name in ['DMP3007LK3','DMP3007LK3_Mirrored','SS12D10G5_3P']:nn.pop(prefix+name,None)
  for change in self.connectors.get('changes',{}).values():
   key=change['lib_id'] if cache else change['lib_id'].split(':')[1]
   if key in oo and prop(oo[key],'Footprint'):prop(oo[key],'Footprint')[2]=change['footprint']
  if extra_fuse is not None and self.power:oo[prefix+'Device__Fuse']=copy.deepcopy(extra_fuse)
  return oo==nn

def checks(root,actual,schemas):
 r=Revisions(root);out=[]
 def ck(name,ok,detail=None):out.append({'name':'revision: '+name,'pass':bool(ok),'detail':detail})
 def group(pin):return actual.get(pin,frozenset())
 if r.zdt:
  ck('ZDT TTL variant explicitly confirmed',r.zdt['user_confirmed'].endswith('TTL variant'))
  ck('J23 8P only four active wires',r.zdt['J23']['pins']=={'1':'VM','2':'GND','3':'NC','4':'NC','5':'NC','6':'NC','7':'STEP_UART_TX','8':'STEP_UART_RX'})
  for k in range(3,7):ck(f'J23:{k} electrically isolated',group(f'J23:{k}')=={f'J23:{k}'})
  ck('J23:2 common ground','J1:2' in group('J23:2'))
  ck('UART TX to driver RX','J14:A30' in group('J23:7'))
  ck('UART RX from driver TX','J14:A29' in group('J23:8'))
 for ref,c in r.connectors.get('changes',{}).items():
  z=next(z for d in schemas.values() for z in nodes(d,'symbol') if prop(z,'Reference')[2]==ref)
  ck(ref+' selected connector footprint',prop(z,'Footprint')[2]==c['footprint'],c['type'])
 if r.jumpers:
  byref={prop(z,'Reference')[2]:z for d in schemas.values() for z in nodes(d,'symbol')}
  ck('J16 Bluetooth selector removed','J16' not in byref)
  ck('Bluetooth fixed to peripheral 3V3','U7:5' in group('J15:1') and 'J16:2' not in group('J15:1'))
  ck('J10 remains jumper header',prop(byref['J10'],'Footprint')[2]=='Motor:Connector_PinHeader_2.54mm__PinHeader_1x02_P2.54mm_Vertical')
 if r.passives:
  current={prop(z,'Reference')[2]:z for d in schemas.values() for z in nodes(d,'symbol')}
  ck('all 67 original 0603 parts included',len(r.passives['changes'])==67)
  for ref,c in r.passives['changes'].items():ck(ref+' unified 0603 footprint',prop(current[ref],'Footprint')[2]==c['footprint'])
  for fp in r.passives['footprint_sha256']:
   d=parse((root/'Motor.pretty'/(fp.split(':')[1]+'.kicad_mod')).read_text(encoding='utf-8'));pads={p[1]:p for p in nodes(d,'pad') if p[1] in ['1','2']}
   x={k:float(node(p,'at')[1]) for k,p in pads.items()};size={k:list(map(float,node(p,'size')[1:3])) for k,p in pads.items()}
   ck(fp+' exact largest checked stock gap',abs(x['2']-x['1']-(size['1'][0]+size['2'][0])/2-.85)<1e-6)
   ck(fp+' uniform pad size',size=={'1':[.975,.95],'2':[.975,.95]})
   ck(fp+' pin 1 direction retained',x['1']<0<x['2'])
 if r.power:
  byref={prop(z,'Reference')[2]:z for d in schemas.values() for z in nodes(d,'symbol')}
  for ref,expected in [('Q1','DMP3007LK3-13'),('Q2','DMP3007LK3-13'),('SW1','SS12D10G5'),('F1','0454012.MR'),('F2','0454004.MR')]:ck(ref+' exact MPN',prop(byref[ref],'MPN')[2]==expected)
  for ref,value in [('R1','100k'),('R2','4.7k')]:ck(ref+' gate network value',prop(byref[ref],'Value')[2]==value)
  ck('MOS common-source pair',all(p in group('Q1:3') for p in ['Q2:3','SW1:1','R1:2']))
  ck('MOS common gate controlled by SW1',all(p in group('Q1:1') for p in ['Q2:1','SW1:2','R1:1']))
  ck('switch ON throw via R2','R2:1' in group('SW1:3'))
  ck('switch pull-down returns to ground','J1:2' in group('R2:2'))
  ck('F1 between XT60 and input MOS',group('J1:1')=={'J1:1','F1:1'} and group('F1:2')=={'F1:2','Q1:2'})
  ck('protected main VM reaches branch fuse',all(p in group('Q2:2') for p in ['F2:1','U6:3']))
  ck('step branch isolated by F2',group('F2:2')=={'F2:2','J23:1','J24:1'} and group('F2:1')!=group('F2:2'))
  ck('10V full-system minimum',r.power['budget']['battery_voltage_range_v'][0]==10)
  b=r.power['budget'];ck('budget calculation uses power conversion not sum of unlike currents',all(abs(b['input_current_a'][str(v)]-(4+3+5*(.75+1.5)/(v*.85)))<.0001 for v in [10,12,12.6]))
  ck('12A fuse after 25 percent continuous derating covers declared budget',12*.75>b['input_current_a']['10'])
  for ref in ['Q1','Q2','SW1','J23']:
   z=byref[ref];d=next(d for d in schemas.values() if z in nodes(d,'symbol'));lib=next(p for p in nodes(node(d,'lib_symbols'),'symbol') if p[1]==node(z,'lib_id')[1]);pn={node(p,'number')[1]:node(p,'name')[1] for sub in nodes(lib,'symbol') for p in nodes(sub,'pin')}
   expected={'1':'G','2':'D','3':'S'} if ref.startswith('Q') else {'1':'A','2':'B','3':'C'} if ref=='SW1' else {'1':'VM_V+','2':'GND','3':'COM_NC','4':'EN_NC','5':'STEP_NC','6':'DIR_NC','7':'R/A/H_RX','8':'T/B/L_TX'}
   ck(ref+' manufacturer pin identity',pn==expected,pn)
 return out
