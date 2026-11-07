"""Unify Motor's 0603 inner pad gap, retaining typed models and LED polarity."""
from pathlib import Path
import copy,csv,hashlib,json,sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT.parent/'Shared/design_tools'))
from sexpr import parse,dump,node,nodes,prop,Atom
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 if '--apply-live-baseline' not in sys.argv:raise SystemExit('Historical edit only; explicit --apply-live-baseline required.')
 record=ROOT/'docs/passive_0603_revision.json'
 if record.exists():raise SystemExit('Already applied; use KiCad for further edits.')
 base=ROOT/'docs/baseline-2026-11-06-0603';base.mkdir(exist_ok=True)
 for p in [*ROOT.glob('*.kicad_sch'),ROOT/'Motor.kicad_sym',ROOT/'docs/BOM_M2_SCHEMATIC_DRAFT.csv',ROOT/'docs/netlist.xml',ROOT/'docs/erc.json']:
  dest=base/p.name
  if dest.exists() and dest.read_bytes()!=p.read_bytes():raise SystemExit('Existing baseline differs: '+str(dest))
  dest.write_bytes(p.read_bytes())
 stock=Path('C:/Program Files/KiCad/10.0/share/kicad/footprints')
 choices={'R':('Resistor_SMD','R_0603_1608Metric_Pad0.98x0.95mm_HandSolder','Resistor_SMD__R_0603_1608Metric'),
 'C':('Capacitor_SMD','C_0603_1608Metric_Pad1.08x0.95mm_HandSolder','Capacitor_SMD__C_0603_1608Metric'),
 'LED':('LED_SMD','LED_0603_1608Metric_Pad1.05x0.95mm_HandSolder','LED_SMD__LED_0603_1608Metric')}
 newfp={}
 for kind,(cat,name,oldname) in choices.items():
  d=parse((stock/(cat+'.pretty')/(name+'.kicad_mod')).read_text(encoding='utf-8'));local=kind+'_0603_1608Metric_Gap0.85mm_HandSolder';d[1]=local
  old=parse((ROOT/'Motor.pretty'/(oldname+'.kicad_mod')).read_text(encoding='utf-8'))
  for m in list(nodes(d,'model')):d.remove(m)
  d.extend(copy.deepcopy(nodes(old,'model')))
  for p in nodes(d,'pad'):
   if p[1] not in ['1','2']:continue
   sign=-1 if float(node(p,'at')[1])<0 else 1;node(p,'at')[1]=Atom(str(sign*.9125));node(p,'size')[1:]=[Atom('0.975'),Atom('0.95')]
  node(d,'descr')[1]='0603 imperial / 1608 metric; 0.85mm inner gap, 0.975x0.95mm hand-solder pads; typed geometry/model retained'
  dest=ROOT/'Motor.pretty'/(local+'.kicad_mod')
  if dest.exists():raise SystemExit('Refusing footprint collision: '+str(dest))
  dest.write_text(dump(d)+'\n',encoding='utf-8');newfp[kind]='Motor:'+local
 changes={}
 for path in ROOT.glob('*.kicad_sch'):
  d=parse(path.read_text(encoding='utf-8'))
  for z in nodes(d,'symbol'):
   fp=prop(z,'Footprint')
   if not fp or '0603_1608Metric' not in fp[2]:continue
   kind='LED' if 'LED_' in fp[2] else 'R' if 'Resistor_' in fp[2] else 'C' if 'Capacitor_' in fp[2] else None
   if kind is None:raise SystemExit('Unsupported 0603 footprint '+fp[2])
   ref=prop(z,'Reference')[2];changes[ref]={'old_footprint':fp[2],'footprint':newfp[kind],'kind':kind};fp[2]=newfp[kind]
  path.write_text(dump(d)+'\n',encoding='utf-8')
 bom=ROOT/'docs/BOM_M2_SCHEMATIC_DRAFT.csv';rows=list(csv.DictReader(bom.open(encoding='utf-8-sig')))
 for row in rows:
  if row['reference'] in changes:row['footprint']=changes[row['reference']]['footprint']
 with bom.open('w',encoding='utf-8-sig',newline='') as f:
  w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
 data={'date':'2026-11-06','scope':'Motor nine schematic pages only; no PCB access','baseline':str(base.relative_to(ROOT)),'count':len(changes),'gap_mm':.85,'pad_size_mm':[.975,.95],'pad_center_x_mm':[-.9125,.9125],'largest_checked_stock_gap_mm':.85,'stock_gap_comparison_mm':{'R normal and hand-solder':.85,'C normal and hand-solder':.65,'LED normal and hand-solder':.7},'typed_models_and_LED_polarity_retained':True,'changes':changes,'footprint_sha256':{fp:sha(ROOT/'Motor.pretty'/(fp.split(':')[1]+'.kicad_mod')) for fp in newfp.values()},'source_sha256':{p.name:sha(p) for p in ROOT.glob('*.kicad_sch')}}
 record.write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf-8');print('Unified',len(changes),'0603 parts to 0.85mm gap; electrical connectivity unchanged by this step.')
if __name__=='__main__':main()
