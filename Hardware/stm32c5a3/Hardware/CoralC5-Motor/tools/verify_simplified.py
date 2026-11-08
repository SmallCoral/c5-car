"""Audit the current simplified design; historical revision checks stay intact.

Electrical intent is an immutable older native export + explicit reviewed edits
in simplified_contract. Preservation uses this task's saved live-user baseline.
PCB association and object-preservation checks are read-only and separate.
"""
from pathlib import Path
import sys,json,csv,hashlib,subprocess,xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parents[1];DOC=ROOT/'docs';OUT=DOC/'finalize-2026-11-08';BASE=OUT/'baseline'
sys.path.insert(0,str(ROOT.parent/'Shared/design_tools'))
from sexpr import parse,node,nodes,prop
from schematic_semantic import canonical
from pin_functions import parse_alternate_functions
from verify_m27 import native
from simplified_contract import intended,REMOVED
from m28_contract import PRIMARY_PINOUTS
from verify_pcb_associations import audit

def main():
    checks=[]
    def ck(name,ok,detail=None):checks.append(dict(name=name,pass_=bool(ok),detail=detail))
    def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
    record=json.loads((OUT/'current-revision.json').read_text(encoding='utf8'))
    manifest=json.loads((BASE/'baseline.json').read_text(encoding='utf8'))
    for f,want in manifest['files'].items():ck('immutable task baseline '+f,sha(BASE/f)==want)
    pages=[];schemas={}
    def visit(f):
        if f in schemas:return
        pages.append(f);schemas[f]=parse((ROOT/f).read_text(encoding='utf-8-sig'))
        for sh in nodes(schemas[f],'sheet'):visit(prop(sh,'Sheetfile')[2])
    visit('CoralC5-Motor.kicad_sch')
    ck('only ten active schematic files, no scratch projects',len(pages)==10 and {p.name for p in ROOT.glob('*.kicad_sch')}==set(pages))
    current={(f,prop(z,'Reference')[2],node(z,'unit')[1]):z for f,d in schemas.items() for z in nodes(d,'symbol') if not prop(z,'Reference')[2].startswith('#')}
    prior={(f,prop(z,'Reference')[2],node(z,'unit')[1]):z for f in pages for z in nodes(parse((BASE/f).read_text(encoding='utf-8-sig')),'symbol') if not prop(z,'Reference')[2].startswith('#')}
    ck('only unused C76 removed from current physical units',set(current)=={k for k in prior if k[1]!='C76'})
    changed_poses={'U10','J19','C72','C78','C62','R72','J15','J23'}
    for key,z in current.items():
        b=prior[key];ref=key[1]
        for tag in ['uuid','unit','lib_id','pin','instances','in_bom','on_board','dnp']:ck(ref+' preserved '+tag,nodes(z,tag)==nodes(b,tag))
        ck(ref+' properties and manufacturing references preserved',{p[1]:p[2] for p in nodes(z,'property')}=={p[1]:p[2] for p in nodes(b,'property')})
        if ref not in changed_poses:ck(ref+' unchanged schematic pose',node(z,'at')==node(b,'at'))
    for f,d in schemas.items():
        ck(f+' paper',node(d,'paper')[1]==record['paper'][f])
        for t in nodes(d,'text'):ck(f+' Chinese note 100mil',list(map(float,node(node(node(t,'effects'),'font'),'size')[1:]))==[2.54,2.54])
        parent=next((sh for pd in schemas.values() for sh in nodes(pd,'sheet') if prop(sh,'Sheetfile')[2]==f),None)
        if parent:ck(f+' hierarchy pin names match',sorted(p[1] for p in nodes(parent,'pin'))==sorted(p[1] for p in nodes(d,'hierarchical_label')))
    source=parse((ROOT/'Motor.kicad_sym').read_text(encoding='utf8'));oldsource=parse((BASE/'Motor.kicad_sym').read_text(encoding='utf8'))
    definitions={z[1]:z for z in nodes(source,'symbol')};olddefs={z[1]:z for z in nodes(oldsource,'symbol')}
    changed_defs={'SGM2531C_PS8','SN74LV1T125_DBV_M28_RX','SN74LVC1G125_DBV_M28'}
    ck('source library definition set preserved',set(definitions)==set(olddefs))
    for k in definitions:
        if k not in changed_defs:ck(k+' unused and other source definitions preserved',definitions[k]==olddefs[k])
    for f,ref,_ in current:
        z=current[(f,ref,_)];key=node(z,'lib_id')[1];short=key.split(':')[-1]
        if short in changed_defs:
            cached=next(s for s in nodes(node(schemas[f],'lib_symbols'),'symbol') if s[1]==key)
            want=definitions[short].copy();want[1]=key
            ck(ref+' local source matches retained user geometry',canonical(want)==canonical(cached))
    _,old,_=native(DOC/'baseline-2026-11-07-m28/live-netlist.xml')
    tree,actual,pins=native(DOC/'netlist.xml');expected=intended(old)
    ck('all 184 nets match independent simplified intent',actual==expected and len(actual)==184,dict(missing=[sorted(g) for g in expected-actual],extra=[sorted(g) for g in actual-expected]))
    _,before,bp=native(BASE/'netlist.xml')
    dropped={frozenset(p for p in g if not p.startswith('C76:')) for g in before};dropped.discard(frozenset())
    ck('only current-baseline electrical change is removal of C76',dropped==actual)
    components={c.get('ref'):c for c in tree.findall('.//components/comp')}
    ck('142 entities exported exactly',set(components)=={k[1] for k in current} and len(components)==142)
    ck('approved deleted parts absent',not set(components)&REMOVED)
    _,_,core=native(DOC/'core_netlist.xml')
    contacts=list(csv.DictReader((ROOT.parent/'Shared/interface.csv').open(encoding='utf-8-sig')))
    assignments=json.loads((DOC/'baseline-2026-11-06-m2/design_manifest.json').read_text(encoding='utf-8-sig'))['motor_pin_assignment']['entries']+list(csv.DictReader((DOC/'m2_pin_assignment.csv').open(encoding='utf-8-sig')))
    functions=parse_alternate_functions((ROOT.parent/'Shared/sources/DS15137_STM32C5Axxx_Datasheet_Rev3.pdf.txt').read_text(encoding='utf-8-sig'))
    ck('98 unique Core contacts',len(contacts)==98 and len({r['contact'] for r in contacts})==98)
    ck('39 distinct assigned GPIOs',len(assignments)==39 and len({r['gpio'] for r in assignments})==39)
    ck('reserved pins remain free',not {'PA9','PA11','PA12','PA13','PA14','PH2','PE3','PD2','PD12'}&{r['gpio'] for r in assignments})
    socket=next(s for s in nodes(node(schemas['03_Core_Interface.kicad_sch'],'lib_symbols'),'symbol') if s[1].endswith('CoralC5_Streamline_Socket'))
    names={node(p,'number')[1]:node(p,'name')[1] for s in nodes(socket,'symbol') for p in nodes(s,'pin')};rows=[]
    for r in contacts:
        c=r['contact'];cn=core.get('J13:'+c,set());mn=pins.get('J14:'+c,set());a=next((a for a in assignments if a['contact']==c),None)
        ck(c+' matching Core socket',c in names and 'J14:'+c in pins)
        if r['mcu_pin']:ck(c+' MCU native contact','U1:'+r['mcu_pin'] in cn)
        if r['gpio']:ck(c+' same GPIO identity',names[c].endswith('/_'+r['gpio']))
        if r['net'] in ['GND','+3V3','+5V_SYS']:
            same=[q['contact'] for q in contacts if q['net']==r['net']]
            ck(c+' same-voltage power contacts',all('J13:'+q in cn and 'J14:'+q in mn for q in same))
        if a:ck(c+' assigned endpoint',a['endpoint'] in mn)
        elif r['gpio'] and c not in ['A46','B43']:ck(c+' unused GPIO NC',mn=={'J14:'+c})
        rows.append(dict(contact=c,gpio=r['gpio'],mcu_pin=r['mcu_pin'],core_native_pins=' '.join(sorted(cn)),motor_native_pins=' '.join(sorted(mn)),role=a['signal'] if a else ('connected' if len(mn)>1 else 'unused NC')))
    for a in assignments:
        if a.get('af') not in [None,'','None']:ck(a['signal']+' verified alternate function',functions.get((a['gpio'],a['function']))==int(a['af']))
    def group(anchor,text):ck(anchor+' exact interface net',pins.get(anchor,set())==set(text.split()))
    group('J19:2','U10:5 C62:1 J19:2');group('J19:5','J19:5 J14:B35');group('J19:6','J19:6 J14:B34')
    group('J23:7','J23:7 J14:A30');group('J23:8','J23:8 J14:A29');group('J14:B30','J14:B30')
    group('J14:A48','D3:1 J14:A48 J14:B48 R5:1')
    for j,unused in [('J23',['3','4','5','6']),('J19',['3','4']),('J18',['1','2','5','6','7','8','9','10'])]:
        for no in unused:group(j+':'+no,j+':'+no)
    ck('stepper gets main VM',{'U2:11','U4:11','U1:2','J23:1'}<=pins['J23:1'])
    ck('Maix simple protection enabled correctly',{'U10:3','U10:4','C72:1','C78:1','L2:2'}<=pins['U10:4'] and 'U10:8' in pins['U10:1'])
    ck('Lichee translators always enabled and powered from Core',{'U11:1','U12:1'}<=pins['J14:A1'] and {'U11:5','U12:5'}<=pins['J14:A47'])
    ck('5V A and B remain independent',pins['L1:2']!=pins['L2:2'])
    ck('Core and peripheral 3V3 not paralleled',pins['U7:5']!=pins['J14:A47'])
    ck('Core USB supply isolation retained',{'D2:1','U2:2','U2:3','J13:A48','J13:B48'}<=core['J13:A48'])
    resources=[];unbound=[];gap_count=0
    for (f,ref,unit),z in current.items():
        if unit!='1':continue
        fp=prop(z,'Footprint')[2];path=ROOT/'Motor.pretty'/(fp.split(':')[-1]+'.kicad_mod')
        ck(ref+' local footprint bound',fp.startswith('Motor:') and path.is_file())
        if not path.exists():continue
        land=parse(path.read_text(encoding='utf8'));pads={p[1] for p in nodes(land,'pad') if p[1]};allpins={p[1] for (ff,rr,uu),s in current.items() if rr==ref for p in nodes(s,'pin')}
        ck(ref+' complete symbol/pad mapping',pads==allpins,dict(pads=sorted(pads),symbol=sorted(allpins)))
        models=[m[1] for m in nodes(land,'model')]
        if not models and ref not in ['H1','H2']:unbound.append(ref)
        for m in models:ck(ref+' portable existing model',m.startswith('${KIPRJMOD}/Models/') and (ROOT/m.replace('${KIPRJMOD}/','')).is_file())
        if 'Gap0.85mm' in fp:
            pp={p[1]:p for p in nodes(land,'pad')};gap=abs(float(node(pp['2'],'at')[1])-float(node(pp['1'],'at')[1]))-(float(node(pp['1'],'size')[1])+float(node(pp['2'],'size')[1]))/2
            ck(ref+' retained 0.85mm passive gap',abs(gap-.85)<1e-8);gap_count+=1
        resources.append(dict(reference=ref,footprint=fp,models=models))
    ck('only existing XT60 exact-model gap remains',unbound==['J1'],unbound)
    ck('TFT two mechanical holes retained',all(r in components for r in ['H1','H2']))
    for ref,kind in [('U1','AP64500'),('U6','AP64500'),('U8','TPS25810'),('U16','TPS25810'),('U11','SN74LV1T125'),('U12','SN74LVC1G125'),('J18','Lichee_header')]:
        f=next(f for f,r,u in current if r==ref);z=next(z for (ff,r,u),z in current.items() if r==ref)
        lib=next(s for s in nodes(node(schemas[f],'lib_symbols'),'symbol') if s[1]==node(z,'lib_id')[1]);found={node(p,'number')[1]:node(p,'name')[1] for s in nodes(lib,'symbol') for p in nodes(s,'pin')}
        ck(ref+' primary pin identities',found==PRIMARY_PINOUTS[kind])
    erc=json.loads((DOC/'erc.json').read_text(encoding='utf8'));viol=[v for s in erc['sheets'] for v in s['violations']]
    ck('native ERC zero errors and warnings',not viol,viol)
    project=json.loads((ROOT/'CoralC5-Motor.kicad_pro').read_text(encoding='utf8'));oldproject=json.loads((BASE/'CoralC5-Motor.kicad_pro').read_text(encoding='utf8'))
    ck('ERC settings unchanged, no new exclusions',project.get('erc')==oldproject.get('erc') and not project.get('erc',{}).get('erc_exclusions'))
    visual=json.loads((DOC/'visual_check.json').read_text(encoding='utf8'))
    ck('PDF visual check current and no intersections',visual['pdf_sha256']==sha(DOC/'CoralC5-Motor_Schematic.pdf') and visual['pages']==10 and not visual['wire_text_intersections'] and not visual['text_text_overlaps'])
    assoc=audit(ROOT/'CoralC5-Motor.kicad_pcb',DOC/'netlist.xml');(OUT/'pcb-associations.json').write_text(json.dumps(assoc,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    ck('read-only PCB UUID and pad-network consistency',assoc['pass'])
    br=json.loads((OUT/'pcb-minimal-sync.json').read_text(encoding='utf8'));oldboard=parse((ROOT/br['baseline_file']).read_text(encoding='utf8'));board=parse((ROOT/'CoralC5-Motor.kicad_pcb').read_text(encoding='utf8'))
    without=[x for x in oldboard if not (isinstance(x,list) and x[0]=='footprint' and prop(x,'Reference')[2]=='C76')]
    ck('PCB only edit is removing unrouted C76; all other objects exact',without==board)
    for file in ['BOM.csv','BOM_M2_SCHEMATIC_DRAFT.csv']:
        bom=list(csv.DictReader((DOC/file).open(encoding='utf-8-sig')))
        ck(file+' 140 electrical references, no stale parts',{r['reference'] for r in bom}==set(components)-{'H1','H2'} and len(bom)==140)
    for c in checks:c['pass']=c.pop('pass_')
    report=dict(date='2026-11-08',revision='M2.8S',source_commit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT).decode().strip(),kicad_version=erc['kicad_version'],checks=len(checks),failed=[c for c in checks if not c['pass']],pages=len(pages),native_nets=len(actual),physical_references=len(resources),gpio_roles=len(assignments),passive_0603_count=gap_count,native_erc_errors=sum(v['severity']=='error' for v in viol),native_erc_warnings=sum(v['severity']=='warning' for v in viol),model_bound_references=sum(bool(r['models']) for r in resources),unbound_models=unbound,generic_model_references=['F1','SW1','SW2','SW3','SW4','SW5','U1','U6','U10','J18','J19'],resources=resources,results=checks,artifacts_sha256={f:sha(ROOT/f) for f in pages},limitations=['Electrical/interface intent and schematic resources audited; no claim of measured board current, startup stability or thermal capability.','Maix nominal limit ~0.9986A is typical, not a guaranteed 1A upper bound; recognition plus charging must be measured.','Independent reverse blocking and UART power isolation were removed by the approved simplification. Use one external Maix power source and coordinate module power/UART sequencing.','12V total input estimate ~11.09A assumes 85% conversion efficiency; 15A fuse at 75% continuous derating leaves little margin.','PCB tracks/outline/DRC and physical tests remain separate; no routing or DRC pass claimed.','J1 exact 3D model, button lead geometry, TFT hole pitch and complete procurement/long-term stock audit remain unresolved.'])
    report['pass']=not report['failed'];(DOC/'verification.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8');(OUT/'verification.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    with (DOC/'core_motor_98_contact_audit.csv').open('w',encoding='utf-8-sig',newline='') as out:
        w=csv.DictWriter(out,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
    print(json.dumps({k:v for k,v in report.items() if k not in ['results','resources','artifacts_sha256']},ensure_ascii=False,indent=2))
    return 0 if report['pass'] else 1

if __name__=='__main__':raise SystemExit(main())
