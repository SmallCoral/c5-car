"""M2.7 audit against the saved live user design and independent circuit intent.

Historical M2-M2.6 verifiers remain intact. This revision verifies the entire
native net partition, not only changed nets; unchanged circuit files, identities,
98 Core contacts, GPIO functions and local manufacturing resources are checked.
PCB synchronization and DRC are separate reports.
"""
from pathlib import Path
import sys,json,csv,copy,hashlib,subprocess,xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parents[1];DOC=ROOT/'docs'
sys.path.insert(0,str(ROOT.parent/'Shared/design_tools'))
from sexpr import parse,node,nodes,prop
from schematic_semantic import canonical
from pin_functions import parse_alternate_functions

def native(path):
    tree=ET.parse(path);groups=set();by_pin={}
    for n in tree.findall('.//nets/net'):
        p=frozenset(z.get('ref')+':'+z.get('pin') for z in n.findall('node'))
        groups.add(p)
        for pin in p:
            if pin in by_pin:raise ValueError('Duplicate native pin '+pin)
            by_pin[pin]=p
    return tree,groups,by_pin

def intended(before,edit):
    removed=set(edit['remove_references']);mapping=edit['pin_map'];parts=set()
    for group in before:
        part=frozenset(mapping.get(p,p) for p in group if p.split(':')[0] not in removed)
        if part:parts.add(part)
    def take(anchor):
        found=[g for g in parts if anchor in g]
        if len(found)!=1:raise ValueError('Missing or duplicate intent anchor '+anchor)
        parts.remove(found[0]);return found[0]
    for anchors in edit['merge_anchors']:
        merged=set()
        for anchor in anchors:merged.update(take(anchor))
        parts.add(frozenset(merged))
    for anchor,add in edit['augment'].items():parts.add(take(anchor)|frozenset(add))
    parts.update(frozenset(g) for g in edit['new_nets'])
    flat=[p for g in parts for p in g]
    if len(flat)!=len(set(flat)):raise ValueError('Intent assigns a pin twice')
    return parts

