"""Audit the exported native netlist against the electrical design contract.

Checks shorts as well as omissions, every physical pin/footprint mapping,
portable model paths, A4 hierarchy, 100mil notes, and native ERC. No PCB access.
"""
from pathlib import Path
import sys, json, csv, xml.etree.ElementTree as ET, hashlib, re
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT.parent/'Shared/design_tools'))
from sexpr import parse,node,nodes,prop
from pin_functions import parse_alternate_functions
DOC=ROOT/'docs'
if (DOC/'m27_revision.json').exists():
    from verify_m27 import main
    raise SystemExit(main())
if (DOC/'m2_design.json').exists():
    from verify_m2 import main
    raise SystemExit(main())
manifest=json.loads((DOC/'design_manifest.json').read_text(encoding='utf-8-sig'))
assignment=manifest['motor_pin_assignment']['entries']
signal_pins={entry['signal']:entry['contact'] for entry in assignment}
tree=ET.parse(DOC/'netlist.xml')
actual={};pin_net={}
for net in tree.findall('.//nets/net'):
    pins=frozenset(x.attrib['ref']+':'+x.attrib['pin'] for x in net.findall('node'))
    actual[net.attrib['name']]=pins
    for pin in pins:
        assert pin not in pin_net,pin
        pin_net[pin]=pins
checks=[]
def check(name,condition,detail=''):
    checks.append({'name':name,'pass':bool(condition),'detail':detail})
def group(name,parts):
    expected=frozenset(parts.split())
    found=next((pin_net[p] for p in sorted(expected) if p in pin_net),frozenset())
    check(name,found==expected,{'missing':sorted(expected-found),'unexpected':sorted(found-expected)})

group('external input','J1:1 F1:1')
group('fused MOS input','F1:2 Q1:5 Q1:6 Q1:7 Q1:8')
group('back-to-back common sources','Q1:1 Q1:2 Q1:3 Q2:1 Q2:2 Q2:3 D1:1 R1:2 SW1:1')
group('switch gates, default off','Q1:4 Q2:4 D1:2 R1:1 SW1:2')
group('ON throw gate resistor','SW1:3 R2:1')
group('protected motor rail','Q2:5 Q2:6 Q2:7 Q2:8 C1:1 D2:1 U1:3 C2:1 C3:1 R3:1 U2:11 U4:11 C11:1 C12:1 C13:1 C31:1 C32:1 C33:1')
group('buck bootstrap','U1:6 C4:2')
group('buck switch','U1:5 L1:1 C4:1')
group('buck enable divider','U1:2 R3:2 R4:1')
group('fixed output feedback before isolation','L1:2 U1:1 C5:1 C6:1 D3:2')
group('core supply after isolation','D3:1 J14:A48 J14:B48 J9:9 R5:1')
group('power LED','R5:2 D4:2')
group('run request',f'J14:{signal_pins["MOTOR_EN_REQ"]} R6:1')
group('NC stop loop supply','R6:2 J10:1')
group('common run and pulldowns','J10:2 R7:1 R14:1 R34:1 U2:3 U4:3')
group('core 3.3V loads','J14:A47 J14:B47 J8:1 J9:1 J4:2 J6:2 R10:1 R30:1 R13:2 R33:2 R18:1 R19:2 R38:1 R39:2 U3:5 U5:5 C19:1 C20:1 C39:1 C40:1')
group('SWDIO','J14:B43 J8:2');group('SWCLK','J14:A46 J8:3');group('NRST','J14:A12 J9:10')
for finger,pin in [('A30','3'),('A29','4'),('A2','5'),('B2','6'),('B3','7'),('B4','8')]:group('aux '+pin,f'J14:{finger} J9:{pin}')
for side,driver,buf,encoder,base in [('L','U2','U3','J4',10),('R','U4','U5','J6',30)]:
    pwm,direction,fault,current,ea,eb=(signal_pins[side+suffix] for suffix in ['_PWM','_DIR','_nFAULT','_ISENSE','_ENC_A','_ENC_B'])
    group(side+' PWM',f'J14:{pwm} {driver}:1')
    group(side+' DIR',f'J14:{direction} {driver}:2')
    group(side+' fault',f'J14:{fault} {driver}:4 R{base+3}:1')
    group(side+' output1',f'{driver}:8 {encoder}:6');group(side+' output2',f'{driver}:10 {encoder}:1')
    group(side+' charge pump',f'{driver}:12 C{base+1}:2')
    group(side+' flying cap high',f'{driver}:13 C{base}:1');group(side+' flying cap low',f'{driver}:14 C{base}:2')
    group(side+' VREF',f'{driver}:5 R{base}:2 R{base+1}:1 C{base+4}:1')
    group(side+' IPROPI burden',f'{driver}:6 R{base+2}:1 C{base+5}:1 R{base+5}:1')
    group(side+' ADC filter',f'R{base+5}:2 C{base+6}:1 J14:{current}')
    group(side+' encoder A raw',f'{encoder}:3 R{base+6}:1 R{base+8}:2')
    group(side+' encoder B raw',f'{encoder}:4 R{base+7}:1 R{base+9}:1')
    group(side+' Schmitt A',f'R{base+6}:2 C{base+7}:2 {buf}:1')
    group(side+' Schmitt B',f'R{base+7}:2 C{base+8}:1 {buf}:3')
    group(side+' encoder A to timer',f'{buf}:6 J14:{ea}');group(side+' encoder B to timer',f'{buf}:4 J14:{eb}')

