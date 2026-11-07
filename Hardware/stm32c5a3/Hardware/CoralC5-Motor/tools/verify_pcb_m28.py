"""Read-only component/net synchronization and preservation audit, not routing QA."""
from pathlib import Path
from datetime import datetime
import sys,json,hashlib,xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parents[1];DOC=ROOT/'docs'
sys.path.insert(0,str(ROOT.parent/'Shared/design_tools'))
from sexpr import parse,node,nodes,prop
from m28_contract import ADDED_PHYSICAL
def main():
    record=json.loads((DOC/'pcb_sync_m28.json').read_text(encoding='utf8'))
    baseline=ROOT/record['baseline_file'];old=parse(baseline.read_text(encoding='utf8'));now=parse((ROOT/'CoralC5-Motor.kicad_pcb').read_text(encoding='utf8'))
    before={prop(f,'Reference')[2]:f for f in nodes(old,'footprint')};parts={prop(f,'Reference')[2]:f for f in nodes(now,'footprint')}
    tree=ET.parse(DOC/'netlist.xml');components={c.get('ref'):c for c in tree.findall('.//components/comp')}
    def board_name(name):
        return name.replace('/','{slash}') if name.startswith(('Net-(','unconnected-(')) else name
    pins={(p.get('ref'),p.get('pin')):board_name(n.get('name')) for n in tree.findall('.//nets/net') for p in n.findall('node')}
    checks=[]
    def ck(name,ok,detail=None):checks.append({'name':name,'pass':bool(ok),'detail':detail})
    ck('input PCB baseline SHA',hashlib.sha256(baseline.read_bytes()).hexdigest()==record['input_pcb_sha256'])
    ck('180 exact physical references',set(parts)==set(components) and len(parts)==180)
    ck('exactly33 staged additions; no deletions',set(parts)-set(before)==ADDED_PHYSICAL and set(before)<=set(parts))
    for ref,f in parts.items():
        c=components[ref];ck(ref+' footprint identity',f[1]==c.findtext('footprint'));ck(ref+' value',prop(f,'Value')[2]==c.findtext('value'))
        ck(ref+' schematic UUID path',node(f,'path')[1]==c.find('sheetpath').get('tstamps')+c.findtext('tstamps').split()[0])
        for field in c.findall('fields/field'):
            p=prop(f,field.get('name'));ck(ref+' field '+field.get('name'),p is not None and p[2]==(field.text or ''))
        for pad in nodes(f,'pad'):
            if not pad[1]:continue
            actual=node(pad,'net');expected=pins.get((ref,pad[1]));ck(ref+':'+pad[1]+' exact native pad net',actual is not None and actual[1]==expected,{'expected':expected,'actual':actual})
        if ref in before:
            b=before[ref]
            for tag in ['uuid','at','layer','locked']:ck(ref+' user '+tag+' retained',nodes(f,tag)==nodes(b,tag))
            for p in nodes(b,'property'):
                q=prop(f,p[1]);ck(ref+' property '+p[1]+' identity/pose',q is not None and all(nodes(q,k)==nodes(p,k) for k in ['uuid','at','layer','hide','effects']))
            if ref!='U1':
                ignored={'property','path','sheetname','sheetfile','units','pad'}
                ck(ref+' full package geometry/model/constraints retained',[x for x in f if not isinstance(x,list) or x[0] not in ignored]==[x for x in b if not isinstance(x,list) or x[0] not in ignored])
                def pads(shape):return [[x for x in p if not isinstance(x,list) or x[0] not in ['net','pinfunction','pintype']] for p in nodes(shape,'pad')]
                ck(ref+' all pad geometries and UUIDs retained',pads(f)==pads(b))
        else:ck(ref+' outside original component envelope',float(node(f,'at')[1])>=330)
    for kind in {str(x[0]) for x in old if isinstance(x,list) and x[0]!='footprint'}:
        ck(kind+' all original objects exactly retained',nodes(old,kind)==nodes(now,kind))
    for ref in ['U1','U6']:
        f=parts[ref];lib=parse((ROOT/'Motor.pretty/Diodes_SO8EP_AP64500.kicad_mod').read_text(encoding='utf8'))
        def geom(obj):return sorted((p[1],*map(float,node(p,'at')[1:3]),*map(float,node(p,'size')[1:3])) for p in nodes(obj,'pad'))
        ck(ref+' new primary package land pattern',geom(f)==geom(lib))
        angle=float(node(f,'at')[3]) if len(node(f,'at'))>3 else 0
        ck(ref+' pad rotations include package rotation',all(abs((float(node(p,'at')[3])-angle)%360)<1e-6 for p in nodes(f,'pad')))
    actual={p[1]:node(p,'uuid')[1] for p in nodes(parts['U1'],'pad') if p[1]}
    for p in record['U1_pad_uuid_map']:ck('U1 pin'+p['new_pin']+' explicit UUID migration',actual[p['new_pin']]==p['retained_uuid'])
    drc=json.loads((DOC/'pcb-drc-m28.json').read_text(encoding='utf8'))
    ck('native schematic parity zero',not drc['schematic_parity'],drc['schematic_parity'])
    result={'date':'2026-11-07','revision':'M2.8','kicad_version':drc['kicad_version'],'checks':len(checks),'failed':[c for c in checks if not c['pass']],'results':checks,'pcb_sha256':hashlib.sha256((ROOT/'CoralC5-Motor.kicad_pcb').read_bytes()).hexdigest(),'physical_references':len(parts),'retained_segments':len(nodes(now,'segment')),'retained_vias':len(nodes(now,'via')),'added_staged_references':sorted(ADDED_PHYSICAL),'schematic_parity':len(drc['schematic_parity']),'drc_violations':len(drc['violations']),'unconnected_items':len(drc['unconnected_items']),'routing_pass':not drc['violations'] and not drc['unconnected_items'],'limitations':['Component/net synchronization is separate from copper correctness. The existing layout has no Edge.Cuts outline and has not passed DRC.','All original copper is preserved. U1 lands and split power rails require manual rerouting; 33 new components still need placement, copper, decoupling/COMP layout and thermal design.','No physical board, cable power integrity, startup or closed-loop system verification performed.']}
    result['run_at']=datetime.now().astimezone().isoformat(timespec='seconds')
    result['pass']=not result['failed'];(DOC/'pcb_sync_verification.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print(json.dumps({k:v for k,v in result.items() if k not in ['results']},ensure_ascii=False,indent=2));return 0 if result['pass'] else 1
if __name__=='__main__':raise SystemExit(main())
