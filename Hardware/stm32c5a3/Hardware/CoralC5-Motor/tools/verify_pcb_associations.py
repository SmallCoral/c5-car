"""Read-only F8 UUID matching preflight, including all units of each symbol.

KiCad10 BOARD_NETLIST_UPDATER matches footprint.path against component.sheetpath
plus each symbol UUID. A reference-based netlist DRC does not establish this.
This simulates that matching step; it does not drive the GUI update dialog.
"""
from pathlib import Path
from datetime import datetime
import argparse,collections,hashlib,json,sys,subprocess,shutil,xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT.parent/'Shared/design_tools'))
from sexpr import parse,node,nodes,prop
def audit(board_path,netlist_path):
    board=parse(board_path.read_text(encoding='utf8'));fps=nodes(board,'footprint')
    tree=ET.parse(netlist_path);components=[c for c in tree.findall('.//components/comp') if not any(p.get('name')=='exclude_from_board' for p in c.findall('property'))]
    refs=collections.Counter(prop(f,'Reference')[2] for f in fps)
    # KiCad deliberately keeps board-only artwork without a schematic symbol.
    board_only={node(f,'uuid')[1] for f in fps if 'board_only' in (node(f,'attr') or [])}
    paths=collections.defaultdict(list)
    for f in fps:
        p=node(f,'path')
        if p:paths[p[1]].append(f)
    matches=[];additions=[];ambiguous=[];wrong_ref=[];replacements=[];used=set();pin_errors=[];value_errors=[]
    def board_name(n):return n.replace('/','{slash}') if n.startswith(('Net-(','unconnected-(')) else n
    pin_nets={(p.get('ref'),p.get('pin')):board_name(n.get('name')) for n in tree.findall('.//nets/net') for p in n.findall('node')}
    for c in components:
        ref=c.get('ref');sheet=c.find('sheetpath').get('tstamps');expected=[sheet+u for u in c.findtext('tstamps').split()]
        found=[f for path in expected for f in paths[path]]
        if not found:additions.append(ref)
        if len(found)>1:ambiguous.append(ref)
        row={'reference':ref,'valid_paths':expected,'matched_footprint_uuids':[node(f,'uuid')[1] for f in found]};matches.append(row)
        for f in found:
            used.add(node(f,'uuid')[1]);fr=prop(f,'Reference')[2]
            if fr!=ref:wrong_ref.append({'schematic_reference':ref,'pcb_reference':fr})
            if f[1]!=c.findtext('footprint'):replacements.append(ref)
            if prop(f,'Value')[2]!=c.findtext('value'):value_errors.append(ref)
            for pad in nodes(f,'pad'):
                if not pad[1]:continue
                n=node(pad,'net');wanted=pin_nets.get((ref,pad[1]))
                if n is None or n[1]!=wanted:pin_errors.append({'reference':ref,'pad':pad[1],'expected':wanted,'actual':n[1] if n else None})
    result={'checked_at':datetime.now().astimezone().isoformat(timespec='seconds'),'method':'KiCad10 UUID path lookup: component sheetpath + each component UUID; no reference fallback','source':'https://github.com/KiCad/kicad-source-mirror/blob/10.0/pcbnew/netlist_reader/board_netlist_updater.cpp','schematic_components':len(components),'pcb_footprints':len(fps),'board_only_footprints':[prop(f,'Reference')[2] for f in fps if node(f,'uuid')[1] in board_only],'duplicate_pcb_references':{k:v for k,v in refs.items() if v>1},'missing_pcb_paths':[prop(f,'Reference')[2] for f in fps if node(f,'path') is None and node(f,'uuid')[1] not in board_only],'predicted_additions_by_uuid':sorted(additions),'ambiguous_symbol_matches':ambiguous,'unmatched_pcb_footprints':[prop(f,'Reference')[2] for f in fps if node(f,'uuid')[1] not in used|board_only],'reference_mismatches':wrong_ref,'predicted_footprint_type_replacements':sorted(replacements),'value_mismatches':value_errors,'pad_net_mismatches':pin_errors,'pcb_sha256':hashlib.sha256(board_path.read_bytes()).hexdigest(),'native_netlist_sha256':hashlib.sha256(netlist_path.read_bytes()).hexdigest(),'matches':matches,'limitation':'Exact UUID matching preflight only; GUI memory, update options, library/field refresh, copper DRC and physical hardware are separate.'}
    result['pass']=len(components)==len(fps)-len(board_only-used) and not any(result[k] for k in ['duplicate_pcb_references','missing_pcb_paths','predicted_additions_by_uuid','ambiguous_symbol_matches','unmatched_pcb_footprints','reference_mismatches','predicted_footprint_type_replacements','value_mismatches','pad_net_mismatches'])
    return result