ground='J1:3 C1:2 D2:2 R2:2 SW1:4 SW1:5 U1:4 C2:2 C3:2 R4:2 C5:2 C6:2 D4:1 J14:A1 J14:A49 J14:B1 J14:B49 J14:50 J14:51 J8:4 J9:2 R7:2'
for driver,buf,encoder,b in [('U2','U3','J4',10),('U4','U5','J6',30)]:
    ground+=' '+ ' '.join([f'{driver}:{pn}' for pn in ['7','9','15','16','17']]+[f'{buf}:2',f'{encoder}:5',f'R{b+1}:2',f'R{b+2}:2',f'R{b+4}:2']+[f'C{b+i}:2' for i in [2,3,4,5,6,8,9,10]]+[f'C{b+7}:1'])
group('all ground returns, EP, PH/EN and fixed off-time straps',ground)

components={};all_symbols=[]
for filename in ['CoralC5-Motor.kicad_sch','02_Power.kicad_sch','03_Core_Interface.kicad_sch','04_Left_Motor.kicad_sch','05_Right_Motor.kicad_sch']:
    sch=parse((ROOT/filename).read_text(encoding='utf-8'))
    check(filename+' A4',node(sch,'paper')[1]=='A4')
    for text in nodes(sch,'text'):
        check(filename+' note size',list(map(float,node(node(node(text,'effects'),'font'),'size')[1:]))==[2.54,2.54])
    for sym in nodes(sch,'symbol'):
        ref=prop(sym,'Reference')[2]
        if ref.startswith('#'):continue
        all_symbols.append(sym)
        item=components.setdefault(ref,{'fp':prop(sym,'Footprint')[2],'pins':set()})
        item['pins'].update(pin[1] for pin in nodes(sym,'pin'))

for removed_ref in ['J3','J5']:
    check(removed_ref+' removed',removed_ref not in components)

contract={r['contact']:r for r in csv.DictReader((ROOT.parent/'Shared/interface.csv').open(encoding='utf-8-sig'))}
datasheet=(ROOT.parent/'Shared/sources/DS15137_STM32C5Axxx_Datasheet_Rev3.pdf.txt').read_text(encoding='utf-8-sig')
functions=parse_alternate_functions(datasheet)
check('all 13 motor roles assigned',len(assignment)==13 and len(signal_pins)==13)
check('motor GPIOs unique',len({e['gpio'] for e in assignment})==len(assignment))
check('motor contacts unique',len(set(signal_pins.values()))==len(assignment))
check('reserved interfaces retained',not {e['gpio'] for e in assignment}&{'PA2','PA3','PB6','PB7','PA11','PA12','PA13','PA14','PH2','PE3','PD2'})
for entry in assignment:
    check(entry['signal']+' Core contact',contract[entry['contact']]['gpio']==entry['gpio'])
    if entry['af'] is not None:
        check(entry['signal']+' datasheet AF',functions.get((entry['gpio'],entry['function']))==entry['af'])
    if entry['function'].startswith('ADC'):
        blocks=datasheet.split('\n\n')
        check(entry['signal']+' datasheet analog input',any(re.search(r'\b'+entry['gpio']+r'\b[^\n]*\bI/O\b',block) and entry['function'] in block for block in blocks))
