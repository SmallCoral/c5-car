"""Apply the user's female-header / ordinary XH connector choices, schematic only."""
from pathlib import Path
import csv,hashlib,json,sys,shutil
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'docs/baseline-2026-11-06-connector-types'
sys.path.insert(0,str(ROOT.parent/'Shared/design_tools'))
from sexpr import parse,dump,node,nodes,prop

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 if '--apply-saved-baseline' not in sys.argv:raise SystemExit('Historical edit only; explicit --apply-saved-baseline required.')
 if (ROOT/'docs/connector_revision.json').exists():raise SystemExit('Connector edit already applied; use KiCad for later edits.')
 for p in BASE.glob('*.kicad_sch'):
  if sha(ROOT/p.name)!=sha(p):raise SystemExit('Refusing later manual edits: '+p.name)
 if sha(ROOT/'Motor.kicad_sym')!=sha(BASE/'Motor.kicad_sym'):raise SystemExit('Local symbol library changed after baseline.')
 choices={'J15':('Connector_PinSocket_2.54mm','PinSocket_1x04_P2.54mm_Vertical','female header'),
 'J17':('Connector_PinSocket_2.54mm','PinSocket_1x08_P2.54mm_Vertical','female header')}
 for ref,count in [('J10',2),('J18',4),('J19',4),('J20',10),('J21',4),('J22',6),('J23',8)]:
  choices[ref]=('Connector_JST',f'JST_XH_B{count}B-XH-A_1x{count:02d}_P2.50mm_Vertical','ordinary XH; not XHB positive latch')
 stock=Path('C:/Program Files/KiCad/10.0/share/kicad');resource={}
 for category,name,kind in set(choices.values()):
  localname=category+'__'+name;fp=ROOT/'Motor.pretty'/(localname+'.kicad_mod')
  if not fp.exists():
   d=parse((stock/'footprints'/(category+'.pretty')/(name+'.kicad_mod')).read_text(encoding='utf-8'));d[1]=localname
   for m in nodes(d,'model'):
    rel=m[1].replace('${KICAD10_3DMODEL_DIR}/','');src=stock/'3dmodels'/rel
    if not src.is_file():raise SystemExit('Exact KiCad model unavailable: '+str(src))
    modelname=category+'__'+Path(rel).name;dst=ROOT/'Models'/modelname
    if dst.exists() and sha(src)!=sha(dst):raise SystemExit('Refusing to overwrite existing model: '+modelname)
    if not dst.exists():shutil.copy2(src,dst)
    m[1]='${KIPRJMOD}/Models/'+modelname
   fp.write_text(dump(d)+'\n',encoding='utf-8')
  resource[localname]=sha(fp)
 changes={};libchanges={}
 for path in ROOT.glob('*.kicad_sch'):
  d=parse(path.read_text(encoding='utf-8'))
  for z in nodes(d,'symbol'):
   ref=prop(z,'Reference')[2]
   if ref not in choices:continue
   cat,name,kind=choices[ref];new='Motor:'+cat+'__'+name;old=prop(z,'Footprint')[2];prop(z,'Footprint')[2]=new
   libid=node(z,'lib_id')[1];libchanges[libid]=new
   changes[ref]={'old_footprint':old,'footprint':new,'type':kind,'lib_id':libid,'source':str(path.name)}
  if path.name=='07_TFT_Buttons.kicad_sch':
   z=next(z for z in nodes(d,'text') if z[1].startswith('三伏三供电，屏幕板'));z[1]='J17 为 2.54mm 八位排母；三伏三供电，针序按屏幕资料。'
  if path.name=='09_Bluetooth_Stepper.kicad_sch':
   z=next(z for z in nodes(d,'text') if z[1].startswith('默认 1—2 跳帽'))
   z[1]='J15 为 2.54mm 四位排母。\n默认 1—2 跳帽选择三伏三。\n确认稳压载板后才可选五伏。\n四脚线束按本板针序制作。'
  path.write_text(dump(d)+'\n',encoding='utf-8')
 # Keep the library and all its schematic caches synchronized for these defaults.
 libpath=ROOT/'Motor.kicad_sym';d=parse(libpath.read_text(encoding='utf-8'))
 for z in nodes(d,'symbol'):
  name='Motor:'+z[1]
  if name in libchanges and prop(z,'Footprint'):prop(z,'Footprint')[2]=libchanges[name]
 libpath.write_text(dump(d)+'\n',encoding='utf-8')
 for path in ROOT.glob('*.kicad_sch'):
  d=parse(path.read_text(encoding='utf-8'))
  for z in nodes(node(d,'lib_symbols'),'symbol'):
   if z[1] in libchanges and prop(z,'Footprint'):prop(z,'Footprint')[2]=libchanges[z[1]]
  path.write_text(dump(d)+'\n',encoding='utf-8')
 bom=ROOT/'docs/BOM_M2_SCHEMATIC_DRAFT.csv';rows=list(csv.DictReader(bom.open(encoding='utf-8-sig')))
 for row in rows:
  if row['reference'] in changes:row['footprint']=changes[row['reference']]['footprint']
 with bom.open('w',encoding='utf-8-sig',newline='') as f:
  w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
 record={'date':'2026-11-06','revision':'M2.2','baseline':str(BASE.relative_to(ROOT)),'requested':'TFT and HC-08 female headers; other harness ports ordinary XH, not XHB latch','changes':changes,'unchanged_exceptions':{'J1':'XT60 power inlet','J14':'Core edge socket','J16':'3P male jumper selector','J24':'5.08mm screw power terminal retained for separate power wiring'},'XH_pitch_mm':2.5,'XH_rating':'3A with AWG22 per JST; not an arbitrary-current power connector','resources_sha256':resource,'source_sha256':{p.name:sha(p) for p in ROOT.glob('*.kicad_sch')}}
 (ROOT/'docs/connector_revision.json').write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
 print('Female TFT/BT and ordinary XH connector footprints/models applied; electrical netlist verification required.')
if __name__=='__main__':main()