def main():
    record=json.loads((DOC/'m27_revision.json').read_text(encoding='utf8'))
    base=DOC/record['baseline'];baseline=json.loads((base/'baseline.json').read_text(encoding='utf8'))
    checks=[]
    def ck(name,ok,detail=None):checks.append({'name':name,'pass':bool(ok),'detail':detail})
    def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
    beforetree,before,_=native(base/'live-netlist.xml');tree,observed,pins=native(DOC/'netlist.xml')
    expected=intended(before,record['net_edit'])
    ck('all native nets equal independent M2.7 intent',observed==expected,{'missing':[sorted(g) for g in expected-observed],'extra':[sorted(g) for g in observed-expected]})
    schemas={p.name:parse(p.read_text(encoding='utf8')) for p in ROOT.glob('*.kicad_sch')}
    oldschemas={f:parse((base/f).read_text(encoding='utf8')) for f in schemas}
    oldrefs={prop(z,'Reference')[2]:(f,z) for f,d in oldschemas.items() for z in nodes(d,'symbol')}
    current={prop(z,'Reference')[2]:(f,z) for f,d in schemas.items() for z in nodes(d,'symbol')}
    ck('exactly nine hierarchical schematic files',len(schemas)==9)
    ck('reference deletions are exactly recorded',set(oldrefs)-set(current)==set(record['removed_references']))
    ck('reference additions are exactly recorded',set(current)-set(oldrefs)==set(record['added_references']))
    for f in schemas:
        ck(f+' baseline SHA verified',sha(base/f)==baseline['sha256'][f])
        if f not in record['changed_sheets']:ck(f+' live user file byte preserved',sha(ROOT/f)==sha(base/f))
        ck(f+' declared paper',node(schemas[f],'paper')[1]==record['paper'][f])
        for t in nodes(schemas[f],'text'):ck(f+' 100mil notes',list(map(float,node(node(node(t,'effects'),'font'),'size')[1:]))==[2.54,2.54])
        for z in nodes(schemas[f],'symbol'):
            ck(prop(z,'Reference')[2]+' unique reference/unit',sum(prop(q,'Reference')[2]==prop(z,'Reference')[2] and node(q,'unit')==node(z,'unit') for q in nodes(schemas[f],'symbol'))==1)
        oldunits={(prop(z,'Reference')[2],node(z,'unit')[1]):z for z in nodes(oldschemas[f],'symbol')}
        newunits={(prop(z,'Reference')[2],node(z,'unit')[1]):z for z in nodes(schemas[f],'symbol')}
        for key,z in oldunits.items():
            if key in newunits:
                ck(str(key)+' every retained unit UUID',node(z,'uuid')==node(newunits[key],'uuid'))
                if key[0]=='J14':ck(str(key)+' Core connector unit unchanged',z==newunits[key])
    moved=record['moved_references'];pose_changes={'C4','C5','D4','R5','C50','R59','R60','C63'}
    for ref,(of,z) in oldrefs.items():
        if ref not in current:continue
        nf,now=current[ref]
        ck(ref+' symbol UUID retained',node(z,'uuid')==node(now,'uuid'))
        ck(ref+' sheet preserved or explicit move',nf==(moved.get(ref) or {}).get('to',of))
        op={p[1]:node(p,'uuid')[1] for p in nodes(z,'pin')};np={p[1]:node(p,'uuid')[1] for p in nodes(now,'pin')}
        op={record['pin_map_from_prior'].get(ref+':'+p,ref+':'+p).split(':')[1]:u for p,u in op.items()}
        ck(ref+' pin UUIDs preserved through explicit map',all(np.get(p)==u for p,u in op.items()))
        if not ref.startswith('#') and ref not in pose_changes:ck(ref+' user symbol pose preserved',node(z,'at')==node(now,'at'))
        fields={p[1]:p[2] for p in nodes(z,'property')};fields.update({k:v for k,v in record['changes'].get(ref,{}).items() if k!='lib_id'})
        ck(ref+' only authorized property edits',{p[1]:p[2] for p in nodes(now,'property')}==fields)
        ck(ref+' authorized symbol type',node(now,'lib_id')[1]==record['changes'].get(ref,{}).get('lib_id',node(z,'lib_id')[1]))
    for ref,fields in record['new_parts'].items():ck(ref+' new part fields',{p[1]:p[2] for p in nodes(current[ref][1],'property')}==fields)
    oldlib=parse((base/'Motor.kicad_sym').read_text(encoding='utf8'));newlib=parse((ROOT/'Motor.kicad_sym').read_text(encoding='utf8'))
    olddefs={z[1]:z for z in nodes(oldlib,'symbol')};newdefs={z[1]:z for z in nodes(newlib,'symbol')}
    ck('source library only declared additions',set(newdefs)-set(olddefs)==set(record['new_library_definitions']))
    for k,v in olddefs.items():ck(k+' existing source symbol preserved',v==newdefs.get(k))
    for k,v in record['new_library_definitions'].items():ck(k+' new source definition verified',canonical(v)==canonical(newdefs[k]))
    for f,d in schemas.items():
        old={z[1]:z for z in nodes(node(oldschemas[f],'lib_symbols'),'symbol')};new={z[1]:z for z in nodes(node(d,'lib_symbols'),'symbol')}
        for k,z in old.items():ck(f+' existing cached '+k,canonical(z)==canonical(new.get(k)))
        for k,z in record['extra_cached_library_definitions'].get(f,{}).items():ck(f+' authorized cached '+k,canonical(z)==canonical(new.get(k)))
        ck(f+' cache addition set',set(new)-set(old)==set(record['extra_cached_library_definitions'].get(f,{})))
    # 98-contact power and GPIO contract against a fresh Core native export.
    _,_,core=native(DOC/'core_netlist.xml')
    contract=list(csv.DictReader((ROOT.parent/'Shared/interface.csv').open(encoding='utf-8-sig')))
    ck('98 unique Core contacts',len(contract)==98 and len({r['contact'] for r in contract})==98)
    motorlib=next(z for z in nodes(node(schemas['03_Core_Interface.kicad_sch'],'lib_symbols'),'symbol') if z[1].endswith('CoralC5_Streamline_Socket'))
    socket={node(p,'number')[1]:node(p,'name')[1] for s in nodes(motorlib,'symbol') for p in nodes(s,'pin')}
    assignments=json.loads((DOC/'baseline-2026-11-06-m2/design_manifest.json').read_text(encoding='utf-8-sig'))['motor_pin_assignment']['entries']+list(csv.DictReader((DOC/'m2_pin_assignment.csv').open(encoding='utf-8-sig')))
    functions=parse_alternate_functions((ROOT.parent/'Shared/sources/DS15137_STM32C5Axxx_Datasheet_Rev3.pdf.txt').read_text(encoding='utf-8-sig'))
    ck('38 distinct assigned GPIOs',len(assignments)==38 and len({r['gpio'] for r in assignments})==38)
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
    # Primary datasheet pin identities are deliberately specified here.
    for ref,identity in [('U1',{'1':'EN','2':'FB','3':'AGND','4':'PG','5':'SS','6':'SW','7':'BOOT','8':'VIN','9':'PGND','10':'MODE'}),('J19',{'1':'GND','2':'5V_IN','3':'SCL_NC','4':'SDA_NC','5':'UART4_TX','6':'UART4_RX','MP':'~'}),('U16',{'1':'nFAULT','2':'IN1','3':'IN1','4':'IN2','5':'AUX','6':'EN','7':'CHG','8':'CHG_HI','9':'REF_RTN','10':'REF','11':'CC1','12':'GND','13':'CC2','14':'OUT','15':'OUT','16':'nDEBUG','17':'nAUDIO','18':'nPOL','19':'nUFP','20':'nLD_DET','21':'EP_GND'})]:
        f,z=current[ref];definition=next(x for x in nodes(node(schemas[f],'lib_symbols'),'symbol') if x[1]==node(z,'lib_id')[1]);found={node(p,'number')[1]:node(p,'name')[1] for s in nodes(definition,'symbol') for p in nodes(s,'pin')}
        ck(ref+' primary manufacturer pinout',found==identity)
    ck('5V divider gives nominal 4.992V',abs(record['bus_voltage_nominal_v']-.6*(1+73.2/10))<1e-10)
    ck('Maix connector-rated nominal current limit',abs(record['maix_current_nominal_a']-(.0103*95.3+.017))<1e-10)
    b=record['power_budget'];ck('budget sums same-voltage loads',abs(b['total_5v_a']-sum(b[k] for k in ['core_5v_a','maix_5v_a','action4_5v_a','gray_5v_a','ldo_input_a']))<1e-8)
    ck('8A buck headroom',8>b['total_5v_a']*1.25)
    ck('15A fuse continuous derating budget',15*.75>b['input_current_a']['10'])
    erc=json.loads((DOC/'erc.json').read_text(encoding='utf8'));violations=[v for s in erc['sheets'] for v in s['violations']]
    ck('native ERC no violations',not violations,violations)
    project=json.loads((ROOT/'CoralC5-Motor.kicad_pro').read_text(encoding='utf8'));ck('no ERC exclusions',not project.get('erc',{}).get('erc_exclusions'))
    ck('project settings byte preserved',sha(ROOT/'CoralC5-Motor.kicad_pro')==baseline['sha256']['CoralC5-Motor.kicad_pro'])
    with (DOC/'core_motor_98_contact_audit.csv').open('w',encoding='utf-8-sig',newline='') as out:
        w=csv.DictWriter(out,fieldnames=list(audit[0]));w.writeheader();w.writerows(audit)
    generic=sorted({'F1','F2','SW1','SW2','SW3','SW4','SW5','U10','U1','J19'})
    report=dict(date='2026-11-07',revision='M2.7',kicad_version=erc['kicad_version'],source_commit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT).decode().strip(),checks=len(checks),failed=[c for c in checks if not c['pass']],native_erc_errors=sum(v['severity']=='error' for v in violations),native_erc_warnings=sum(v['severity']=='warning' for v in violations),physical_references=len(resources),model_bound_references=sum(bool(r['models']) for r in resources),mechanical_holes=2,unbound_models=missing,generic_model_references=generic,missing_exact_models=sorted(generic+missing),resources=resources,artifacts_sha256={f:sha(ROOT/f) for f in schemas},results=checks,limitations=['Schematic/ERC/resource verification does not certify PCB routing or physical hardware. PCB component sync and native DRC are reported separately.','8A is the buck IC rating, not a demonstrated board output. Thermal, load-step stability, shared-bus noise and short-circuit/inrush behavior require measurement.','Maix MX1.25 contacts are rated 1A; nominal eFuse limit is 0.9986A, with tolerance. Recognition plus battery charging may exceed the available branch power and cause brownout.','Maix six-pin 5V and USB VBUS are common internally: remove J27 before powered USB debug; battery can keep module alive.','Action4 output is fixed 5V, Type-C 3A advertisement, not PD/PPS fast charging. Recording plus charging demand must be tested.','C81 16SVP150M is domestic in-stock but manufacturer NRND; replacement needed for production longevity.','SW2-SW5 unbranded SMD button lead span/contact pairs and TFT mounting-hole pitch still require physical confirmation. Firmware must debounce buttons.','STEP_PWR_EN is PD12/B30, active high and hardware default off; stop motion before cutting power.'])
    report['pass']=not report['failed'];(DOC/'verification.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print(json.dumps({k:v for k,v in report.items() if k not in ['results','resources','artifacts_sha256']},ensure_ascii=False,indent=2))
    return 0 if report['pass'] else 1

if __name__=='__main__':raise SystemExit(main())