enc_timers=[]
for side in ['L','R']:
    entries=[e for e in assignment if e['signal'] in [side+'_ENC_A',side+'_ENC_B']]
    matches=[re.fullmatch(r'TIM(\d+)_CH([12])',e['function']) for e in entries]
    check(side+' hardware encoder pair',all(matches) and {m[2] for m in matches if m}=={'1','2'} and len({m[1] for m in matches if m})==1)
    enc_timers.append(matches[0][1] if matches[0] else '')
pwm_entries=[e for e in assignment if e['signal'] in ['L_PWM','R_PWM']]
pwm_matches=[re.fullmatch(r'TIM(\d+)_CH([1-4])',e['function']) for e in pwm_entries]
check('independent encoder timers',len(set(enc_timers))==2)
check('PWM shared timer and separate channels',all(pwm_matches) and len({m[1] for m in pwm_matches if m})==1 and len({m[2] for m in pwm_matches if m})==2)
check('PWM timers separate from encoders',all(m and m[1] not in enc_timers for m in pwm_matches))

models=set();generic=[]
for ref,item in components.items():
    lib,name=item['fp'].split(':');check(ref+' portable footprint',lib=='Motor')
    fp=parse((ROOT/'Motor.pretty'/(name+'.kicad_mod')).read_text(encoding='utf-8'))
    pads={pad[1] for pad in nodes(fp,'pad') if pad[1]}
    check(ref+' symbol to pad map',item['pins']==pads,{'symbol_only':sorted(item['pins']-pads),'pad_only':sorted(pads-item['pins'])})
    mm=nodes(fp,'model');check(ref+' model bound',bool(mm))
    for model in mm:
        path=model[1];check(ref+' model path',path.startswith('${KIPRJMOD}/Models/') and (ROOT/path.replace('${KIPRJMOD}/','')).is_file(),path);models.add(path)

active_fingers={'A1','A49','B1','B49','A47','B47','A48','B48','A12','A46','B43','A30','A29','A2','B2','B3','B4','50','51'}|set(signal_pins.values())
for pn in components['J14']['pins']-active_fingers:
    check('unused finger '+pn,len(pin_net.get('J14:'+pn,()))==1)
check('jack switched contact NC',pin_net.get('J1:2')==frozenset(['J1:2']))

erc=json.loads((DOC/'erc.json').read_text(encoding='utf-8'))
violations=[v for sheet in erc['sheets'] for v in sheet['violations']]
check('native ERC no violations',not violations)
pro=json.loads((ROOT/'CoralC5-Motor.kicad_pro').read_text(encoding='utf-8'));check('no ERC exclusions',not pro.get('erc',{}).get('erc_exclusions'))
result={'date':'2026-11-03','pass':all(x['pass'] for x in checks),'checks':len(checks),'failed':[x for x in checks if not x['pass']],
 'physical_references':len(components),'physical_pins':sum(len(x['pins']) for x in components.values()),'models':len(models),'native_erc_errors':sum(v['severity']=='error' for v in violations),'native_erc_warnings':sum(v['severity']=='warning' for v in violations),
 'model_exceptions':{'J1':'generic manufacturer-dimension external envelope, not exact internal assembly','SW1':'generic manufacturer-dimension external envelope, not exact internal assembly','U2,U4':'KiCad HTSSOP envelope; custom footprint follows TI PWP0016 drawing'},
 'limitations':['J3/J5 removed from schematic and PCB; native PCB DRC and parity results are separate from this schematic audit; no prototype verified','J9 is absent from the current schematic; original auxiliary-header contract is intentionally retained and its failures are reported','Battery usable energy and runtime are estimates','Cable polarity, connector mating direction and encoder operation at 3.3 V require hardware confirmation'],
 'artifacts_sha256':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(ROOT.glob('*.kicad_sch'))},'results':checks}
(DOC/'verification.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(json.dumps({k:v for k,v in result.items() if k not in ['results','artifacts_sha256']},ensure_ascii=False,indent=2))
raise SystemExit(0 if result['pass'] else 1)
