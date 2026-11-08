"""Audit the reviewed Motor pin plan without writing either PCB."""
from pathlib import Path
import sys,json,csv,re,hashlib,subprocess
ROOT=Path(__file__).resolve().parents[1];DOC=ROOT/'docs'
sys.path.insert(0,str(ROOT.parent/'Shared/design_tools'))
from sexpr import parse,node,nodes,prop
from pin_functions import parse_alternate_functions
from verify_m27 import native
from simplified_contract import intended
from m28_contract import PRIMARY_PINOUTS
from verify_pcb_associations import audit
def main():
    record=json.loads((DOC/'layout-pinplan.json').read_text(encoding='utf8'));plan=record['plan'];checks=[]
    def ck(name,ok,detail=None):checks.append({'name':name,'pass':bool(ok),'detail':detail})
    def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
    def normalized_sha(p):return hashlib.sha256(p.read_bytes().replace(b'\r\n',b'\n')).hexdigest()
    _,before,_=native(DOC/'baseline-2026-11-07-m28/live-netlist.xml')
    detach={'J14:'+r[c] for r in plan for c in ['contact','old_contact']}
    expected={frozenset(g-detach) for g in intended(before)};expected.discard(frozenset())
    for r in plan:
        found=[g for g in expected if r['endpoint'] in g]
        if len(found)!=1:raise ValueError('Missing/repeated intent endpoint '+r['endpoint'])
        expected.remove(found[0]);expected.add(found[0]|{'J14:'+r['contact']})
    expected.update(frozenset([p]) for p in detach-{'J14:'+r['contact'] for r in plan})
    tree,observed,pins=native(DOC/'netlist.xml')
    ck('full native net partition matches historical circuit plus explicit approved changes',observed==expected,{'missing':[sorted(g) for g in expected-observed],'unexpected':[sorted(g) for g in observed-expected]})
    components={z.get('ref'):z for z in tree.findall('.//components/comp')}
    ck('142 physical references retained',len(components)==142);ck('184 nets retained',len(observed)==184)
    for filename in ['BOM.csv','BOM_M2_SCHEMATIC_DRAFT.csv']:
        bom=list(csv.DictReader((DOC/filename).open(encoding='utf-8-sig')))
        ck(filename+' exact 140 electrical references',len(bom)==140 and {r['reference'] for r in bom}==set(components)-{'H1','H2'})
    for f,want in record['core_schematic_hashes'].items():
        p=ROOT.parent/'CoralC5-Core'/f
        ck('Core schematic unchanged except permitted Git EOL '+f,sha(p)==want or normalized_sha(p)==record['core_schematic_normalized_sha256'][f])
    p=ROOT/'CoralC5-Motor.kicad_pcb'
    ck('Motor PCB unchanged except permitted Git EOL',sha(p)==record['pcb_sha256'] or normalized_sha(p)==record['pcb_normalized_sha256'])
    _,_,corepins=native(DOC/'core_netlist.xml')
    contract={r['contact']:r for r in csv.DictReader((ROOT.parent/'Shared/interface.csv').open(encoding='utf-8-sig'))}
    ds=(ROOT.parent/'Shared/sources/DS15137_STM32C5Axxx_Datasheet_Rev3.pdf.txt').read_text(encoding='utf8');af=parse_alternate_functions(ds)
    ck('98 unique fixed contacts',len(contract)==98)
    ck('39 distinct GPIOs and contacts',len(plan)==39 and len({r['gpio'] for r in plan})==39 and len({r['contact'] for r in plan})==39)
    ck('USB SWD VBUS BOOT and Core user pins reserved',not {'PA9','PA11','PA12','PA13','PA14','PH2','PE3','PD2'}&{r['gpio'] for r in plan})
    table=list(csv.DictReader((DOC/'core_pin_assignment.csv').open(encoding='utf-8-sig')))+list(csv.DictReader((DOC/'m2_pin_assignment.csv').open(encoding='utf-8-sig')))
    ck('canonical allocation tables match 39 roles',len(table)==39 and {t['signal'] for t in table}=={r['signal'] for r in plan} and all(all(str(t[k])==('' if r[k] is None else str(r[k])) for k in ['gpio','contact','function','af','endpoint']) for r in plan for t in table if t['signal']==r['signal']))
    schemas={}
    def visit(f):
        if f in schemas:return
        d=parse((ROOT/f).read_text(encoding='utf8'));schemas[f]=d
        for sh in nodes(d,'sheet'):visit(prop(sh,'Sheetfile')[2])
    visit('CoralC5-Motor.kicad_sch')
    ck('ten active hierarchy pages',len(schemas)==10 and {p.name for p in ROOT.glob('*.kicad_sch')}==set(schemas))
    socket=next(z for z in nodes(node(schemas['03_Core_Interface.kicad_sch'],'lib_symbols'),'symbol') if z[1].endswith('CoralC5_Streamline_Socket'))
    names={node(p,'number')[1]:node(p,'name')[1] for z in nodes(socket,'symbol') for p in nodes(z,'pin')}
    role={r['contact']:r for r in plan}
    for c,r in contract.items():
        cn=corepins.get('J13:'+c,set());mn=pins.get('J14:'+c,set())
        ck(c+' present in Core and Motor','J13:'+c in corepins and 'J14:'+c in pins)
        if r['gpio']:ck(c+' fixed GPIO name',names[c].endswith('/_'+r['gpio']))
        if r['mcu_pin']:ck(c+' actual Core MCU pad','U1:'+r['mcu_pin'] in cn)
        if r['net'] in ['GND','+3V3','+5V_SYS']:
            contacts=[q for q in contract if contract[q]['net']==r['net']]
            ck(c+' supply contact grouping',all('J13:'+q in cn and 'J14:'+q in mn for q in contacts))
        if c in role:ck(c+' correct module endpoint',role[c]['endpoint'] in mn)
        elif r['gpio'] and c not in ['A46','B43']:ck(c+' unused GPIO NC',mn=={'J14:'+c})
    for r in plan:
        ck(r['signal']+' contact mapping',contract[r['contact']]['gpio']==r['gpio'])
        if r['af'] is not None:ck(r['signal']+' DS15137 Table14 AF',af.get((r['gpio'],r['function']))==r['af'])
        if r['function'].startswith('ADC'):ck(r['signal']+' Table13 ADC',any(re.search(r'\b'+r['gpio']+r'\b[^\n]*\bI/O\b',b) and r['function'] in b for b in ds.split('\n\n')))
    bs={r['signal']:r for r in plan};timers=[]
    for side in ['L','R']:
        matches=[re.fullmatch(r'TIM(\d+)_CH([12])',bs[side+'_ENC_'+ab]['function']) for ab in ['A','B']]
        ck(side+' encoder uses CH1 and CH2 of one timer',all(matches) and len({m[1] for m in matches})==1 and {m[2] for m in matches}=={'1','2'});timers.append(matches[0][1])
    pwms=[re.fullmatch(r'TIM(\d+)_CH([1-4])',bs[side+'_PWM']['function']) for side in ['L','R']]
    ck('wheel PWM TIM4 separate channels',[m[1] for m in pwms]==['4','4'] and len({m[2] for m in pwms})==2)
    backlight=re.fullmatch(r'TIM(\d+)_CH([1-4])',bs['TFT_BL']['function'])
    ck('encoders wheel PWM and backlight do not share counters',len(set(timers+[pwms[0][1],backlight[1]]))==4)
    usarts=[]
    for name,tx,rx in [('BT','BT_TX','BT_RX'),('Maix','VIDEO_TX','VIDEO_RX'),('stepper','STEP_UART_TX','STEP_UART_RX'),('Lichee','LICHEE_TX','LICHEE_RX')]:
        a=bs[tx]['function'];b=bs[rx]['function'];ck(name+' UART TX RX same instance',a.endswith('_TX') and b==a[:-2]+'RX');usarts.append(a.split('_')[0])
    ck('four independent UART instances',len(set(usarts))==4)
    ck('gray single-port PE7 through PE14',[bs['GRAY_CH'+str(i)]['gpio'] for i in range(1,9)]==['PE'+str(i) for i in range(7,15)])
    ck('module signals use corresponding socket side',not record['metrics']['newCross'])
    refs={}
    for f,d in schemas.items():
        uuids=[node(z,'uuid')[1] for z in d if isinstance(z,list) and node(z,'uuid')]
        ck(f+' no duplicate top-level UUIDs',len(uuids)==len(set(uuids)))
        for t in nodes(d,'text'):ck(f+' 100mil note',list(map(float,node(node(node(t,'effects'),'font'),'size')[1:]))==[2.54,2.54])
        for sh in nodes(d,'sheet'):ck(prop(sh,'Sheetfile')[2]+' hierarchy port names',sorted(p[1] for p in nodes(sh,'pin'))==sorted(p[1] for p in nodes(schemas[prop(sh,'Sheetfile')[2]],'hierarchical_label')))
        for z in nodes(d,'symbol'):
            ref=prop(z,'Reference')[2]
            if ref.startswith('#'):continue
            item=refs.setdefault(ref,{'fp':prop(z,'Footprint')[2],'pins':set()});item['pins'].update(p[1] for p in nodes(z,'pin'))
    resources=[];unbound=[];prefix='$'+'{KIPRJMOD}/'
    for ref,item in refs.items():
        fp=item['fp'];path=ROOT/'Motor.pretty'/(fp.split(':')[-1]+'.kicad_mod')
        ck(ref+' local footprint exists',fp.startswith('Motor:') and path.is_file())
        if not path.exists():continue
        land=parse(path.read_text(encoding='utf8'));pads={p[1] for p in nodes(land,'pad') if p[1]}
        ck(ref+' numbered symbol pads match',item['pins']==pads)
        models=[p[1] for p in nodes(land,'model')]
        if not models and ref not in ['H1','H2']:unbound.append(ref)
        for p in models:ck(ref+' portable existing model',p.startswith(prefix+'Models/') and (ROOT/p.replace(prefix,'')).is_file())
        resources.append({'reference':ref,'footprint':fp,'models':models})
    ck('only existing XT60 exact model missing',unbound==['J1'],unbound)
    for ref,kind in [('U1','AP64500'),('U6','AP64500'),('U8','TPS25810'),('U16','TPS25810'),('U11','SN74LV1T125'),('U12','SN74LVC1G125'),('J18','Lichee_header')]:
        d,z=next((d,z) for d in schemas.values() for z in nodes(d,'symbol') if prop(z,'Reference')[2]==ref)
        lib=next(q for q in nodes(node(d,'lib_symbols'),'symbol') if q[1]==node(z,'lib_id')[1])
        found={node(p,'number')[1]:node(p,'name')[1] for sub in nodes(lib,'symbol') for p in nodes(sub,'pin')}
        ck(ref+' manufacturer package pin identities',found==PRIMARY_PINOUTS[kind])
    assoc=audit(ROOT/'CoralC5-Motor.kicad_pcb',DOC/'netlist.xml')
    ck('all Motor UUID associations retained',not any(assoc[k] for k in ['predicted_additions_by_uuid','ambiguous_symbol_matches','unmatched_pcb_footprints','reference_mismatches','predicted_footprint_type_replacements','value_mismatches']))
    erc=json.loads((DOC/'erc.json').read_text(encoding='utf8'));viol=[v for s in erc['sheets'] for v in s['violations']]
    ck('native ERC zero errors and warnings',not viol,viol)
    project=json.loads((ROOT/'CoralC5-Motor.kicad_pro').read_text(encoding='utf8'));ck('no ERC exclusions',not project.get('erc',{}).get('erc_exclusions'))
    visual=json.loads((DOC/'visual_check.json').read_text(encoding='utf8'))
    ck('all ten pages rendered and no wire/text overlaps',visual['pdf_sha256']==sha(DOC/'CoralC5-Motor_Schematic.pdf') and visual['pages']==10 and not visual['wire_text_intersections'] and not visual['text_text_overlaps'])
    report={'date':record['date'],'revision':record['revision'],'source_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT).decode().strip(),'kicad_version':erc['kicad_version'],'pass':all(c['pass'] for c in checks),'checks':len(checks),'failed':[c for c in checks if not c['pass']],'physical_references':len(refs),'native_nets':len(observed),'gpio_roles':len(plan),'changed_gpio_roles':sum(r['contact']!=r['old_contact'] for r in plan),'native_erc_errors':sum(v['severity']=='error' for v in viol),'native_erc_warnings':sum(v['severity']=='warning' for v in viol),'model_bound_references':sum(bool(r['models']) for r in resources),'model_missing_references':unbound,'artifacts_sha256':{f:sha(ROOT/f) for f in schemas},'results':checks,'resources':resources,'motor_pcb_update_pending':True,'pcb_drc_run':False,'limitations':['Motor PCB is byte-identical; remapped GPIO pad nets intentionally await a separate update.','MaixCAM2 ~1A typical limit versus inference/charging peaks is unmeasured; independent reverse blocking and UART isolation absent after approved simplification.','12V 12.5A source estimate ~11.09A at85% efficiency, with assumed4A wheels and3A stepper, leaves limited margin; load/startup/thermal tests required.','Stepper VM and UART are direct; current simplified circuit has no independent MCU power-off.','Core external3.3V load budget<=100mA and encoder3.3V operation need hardware confirmation.','XT60 exact model, generic-model dimensions, connector mating and button lead geometry remain unverified.','Firmware peripheral setup not implemented.']}
    (DOC/'verification.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print(json.dumps({k:v for k,v in report.items() if k not in ['results','resources','artifacts_sha256']},ensure_ascii=False,indent=2))
    return 0 if report['pass'] else 1
if __name__=='__main__':raise SystemExit(main())

