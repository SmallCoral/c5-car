"""Guarded schematic-only power revision from the saved live design."""
from pathlib import Path
import copy,csv,hashlib,json,sys,uuid,shutil
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'docs/baseline-2026-11-06-power-revision'
sys.path.insert(0,str(ROOT.parent/'Shared/design_tools'))
from sexpr import parse,dump,node,nodes,prop,Atom
from schematic_primitives import n,num,xy,effects

def uid(s):return str(uuid.uuid5(uuid.NAMESPACE_URL,'CoralC5-Power-20261106/'+s))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def at(z):return tuple(map(float,node(z,'at')[1:3]))
def put(z,x,y):node(z,'at')[1:3]=[num(x),num(y)]
def save(p,d):p.write_text(dump(d)+'\n',encoding='utf-8')
def wire(d,a,b):d.append(n('wire',n('pts',xy('xy',*a),xy('xy',*b)),n('stroke',n('width',num(0)),n('type',Atom('default'))),n('uuid',uid(str((a,b))))))
def field(z,key,value):
 p=prop(z,key)
 if p:p[2]=value
 else:
  p=copy.deepcopy(prop(z,'Datasheet'));p[1:3]=[key,value];z.append(p)

def main():
 if '--apply-saved-baseline' not in sys.argv:raise SystemExit('Historical edit only; explicit --apply-saved-baseline required.')
 if (ROOT/'docs/power_revision.json').exists():raise SystemExit('Power edit already applied; use KiCad for later changes.')
 for p in [*BASE.glob('*.kicad_sch'),BASE/'Motor.kicad_sym']:
  if sha(ROOT/p.name)!=sha(p):raise SystemExit('Refusing later manual edits in '+p.name)
 stock=Path('C:/Program Files/KiCad/10.0/share/kicad')
 def resource(cat,name,localname=None):
  localname=localname or cat+'__'+name;dst=ROOT/'Motor.pretty'/(localname+'.kicad_mod')
  d=parse((stock/'footprints'/(cat+'.pretty')/(name+'.kicad_mod')).read_text(encoding='utf-8'));d[1]=localname
  for m in nodes(d,'model'):
   rel=m[1].replace('${KICAD10_3DMODEL_DIR}/','');src=stock/'3dmodels'/rel;modelname=cat+'__'+Path(rel).name
   if not src.is_file() and cat=='Fuse':
    # No 454/451 stock model is installed: bind an explicitly generic,
    # dimensioned 454 envelope instead of a wrong small-chip fuse model.
    generic=['#VRML V2.0 utf8','# Generic 454 envelope 6.10x2.69x2.69mm; not an exact manufacturer assembly.']
    for x,dx,color in [(0,6.1,'.9 .9 .85'),(-2.325,1.45,'.7 .7 .7'),(2.325,1.45,'.7 .7 .7')]:
     generic.append('Transform { translation '+str(x/2.54)+' 0 '+str(1.345/2.54)+' children [ Shape { appearance Appearance { material Material { diffuseColor '+color+' } } geometry Box { size '+str(dx/2.54)+' '+str(2.69/2.54)+' '+str(2.69/2.54)+' } } ] }')
    (ROOT/'Models/Fuse_454_generic.wrl').write_text('\n'.join(generic)+'\n',encoding='utf-8');m[1]='${KIPRJMOD}/Models/Fuse_454_generic.wrl';continue
   if not src.is_file():raise SystemExit('Missing matching library model: '+str(src))
   dest=ROOT/'Models'/modelname
   if not dest.exists():shutil.copy2(src,dest)
   elif sha(dest)!=sha(src):raise SystemExit('Model collision: '+modelname)
   m[1]='${KIPRJMOD}/Models/'+modelname
  expected=dump(d)+'\n'
  if dst.exists() and dst.read_text(encoding='utf-8')!=expected:raise SystemExit('Existing resource differs: '+str(dst))
  dst.write_text(expected,encoding='utf-8');return 'Motor:'+localname
 mosfp=resource('Package_TO_SOT_SMD','TO-252-2')
 fusefp=resource('Fuse','Fuse_Littelfuse-NANO2-451_453','Fuse_NANO2_454_6.10x2.69mm')
 # The 454 and 451/453 use the same 6.10 x 2.69 x 2.69 mm envelope.
 # Pads are 1.96 x 3.15 mm, centers +/-2.455 mm per the 454 drawing.
 swfp='Motor:SS12D10G5_SHOUHAN_3P_P4.80mm'
 fp=n('footprint',swfp.split(':')[1],n('version',num(20260206)),n('generator',Atom('pcbnew')),n('layer','F.Cu'),n('descr','SHOU HAN SS12D10G5 C7431054, 3P SPDT, 4.8mm pitch, 12.7x6.7mm; generic envelope model'),n('attr',Atom('through_hole')))
 for label,value,y,layer in [('Reference','REF**',-5.08,'F.SilkS'),('Value','SS12D10G5',5.08,'F.Fab')]:fp.append(n('property',label,value,xy('at',4.8,y),n('layer',layer),effects(1)))
 for layer,bounds in [('F.Fab',(-1.55,-3.35,11.15,3.35)),('F.SilkS',(-1.7,-3.5,11.3,3.5)),('F.CrtYd',(-2.05,-3.85,11.65,3.85))]:
  x1,y1,x2,y2=bounds
  fp.append(n('fp_rect',xy('start',x1,y1),xy('end',x2,y2),n('stroke',n('width',num(.05 if layer=='F.CrtYd' else .12)),n('type',Atom('default'))),n('fill',Atom('none')),n('layer',layer)))
 for k in range(3):fp.append(n('pad',str(k+1),Atom('thru_hole'),Atom('oval'),xy('at',4.8*k,0),xy('size',2.8,2.4),n('drill',Atom('oval'),num(1.8),num(1.2)),n('layers','*.Cu','*.Mask')))
 fp.append(n('model','${KIPRJMOD}/Models/SS12D10G5_generic.wrl',n('offset',n('xyz',num(0),num(0),num(0))),n('scale',n('xyz',num(1),num(1),num(1))),n('rotate',n('xyz',num(0),num(0),num(0)))))
 save(ROOT/'Motor.pretty'/(swfp.split(':')[1]+'.kicad_mod'),fp)
 model=['#VRML V2.0 utf8','# Generic envelope from SHOU HAN C7431054, not an exact manufacturer assembly.']
 def box(x,y,z,dx,dy,dz,color):model.append('Transform { translation '+' '.join(str(round(v/2.54,6)) for v in [x,y,z])+' children [ Shape { appearance Appearance { material Material { diffuseColor '+color+' } } geometry Box { size '+' '.join(str(round(v/2.54,6)) for v in [dx,dy,dz])+' } } ] }')
 box(4.8,0,3.2,12.7,6.7,6.4,'.5 .5 .5');box(3.7,0,8.9,3.9,3.9,5,'.08 .08 .08')
 for x in [0,4.8,9.6]:box(x,0,-3.5,1.3,.8,7,'.75 .75 .7')
 (ROOT/'Models/SS12D10G5_generic.wrl').write_text('\n'.join(model)+'\n',encoding='utf-8')
 fuseurl='https://www.littelfuse.com/assetdocs/fuse-452-and-454-datasheet?assetguid=8c87aa93-80a7-4ea8-8749-a19ba03901c2'
 swurl='https://datasheet.lcsc.com/lcsc/2306211618_SHOU-HAN-SS12D10G5_C7431054.pdf'
 mosurl='https://www.diodes.com/datasheet/download/DMP3007LK3.pdf'
 changes={};libnew={};pinmap={}
 for file in sorted(p.name for p in BASE.glob('*.kicad_sch')):
  d=parse((ROOT/file).read_text(encoding='utf-8'));node(node(d,'title_block'),'rev')[1]='M2.3'
  if file=='02_Power.kicad_sch':
   lib=node(d,'lib_symbols')
   for ref in ['Q1','Q2']:
    z=next(z for z in nodes(d,'symbol') if prop(z,'Reference')[2]==ref);oldid=node(z,'lib_id')[1];oldlib=next(x for x in nodes(lib,'symbol') if x[1]==oldid);new=copy.deepcopy(oldlib)
    newname='DMP3007LK3'+('_Mirrored' if ref=='Q2' else '');new[1]='Motor:'+newname
    oldname=oldid.split(':')[1]
    for sub in nodes(new,'symbol'):
     sub[1]=sub[1].replace(oldname,newname)
     for p in list(nodes(sub,'pin')):
      pn=node(p,'number')[1]
      if pn not in ['1','4','5']:sub.remove(p)
      else:node(p,'number')[1]={'1':'3','4':'1','5':'2'}[pn]
    for key,value in [('Value','DMP3007LK3-13'),('Footprint',mosfp),('Datasheet',mosurl)]:field(new,key,value);field(z,key,value)
    field(z,'MPN','DMP3007LK3-13');field(z,'LCSC','C3281309');node(z,'lib_id')[1]=new[1]
    for p in list(nodes(z,'pin')):
     if p[1] not in ['1','4','5']:z.remove(p)
     else:p[1]={'1':'3','4':'1','5':'2'}[p[1]]
    lib.remove(oldlib);lib.append(new);local=copy.deepcopy(new);local[1]=newname;libnew[oldname]=local
    pinmap.update({f'{ref}:{k}':f'{ref}:'+('3' if k in [1,2,3] else '1' if k==4 else '2') for k in range(1,9)})
    changes[ref]={'Value':'DMP3007LK3-13','Footprint':mosfp,'Datasheet':mosurl,'MPN':'DMP3007LK3-13','LCSC':'C3281309','lib_id':new[1]}
   sw=next(z for z in nodes(d,'symbol') if prop(z,'Reference')[2]=='SW1');oldid=node(sw,'lib_id')[1];oldlib=next(x for x in nodes(lib,'symbol') if x[1]==oldid);new=copy.deepcopy(oldlib);new[1]='Motor:SS12D10G5_3P'
   for sub in nodes(new,'symbol'):
    sub[1]=sub[1].replace(oldid.split(':')[1],'SS12D10G5_3P')
    for p in list(nodes(sub,'pin')):
     if node(p,'number')[1] in ['4','5']:sub.remove(p)
   for p in list(nodes(sw,'pin')):
    if p[1] in ['4','5']:sw.remove(p)
   for key,value in [('Value','SS12D10G5'),('Footprint',swfp),('Datasheet',swurl)]:field(sw,key,value);field(new,key,value)
   field(sw,'MPN','SS12D10G5');field(sw,'LCSC','C7431054');node(sw,'lib_id')[1]=new[1];lib.remove(oldlib);lib.append(new);local=copy.deepcopy(new);local[1]='SS12D10G5_3P';libnew[oldid.split(':')[1]]=local
   changes['SW1']={'Value':'SS12D10G5','Footprint':swfp,'Datasheet':swurl,'MPN':'SS12D10G5','LCSC':'C7431054','lib_id':new[1]};pinmap.update({'SW1:4':None,'SW1:5':None})
   d.remove(next(z for z in nodes(d,'symbol') if prop(z,'Reference')[2]=='#PWR003'))
   d.remove(next(w for w in nodes(d,'wire') if [tuple(map(float,p[1:3])) for p in node(w,'pts')[1:]]==[(143.51,69.85),(143.51,81.28)]))
   for ref,value in [('R1','100k'),('R2','4.7k'),('F1','12A / 75V 延时')]:
    z=next(z for z in nodes(d,'symbol') if prop(z,'Reference')[2]==ref);field(z,'Value',value);changes[ref]={'Value':value}
    if ref=='F1':
     for key,val in [('Footprint',fusefp),('Datasheet',fuseurl),('MPN','0454012.MR')]:field(z,key,val);changes[ref][key]=val
   z=next(z for z in nodes(d,'text') if z[1].startswith('三串锂电池9'))
   z[1]='整车预算最高约八点四安，主保险十二安延时。\n开关只控制栅极，双 MOS 承担负载电流。\n步进电源不低于十伏，启动、短路与温升须验证。'
  if file=='06_Module_Power.kicad_sch':
   z=next(z for z in nodes(d,'text') if z[1].startswith('外设预算 0.15A'));z[1]='外设预算 0.15A，与 Core 电源分开。\n主路按十安等级核算，持续能力须验证。'
  if file=='09_Bluetooth_Stepper.kicad_sch':
   fuse=copy.deepcopy(next(z for z in nodes(parse((ROOT/'02_Power.kicad_sch').read_text(encoding='utf-8')),'symbol') if prop(z,'Reference')[2]=='F1'))
   node(fuse,'uuid')[1]=uid('F2');put(fuse,228.6,109.22)
   for key,value in [('Reference','F2'),('Value','4A / 125V 延时'),('MPN','0454004.MR')]:field(fuse,key,value)
   for p in nodes(fuse,'pin'):node(p,'uuid')[1]=uid('F2/pin/'+p[1])
   for p in nodes(fuse,'property'):put(p,228.6,109.22)
   put(prop(fuse,'Reference'),228.6,100.33);put(prop(fuse,'Value'),228.6,102.87)
   project=node(node(fuse,'instances'),'project');path=node(project,'path');path[1]='/'+node(d,'uuid')[1] # replaced below with the actual root/sheet path
   peer=next(z for z in nodes(d,'symbol') if prop(z,'Reference')[2]=='J24');path[1]=node(node(node(peer,'instances'),'project'),'path')[1];node(path,'reference')[1]='F2'
   d.append(fuse)
   fcache=next(z for z in nodes(node(parse((ROOT/'02_Power.kicad_sch').read_text(encoding='utf-8')),'lib_symbols'),'symbol') if z[1]==node(fuse,'lib_id')[1]);node(d,'lib_symbols').append(copy.deepcopy(fcache))
   for w in list(nodes(d,'wire')):
    pp=[tuple(map(float,p[1:3])) for p in node(w,'pts')[1:]]
    if pp in [[(226.06,109.22),(233.68,109.22)],[(233.68,109.22),(233.68,125.73)]]:d.remove(w)
   port=next(z for z in nodes(d,'hierarchical_label') if z[1]=='VM');put(port,213.36,109.22)
   wire(d,(213.36,109.22),(224.79,109.22));wire(d,(232.41,109.22),(233.68,109.22));wire(d,(233.68,109.22),(233.68,125.73))
   for z in nodes(d,'label'):
    if z[1]=='VM':z[1]='VM_STEP'
   d.append(n('label','VM_STEP',xy('at',233.68,109.22,num(0)),effects(1.016,'left bottom'),n('uuid',uid('step-power-label'))))
   z=next(z for z in nodes(d,'text') if z[1].startswith('J23 八针'))
   z[1]='J23 八针仅接 1、2、7、8 脚。\nTX 接 R/A/H，RX 接 T/B/L。\n步进输入预算三安，最低十伏。\n主保险十二安，支路四安延时。'
  save(ROOT/file,d)
 libpath=ROOT/'Motor.kicad_sym';lib=parse(libpath.read_text(encoding='utf-8'))
 for old,new in libnew.items():lib.remove(next(z for z in nodes(lib,'symbol') if z[1]==old));lib.append(new)
 save(libpath,lib)
 # Refresh the schematic draft directly from actual instantiated properties.
 parts={}
 for p in ROOT.glob('*.kicad_sch'):
  for z in nodes(parse(p.read_text(encoding='utf-8')),'symbol'):
   if not prop(z,'Reference')[2].startswith('#'):parts[prop(z,'Reference')[2]]=z
 bom=ROOT/'docs/BOM_M2_SCHEMATIC_DRAFT.csv';rows=list(csv.DictReader(bom.open(encoding='utf-8-sig')))
 for row in rows:
  z=parts[row['reference']]
  for key,fieldname in [('value','Value'),('mpn','MPN'),('footprint','Footprint'),('datasheet','Datasheet')]:row[key]=prop(z,fieldname)[2] if prop(z,fieldname) else ''
 row=copy.deepcopy(next(r for r in rows if r['reference']=='F1'));row['reference']='F2';row['sheet']=next(r['sheet'] for r in rows if r['reference']=='J23')
 for key,fld in [('value','Value'),('mpn','MPN'),('footprint','Footprint'),('datasheet','Datasheet')]:row[key]=prop(parts['F2'],fld)[2]
 rows.append(row)
 with bom.open('w',encoding='utf-8-sig',newline='') as f:
  w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
 budget={'battery_voltage_range_v':[10,12.6],'wheel_input_budget_a_each':2,'wheel_count':2,'stepper_input_budget_a':3,'stepper_budget_is_measurement':False,'core_5v_output_budget_a':.75,'peripheral_5v_output_budget_a':1.5,'assumed_buck_efficiency':.85,'threev3_is_included_in_5v_not_added_again':True,'max_low_voltage_output_power_w':11.25,'input_current_a':{str(v):round(7+11.25/(v*.85),4) for v in [10,12,12.6]},'normal_operation_current_a':'unknown; measure actual motors and modules','fuse_standard_continuous_derating':.75,'F1_continuous_design_budget_a_at_25c':9,'MOS_cold_pair_loss_w_at_10v_input_budget_using_10mohm_each':round(2*(7+11.25/8.5)**2*.01,4),'J23_XH_continuous_rating_a':3,'F2_is_not_an_exact_3a_current_limiter':True}
 record={'date':'2026-11-06','revision':'M2.3','baseline':str(BASE.relative_to(ROOT)),'changes':changes,'pin_map_from_m2':pinmap,'removed_virtual_symbols':['#PWR003'],'new_references':['F2'],'F2':{'Value':'4A / 125V 延时','Footprint':fusefp,'Datasheet':fuseurl,'MPN':'0454004.MR'},'stepper_supply_net':'VM_STEP','budget':budget,'switch_model':'generic envelope from manufacturer drawing; not exact assembly','fuse_model':'generic 454 envelope 6.10x2.69x2.69mm; exact installed library model unavailable','source_sha256':{p.name:sha(p) for p in ROOT.glob('*.kicad_sch')},'pcb_accessed':False}
 (ROOT/'docs/power_revision.json').write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
 print('M2.3 power edit saved; native ERC/netlist and resource validation required.')
if __name__=='__main__':main()