def main():
    record_path=ROOT/'docs/f8_association_repair.json'
    repair=json.loads(record_path.read_text(encoding='utf8')) if record_path.exists() else None
    ap=argparse.ArgumentParser();ap.add_argument('--board',type=Path,default=ROOT/'CoralC5-Motor.kicad_pcb');ap.add_argument('--netlist',type=Path,help='Explicit saved netlist for historical repro; default exports current schematic');ap.add_argument('--output',type=Path,default=ROOT/'docs/f8_association_verification.json');ap.add_argument('--kicad-cli',default=shutil.which('kicad-cli') or 'D:/KiCad/10.0/bin/kicad-cli.exe');args=ap.parse_args()
    source_hashes={}
    if args.netlist is None:
        args.netlist=ROOT/'docs/f8_current_netlist.xml'
        def hashes():return {p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in ROOT.glob('*.kicad_sch')}
        for attempt in range(3):
            source_hashes=hashes()
            subprocess.run([args.kicad_cli,'sch','export','netlist','--format','kicadxml','--output',str(args.netlist),str(ROOT/'CoralC5-Motor.kicad_sch')],check=True,capture_output=True)
            if source_hashes==hashes():break
        else:raise RuntimeError('Schematic changes during export; save a stable design and retry')
    result=audit(args.board,args.netlist)
    result['fresh_export_source_sha256']=source_hashes
    if repair and args.board.resolve()==(ROOT/'CoralC5-Motor.kicad_pcb').resolve():
        baseline=ROOT/repair['baseline'];saved=baseline/'CoralC5-Motor.kicad_pcb'
        old=parse(saved.read_text(encoding='utf8'));now=parse(args.board.read_text(encoding='utf8'))
        prior={prop(f,'Reference')[2]:f for f in nodes(old,'footprint')};current={prop(f,'Reference')[2]:f for f in nodes(now,'footprint')}
        def no_path(f):return [x for x in f if not isinstance(x,list) or x[0]!='path']
        checks=[]
        def ck(name,ok):checks.append({'name':name,'pass':bool(ok)})
        ck('latest user baseline hash',hashlib.sha256(saved.read_bytes()).hexdigest()==repair['baseline_pcb_sha256'])
        ck('all latest user footprint references retained',set(prior)==set(current))
        for ref in prior:ck(ref+' only association metadata may change',ref in current and no_path(prior[ref])==no_path(current[ref]))
        ck('all other board nodes unchanged',[x for x in old if not isinstance(x,list) or x[0]!='footprint']==[x for x in now if not isinstance(x,list) or x[0]!='footprint'])
        record=json.loads((baseline/'baseline.json').read_text(encoding='utf8'))
        observed=[]
        for f,want in record['files'].items():
            if f.endswith(('.kicad_sch','.kicad_pro','.kicad_prl','.kicad_sym')) or f.startswith('.history/') or f.endswith('-lib-table'):
                actual=hashlib.sha256((ROOT/f).read_bytes()).hexdigest()
                if actual!=want:observed.append({'file':f,'baseline_sha256':want,'current_sha256':actual})
        # Other files can be saved in the user's open editor while we work.
        # Never roll those back or call them our PCB edit: use a fresh, stable
        # native export above and report the observed source changes separately.
        result['other_source_saves_observed_since_baseline']=observed
        result['preservation_checks']=checks;result['preservation_pass']=all(c['pass'] for c in checks)
        result['retained_segments']=len(nodes(now,'segment'));result['retained_vias']=len(nodes(now,'via'))
        result['pass']=result['pass'] and result['preservation_pass']
        drc_path=ROOT/'docs/pcb-drc-2026-11-08-f8.json'
        if drc_path.exists():
            drc=json.loads(drc_path.read_text(encoding='utf8'));result['native_drc']={'date':drc['date'],'kicad_version':drc['kicad_version'],'schematic_parity':len(drc['schematic_parity']),'violations':len(drc['violations']),'unconnected_items':len(drc['unconnected_items'])}
            result['pass']=result['pass'] and not drc['schematic_parity']
            result['routing_pass']=not drc['violations'] and not drc['unconnected_items']
    args.output.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print(json.dumps({k:v for k,v in result.items() if k not in ['matches','preservation_checks','fresh_export_source_sha256','other_source_saves_observed_since_baseline']},ensure_ascii=False,indent=2));return 0 if result['pass'] else 1
if __name__=='__main__':raise SystemExit(main())
