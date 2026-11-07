"""M2.8 independent electrical, identity, resource and 98-contact audit.

Historical verifiers are retained. Input intent is the old M2.7 native netlist
plus static reviewed edits in m28_contract, never the current observed nets.
"""
from pathlib import Path
from datetime import datetime
import sys,json,csv,hashlib,subprocess,xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parents[1];DOC=ROOT/'docs'
sys.path.insert(0,str(ROOT.parent/'Shared/design_tools'))
from sexpr import parse,node,nodes,prop
from schematic_semantic import canonical
from pin_functions import parse_alternate_functions
from verify_m27 import native
from m28_contract import intended,PRIMARY_PINOUTS,ADDED_PHYSICAL,PIN_MAP,REMOVED_PINS

def main():
    record=json.loads((DOC/'m28_revision.json').read_text(encoding='utf8'))
    base=DOC/record['resume_baseline'];baseline=json.loads((base/'baseline.json').read_text(encoding='utf8'))
    checks=[]
    def ck(name,ok,detail=None):checks.append({'name':name,'pass':bool(ok),'detail':detail})
    def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
    _,before,_=native(DOC/'baseline-2026-11-07-m28/live-netlist.xml')
    tree,observed,pins=native(DOC/'netlist.xml');expected=intended(before)
    ck('all205 native nets equal independent M2.8 intent',observed==expected and len(expected)==205,{'missing':[sorted(g) for g in expected-observed],'extra':[sorted(g) for g in observed-expected]})
    schemas={p.name:parse(p.read_text(encoding='utf8')) for p in ROOT.glob('*.kicad_sch')}
    oldschemas={f:parse((base/f).read_text(encoding='utf8')) for f in schemas}
    current={prop(z,'Reference')[2]:(f,z) for f,d in schemas.items() for z in nodes(d,'symbol')}
    oldrefs={prop(z,'Reference')[2]:(f,z) for f,d in oldschemas.items() for z in nodes(d,'symbol')}
    ck('exactly ten hierarchical files',len(schemas)==10)
    ck('resume reference set retained',set(current)==set(oldrefs))
    for f,d in schemas.items():
        ck(f+' baseline hash verified',sha(base/f)==baseline['files'][f]['sha256'])
        ck(f+' declared paper',node(d,'paper')[1]==record['paper'][f])
        for t in nodes(d,'text'):ck(f+' 100mil note',list(map(float,node(node(node(t,'effects'),'font'),'size')[1:]))==[2.54,2.54])
        prior={(prop(z,'Reference')[2],node(z,'unit')[1]):z for z in nodes(oldschemas[f],'symbol')}
        now={(prop(z,'Reference')[2],node(z,'unit')[1]):z for z in nodes(d,'symbol')}
        ck(f+' all units uniquely retained',set(prior)==set(now) and len(now)==len(nodes(d,'symbol')))
        for key,z in now.items():
            b=prior[key];ref=key[0]
            for tag in ['uuid','at','unit','lib_id','pin']:
                ck(str(key)+' preserved '+tag,nodes(z,tag)==nodes(b,tag))
            fields={p[1]:p[2] for p in nodes(b,'property')};fields.update(record['resume_property_edits'].get(ref,{}))
            ck(str(key)+' exact reviewed properties',{p[1]:p[2] for p in nodes(z,'property')}==fields)
        prev={x[1]:x for x in nodes(node(oldschemas[f],'lib_symbols'),'symbol')}
        defs={x[1]:x for x in nodes(node(d,'lib_symbols'),'symbol')}
        ck(f+' cached definitions retained',prev==defs)
    # Preserve M2.7 physical identities through the explicit AP64500 pin map.
    original=DOC/'baseline-2026-11-07-m28'
    # This historical snapshot was committed with LF conversion; its manifest
    # hashes were created from CRLF files. Record that distinction explicitly.
    historical=json.loads((original/'baseline.json').read_text(encoding='utf8'))
    historical_normalized=[]
    for f,want in historical['sha256'].items():
        raw=(original/f).read_bytes();rawhash=hashlib.sha256(raw).hexdigest()
        crlfhash=hashlib.sha256(raw.replace(b'\r\n',b'\n').replace(b'\n',b'\r\n')).hexdigest()
        ck('M2.7 original baseline '+f+' hash or documented CRLF representation',want in [rawhash,crlfhash],{'raw_sha256':rawhash,'manifest_sha256':want,'crlf_sha256':crlfhash})
        if rawhash!=want and crlfhash==want:historical_normalized.append(f)
    ck('fixed original live netlist input SHA',sha(original/'live-netlist.xml')=='e1db7f380c513fa5e9d43b6246c839e0f813e2c6792fba6eeb358e5e610f57b9')
    priorphysical={prop(z,'Reference')[2]:z for p in original.glob('*.kicad_sch') for z in nodes(parse(p.read_text(encoding='utf8')),'symbol') if not prop(z,'Reference')[2].startswith('#')}
    physical={r for r in current if not r.startswith('#')}
    ck('33 physical additions exactly specified',physical-set(priorphysical)==ADDED_PHYSICAL and set(priorphysical)<=physical)
    for ref,z in priorphysical.items():
        now=current[ref][1];ck(ref+' M2.7 symbol UUID retained',node(z,'uuid')==node(now,'uuid'))
        np={p[1]:node(p,'uuid')[1] for p in nodes(now,'pin')}
        for p in nodes(z,'pin'):
            key=ref+':'+p[1]
            if key in REMOVED_PINS:continue
            target=PIN_MAP.get(key,key).split(':')[1]
            ck(key+' prior pin UUID retained',np.get(target)==node(p,'uuid')[1])
    ck('local symbol library byte preserved since resume',sha(ROOT/'Motor.kicad_sym')==baseline['files']['Motor.kicad_sym']['sha256'])
    # 98-contact power and GPIO contract against a fresh Core native export.
    _,_,core=native(DOC/'core_netlist.xml')
    contract=list(csv.DictReader((ROOT.parent/'Shared/interface.csv').open(encoding='utf-8-sig')))
    ck('98 unique Core contacts',len(contract)==98 and len({r['contact'] for r in contract})==98)
    motorlib=next(z for z in nodes(node(schemas['03_Core_Interface.kicad_sch'],'lib_symbols'),'symbol') if z[1].endswith('CoralC5_Streamline_Socket'))
    socket={node(p,'number')[1]:node(p,'name')[1] for s in nodes(motorlib,'symbol') for p in nodes(s,'pin')}
    assignments=json.loads((DOC/'baseline-2026-11-06-m2/design_manifest.json').read_text(encoding='utf-8-sig'))['motor_pin_assignment']['entries']+list(csv.DictReader((DOC/'m2_pin_assignment.csv').open(encoding='utf-8-sig')))
    functions=parse_alternate_functions((ROOT.parent/'Shared/sources/DS15137_STM32C5Axxx_Datasheet_Rev3.pdf.txt').read_text(encoding='utf-8-sig'))
    ck('40 distinct assigned GPIOs',len(assignments)==40 and len({r['gpio'] for r in assignments})==40)
    ck('reserved GPIOs unassigned',not {'PA9','PA11','PA12','PA13','PA14','PH2','PE3','PD2'}&{r['gpio'] for r in assignments})
    audit=[]
    for r in contract:
        c=r['contact'];cn=core.get('J13:'+c,set());mn=pins.get('J14:'+c,set());assigned=next((a for a in assignments if a['contact']==c),None)
        ck(c+' matching socket contact',c in socket and 'J14:'+c in pins)
        if r['mcu_pin']:ck(c+' Core MCU native pin','U1:'+r['mcu_pin'] in cn)
        if r['gpio']:ck(c+' named GPIO',socket[c].endswith('/_'+r['gpio']))
        if r['net'] in ['GND','+3V3','+5V_SYS']:
            same=[a['contact'] for a in contract if a['net']==r['net']]
            ck(c+' power contacts correct',all('J13:'+a in cn and 'J14:'+a in mn for a in same))
        if assigned:ck(c+' assigned endpoint',assigned['endpoint'] in mn)
        elif r['gpio'] and c not in ['A46','B43']:ck(c+' unused GPIO isolated',mn=={'J14:'+c})
        audit.append(dict(contact=c,gpio=r['gpio'],mcu_pin=r['mcu_pin'],core_native_pins=' '.join(sorted(cn)),motor_native_pins=' '.join(sorted(mn)),role=assigned['signal'] if assigned else ('connected' if len(mn)>1 else 'unused NC')))
    for a in assignments:
        af=a.get('af')
        if af not in [None,'','None']:ck(a['signal']+' datasheet alternate function',functions.get((a['gpio'],a['function']))==int(af))
    # The independent counters and PWM channels must not share timer instances.
    wheel={a['signal']:a for a in assignments if a['signal'].startswith(('L_','R_'))}
    ck('encoder timers independent of PWM and TFT timers',all(wheel[s]['function'].startswith(t) for s,t in [('L_ENC_A','TIM5_'),('L_ENC_B','TIM5_'),('R_ENC_A','TIM2_'),('R_ENC_B','TIM2_'),('L_PWM','TIM4_'),('R_PWM','TIM4_')]) and next(a for a in assignments if a['signal']=='TFT_BL')['function']=='TIM3_CH1')
    primary_text=(ROOT.parent/'Shared/sources/DS15137_STM32C5Axxx_Datasheet_Rev3.pdf.txt').read_text(encoding='utf-8-sig')
    import re
    for gpio,adc in [('PH5','ADC2_IN13'),('PC0','ADC1_IN8')]:ck(gpio+' primary ADC pin-table row',bool(re.search(r'\b'+gpio+r'\b[^\n]*\b'+adc+r'\b',primary_text)))
    def group(anchor,expected):ck(anchor+' exact net',pins.get(anchor,set())==set(expected.split()))
    group('J14:A48','D3:1 J14:A48 J14:B48 R5:1')
    ck('Core USB diode cathode on Core system rail',{'D2:1','U2:2','U2:3','J13:A48','J13:B48'}<=core['J13:A48'])
    ck('Core AP63203 output supplies MCU and both 3V3 contacts',{'U2:1','L1:2','J13:A47','J13:B47','U1:100'}<=core['J13:A47'])
    ck('raw 5V isolated from Core USB rail',pins['U7:1']!=pins['J14:A48'])
    ck('TFT/BT LDO separated from Core 3V3',pins['U7:5']!=pins['J14:A47'])
    group('J19:2','C62:1 J19:2 Q4:1 Q5:3 R77:1 U14:6')
    group('J19:5','J19:5 R78:1 U13:5');group('J19:6','J19:6 U13:6')
    group('U16:14','U16:14 U16:15 C83:1 J28:A9 J28:B9')
    group('U16:11','U16:11 J28:A5');group('U16:13','U16:13 J28:B5')
    group('J23:1','J23:1 Q7:2 R86:1');group('J14:B30','J14:B30 R83:1')
    for p in ['3','4','5','6']:group('J23:'+p,'J23:'+p)
    for p in ['3','4']:group('J19:'+p,'J19:'+p)
    resources=[];missing=[]
    for ref,(f,z) in current.items():
        if ref.startswith('#'):continue
        fp=prop(z,'Footprint')[2];path=ROOT/'Motor.pretty'/(fp.split(':')[-1]+'.kicad_mod')
        ck(ref+' local footprint exists',fp.startswith('Motor:') and path.is_file())
        if not path.is_file():continue
        part=parse(path.read_text(encoding='utf8'));pads={p[1] for p in nodes(part,'pad') if p[1]};symbolpins={p[1] for p in nodes(z,'pin')}
        ck(ref+' exact symbol/pad number set',pads==symbolpins,{'symbol':sorted(symbolpins),'pads':sorted(pads)})
        models=[m[1] for m in nodes(part,'model')];mechanical=ref in ['H1','H2']
        if not models and not mechanical:missing.append(ref)
        for m in models:ck(ref+' portable model exists',m.startswith('${KIPRJMOD}/Models/') and (ROOT/m.replace('${KIPRJMOD}/','')).is_file())
        resources.append(dict(reference=ref,footprint=fp,models=models,mechanical_hole=mechanical))
        if 'Gap0.85mm' in fp:
            pp={p[1]:p for p in nodes(part,'pad')};gap=abs(float(node(pp['2'],'at')[1])-float(node(pp['1'],'at')[1]))-(float(node(pp['1'],'size')[1])+float(node(pp['2'],'size')[1]))/2
            ck(ref+' largest selected 0603 gap',abs(gap-.85)<1e-8)
    ck('only exact XT60 model missing',missing==['J1'])
    def lands(name):
        f=parse((ROOT/'Motor.pretty'/(name+'.kicad_mod')).read_text(encoding='utf8'))
        return sorted((p[1],*map(float,node(p,'at')[1:3]),*map(float,node(p,'size')[1:3])) for p in nodes(f,'pad'))
    rpa=[('1',-1.4,-.75,.6,.25),('2',-1.4,-.25,.6,.25),('3',-1.4,.25,.6,.25),('4',-1.4,.75,.6,.25),('5',-.93,1.4,.25,.6),('6',.37,.85,.25,1.7),('7',.87,1.4,.25,.6),('8',.95,-.65,.4,2.1),('8',1.42,-.75,.56,.25),('8',1.42,-.25,.56,.25),('8',1.42,.25,.56,.25),('9',-.21,-.65,.4,2.1),('10',-.93,-1.4,.25,.6)]
    ck('TI RPA0010A exact recommended lands including connected VIN fingers',lands('Texas_RPA0010A_HotRod_3x3')==sorted(rpa))
    mx=[(str(i+1),-3.125+i*1.25,0,.7,1.7) for i in range(6)]+[('MP',-5.225,-2.5,1.8,2.5),('MP',5.225,-2.5,1.8,2.5)]
    ck('MX125 six-pin lands and 3mm side-pad outer offsets',lands('ZX_MX1.25_6PWT')==sorted(mx))
    ck('two TFT mounting holes retained',all(r in current and node(current[r][1],'in_bom')[1]=='no' for r in ['H1','H2']))
    for ref,kind in [('U1','AP64500'),('U6','AP64500'),('U8','TPS25810'),('U16','TPS25810'),('U11','SN74LV1T125'),('U12','SN74LVC1G125'),('J18','Lichee_header')]:
        f,z=current[ref];definition=next(x for x in nodes(node(schemas[f],'lib_symbols'),'symbol') if x[1]==node(z,'lib_id')[1])
        found={node(p,'number')[1]:node(p,'name')[1] for s in nodes(definition,'symbol') for p in nodes(s,'pin')}
        ck(ref+' primary manufacturer pinout',found==PRIMARY_PINOUTS[kind])
    ap=[(str(i+1),-2.4975,y,1.505,.802) for i,y in enumerate([-1.905,-.635,.635,1.905])]+[(str(i+5),2.4975,y,1.505,.802) for i,y in enumerate([1.905,.635,-.635,-1.905])]+[('9',0.,0.,2.613,3.502)]
    ck('AP64500 primary recommended SO8EP copper lands',[p for p in lands('Diodes_SO8EP_AP64500') if p[0]]==sorted(ap))
    ck('AP64500 four split paste apertures',[p for p in lands('Diodes_SO8EP_AP64500') if not p[0]]==sorted([('',x,y,1.05,1.4) for x in [-.65,.65] for y in [-.875,.875]]))
    group('U8:14','U8:14 U8:15 J25:A9 J25:B9 C94:1')
    group('J18:2','J18:2 Q3:1 R75:1')
    group('J18:11','J18:11 U12:4');group('J18:12','J18:12 U11:2')
    group('J14:B26','J14:B26 R73:2 U12:2');group('J14:B27','J14:B27 R79:2 U11:4')
    for p in ['1','5','6','7','8','9','10']:group('J18:'+p,'J18:'+p)
    ck('two raw 5V rails isolated',pins['L1:2']!=pins['L2:2'])
    ck('TPS input bulk and independent switched outputs',all(bulk in pins[anchor] and out not in pins[anchor] for bulk,anchor,out in [('C92:1','U8:2','J25:A9'),('C81:1','U16:2','J28:A9')]))
    ck('5.088V divider',abs(record['bus_voltage_nominal_v']-.8*(1+53.6/10))<1e-10)
    for ref in ['C63','C89']:ck(ref+' ESR-aware compensation',prop(current[ref][1],'Value')[2]=='470pF/50V C0G')
    power=json.loads((DOC/'m28_power_analysis.json').read_text(encoding='utf8'));ck('declared Eq19 envelope covered',power['eq19_envelope_satisfied'])
    erc=json.loads((DOC/'erc.json').read_text(encoding='utf8'));violations=[v for s in erc['sheets'] for v in s['violations']]
    ck('native ERC no violations',not violations,violations)
    project=json.loads((ROOT/'CoralC5-Motor.kicad_pro').read_text(encoding='utf8'));ck('no ERC exclusions',not project.get('erc',{}).get('erc_exclusions'))
    ck('project settings byte preserved',sha(ROOT/'CoralC5-Motor.kicad_pro')==baseline['files']['CoralC5-Motor.kicad_pro']['sha256'])
    repo=ROOT.parents[3]
    for name,want in {**baseline['protected_sha256'],**baseline['history_sha256']}.items():ck(name+' protected SHA unchanged',sha(repo/name)==want)
    procurement=json.loads((DOC/'procurement_m28.json').read_text(encoding='utf8'))
    for p in procurement['parts']:
        raw=json.loads((DOC/p['record']).read_text(encoding='utf8'));prod=raw['product']
        ck(p['lcsc']+' actual domestic purchasable snapshot',raw['url'].startswith('https://item.szlcsc.com/') and prod['hasStockNow']=='yes' and prod['stockNumber']>0 and not prod['isShowQuotedPriceBtn'] and (raw['gdWarehouseStockNumber'] or 0)+(raw['jsWarehouseStockNumber'] or 0)>0)
        ck(p['lcsc']+' source MPN exact',p['mpn']==prod['productModel'])
    with (DOC/'core_motor_98_contact_audit.csv').open('w',encoding='utf-8-sig',newline='') as out:
        w=csv.DictWriter(out,fieldnames=list(audit[0]));w.writeheader();w.writerows(audit)
    generic=sorted({'F1','F2','SW1','SW2','SW3','SW4','SW5','U10','U1','U6','J19','J18'})
    report=dict(date='2026-11-07',revision='M2.8',kicad_version=erc['kicad_version'],source_commit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT).decode().strip(),working_tree_edits=True,baseline=str(base.relative_to(ROOT)),checks=len(checks),failed=[c for c in checks if not c['pass']],native_nets=len(observed),native_erc_errors=sum(v['severity']=='error' for v in violations),native_erc_warnings=sum(v['severity']=='warning' for v in violations),physical_references=len(resources),gpio_roles=len(assignments),model_bound_references=sum(bool(r['models']) for r in resources),mechanical_holes=2,unbound_models=missing,generic_model_references=generic,missing_exact_models=sorted(generic+missing),resources=resources,artifacts_sha256={f:sha(ROOT/f) for f in schemas},results=checks,procurement_unresolved=procurement['unresolved'],limitations=power['limits']+['Schematic, resource audit, PCB synchronization, native DRC and physical verification are separate. No manufacturing or full system validation claimed.','Maix supply nominal0.9986A has tolerance; recognition plus charging can cause brownout. Remove J27 before powered USB debug; battery can keep module alive.','J18 and SW2-SW5 exact purchase/lead geometry and TFT hole pitch need confirmation; generic model bodies are not exact supplier models.'])
    report['historical_baseline_CRLF_hash_matches']=historical_normalized
    report['run_at']=datetime.now().astimezone().isoformat(timespec='seconds')
    report['pass']=not report['failed'];(DOC/'verification.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print(json.dumps({k:v for k,v in report.items() if k not in ['results','resources','artifacts_sha256','limitations','historical_baseline_CRLF_hash_matches']},ensure_ascii=False,indent=2))
    return 0 if report['pass'] else 1

if __name__=='__main__':raise SystemExit(main())
