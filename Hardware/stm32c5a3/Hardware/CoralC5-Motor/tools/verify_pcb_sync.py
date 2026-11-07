"""Read-only component/net/identity audit; does not validate routing or edit PCB."""
from pathlib import Path
import sys,json,hashlib,xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parents[1];DOC=ROOT/'docs'
sys.path.insert(0,str(ROOT.parent/'Shared/design_tools'))
from sexpr import parse,node,nodes,prop

def main():
    if (DOC/'f8_association_repair.json').exists():
        from verify_pcb_associations import main as verify_associations
        return verify_associations()
    if (DOC/'pcb_sync_m28.json').exists():
        from verify_pcb_m28 import main as verify_m28
        return verify_m28()
    sync=json.loads((DOC/'pcb_sync_m27.json').read_text(encoding='utf8'))
    old=parse((ROOT/sync['baseline_file']).read_text(encoding='utf8'))
    current=parse((ROOT/'CoralC5-Motor.kicad_pcb').read_text(encoding='utf8'))
    oldparts={prop(f,'Reference')[2]:f for f in nodes(old,'footprint')}
    now={prop(f,'Reference')[2]:f for f in nodes(current,'footprint')}
    tree=ET.parse(DOC/'netlist.xml');components={c.get('ref'):c for c in tree.findall('.//components/comp')}
    pins={}
    for n in tree.findall('.//nets/net'):
        for p in n.findall('node'):pins[(p.get('ref'),p.get('pin'))]=n.get('name')
    checks=[]
    def ck(name,ok,detail=None):checks.append(dict(name=name,pass_=bool(ok),detail=detail))
    def normalized(name):return name.replace('{slash}','/')
    ck('exact component reference set',set(now)==set(components))
    ck('latest user PCB reference set preserved',set(now)==set(oldparts))
    for ref,f in now.items():
        c=components.get(ref)
        if c is None:continue
        ck(ref+' footprint matches schematic',f[1]==c.findtext('footprint'))
        ck(ref+' value matches schematic',prop(f,'Value')[2]==c.findtext('value'))
        path=c.find('sheetpath').get('tstamps')+c.findtext('tstamps').split()[0]
        ck(ref+' schematic UUID path correct',node(f,'path') is not None and node(f,'path')[1]==path)
        if ref in oldparts:
            before=oldparts[ref]
            ck(ref+' footprint UUID retained',node(f,'uuid')==node(before,'uuid'))
            ck(ref+' user position/rotation retained',node(f,'at')==node(before,'at'))
            ck(ref+' pad UUIDs retained',{node(p,'uuid')[1] for p in nodes(f,'pad')}=={node(p,'uuid')[1] for p in nodes(before,'pad')})
        for p in nodes(f,'pad'):
            if not p[1]:continue
            expected=pins.get((ref,p[1]));actual=node(p,'net')
            ck(ref+':'+p[1]+' net matches',actual is not None and expected is not None and normalized(actual[1])==normalized(expected),dict(expected=expected,actual=actual[1] if actual else None))
    for kind in ['segment','via','zone','gr_line','gr_arc','gr_rect','setup','layers','property']:
        ck(kind+' user objects unchanged',nodes(old,kind)==nodes(current,kind))
    for ref in ['U1','J19']:
        f=now[ref];lib=parse((ROOT/'Motor.pretty'/(f[1].split(':')[1]+'.kicad_mod')).read_text(encoding='utf8'))
        def geometry(part):
            return sorted((p[1],*map(float,node(p,'at')[1:3]),*map(float,node(p,'size')[1:3])) for p in nodes(part,'pad'))
        ck(ref+' board lands match corrected local footprint',geometry(f)==geometry(lib))
    drc=json.loads((DOC/'pcb-drc-m27.json').read_text(encoding='utf8'))
    ck('native schematic parity is zero',not drc['schematic_parity'])
    for row in checks:row['pass']=row.pop('pass_')
    result=dict(date='2026-11-07',checks=len(checks),failed=[c for c in checks if not c['pass']],results=checks,pcb_sha256=hashlib.sha256((ROOT/'CoralC5-Motor.kicad_pcb').read_bytes()).hexdigest(),schematic_parity=len(drc['schematic_parity']),drc_violations=len(drc['violations']),unconnected_items=len(drc['unconnected_items']),limitation='Component/net synchronization is verified. Native DRC remains failing; this report does not assert routing/manufacturing readiness.')
    result['pass']=not result['failed'];(DOC/'pcb_sync_verification.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print(json.dumps({k:v for k,v in result.items() if k!='results'},ensure_ascii=False,indent=2))
    return 0 if result['pass'] else 1

if __name__=='__main__':raise SystemExit(main())
