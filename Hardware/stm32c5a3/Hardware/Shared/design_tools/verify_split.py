"""Audit both native KiCad boards, their fresh netlists, and the mating contract.

Run with KiCad 10's Python after ERC, netlist export, board placement and DRC.
This is a connectivity and work-in-progress audit, not a fabrication release gate.
"""
from pathlib import Path
from collections import Counter
import json, csv, re, sys, xml.etree.ElementTree as ET
import pcbnew as pcb

H=Path(__file__).resolve().parents[2]
from sexpr import parse, node, nodes, prop
from pin_functions import parse_alternate_functions

checks=[];failures=[]
def check(title,condition):
    if condition:checks.append(title)
    else:failures.append(title)

projects={};netpin={}
for key,name in [('C','CoralC5-Core'),('E','CoralC5-Expansion')]:
    folder=H/name;xml=ET.parse(folder/'docs/netlist.xml')
    components={c.attrib['ref']:c for c in xml.findall('./components/comp')}
    manifest=json.loads((folder/'docs/design_manifest.json').read_text(encoding='utf-8'))
    board=pcb.LoadBoard(str(folder/(name+'.kicad_pcb')))
    fps={f.GetReference():f for f in board.GetFootprints() if not f.GetAttributes()&pcb.FP_BOARD_ONLY}
    check(name+' components match PCB',set(components)==set(fps))
    check(name+' component manifest matches schematic',set(manifest['references'])==set(components))
    for net in xml.findall('./nets/net'):
        nn=net.attrib['name']
        for pin in net.findall('node'):netpin[(key,pin.attrib['ref'],pin.attrib['pin'])]=nn
    libparts={l.attrib['part']:l for l in xml.findall('./libparts/libpart')}
    resources={};generic=[];bom=[];bare=[]
    for ref,c in components.items():
        f=fps[ref]
        expected={p.attrib['num'] for p in libparts[c.find('libsource').attrib['part']].findall('./pins/pin')}
        actual={p.GetNumber() for p in f.Pads() if p.GetNumber()}
        check(name+' '+ref+' symbol-pad mapping',expected==actual)
        for p in f.Pads():
            expected_net=netpin.get((key,ref,p.GetNumber()))
            if p.GetNumber():
                normalized=p.GetNetname().replace('{slash}','/')
                check(name+' '+ref+'.'+p.GetNumber()+' PCB net',normalized==(expected_net or '').replace('{slash}','/'))
        footprint=c.findtext('footprint').split(':',1)[1]
        data=parse((folder/'DevBoard.pretty'/(footprint+'.kicad_mod')).read_text(encoding='utf-8'))
        models=[str(m[1]) for m in nodes(data,'model')]
        check(name+' '+ref+' model paths',all('${KIPRJMOD}/Models/' in m and (folder/'Models'/m.rsplit('/',1)[1]).exists() for m in models))
        if ref=='J13' or ref.startswith('TP'):
            bare.append(ref);check(name+' '+ref+' board feature',not models)
        else:
            check(name+' '+ref+' model bound',bool(models))
            bom.append([ref,c.findtext('value'),c.findtext('footprint'),any(p.attrib.get('name')=='dnp' for p in c.findall('property')),';'.join(models)])
        if any('GENERIC_' in m for m in models):generic.append(ref)
        resources[ref]={'footprint':c.findtext('footprint'),'models':models}
    for pg in manifest['pages']:
        s=parse((folder/pg['file']).read_text(encoding='utf-8'))
        check(name+' '+pg['file']+' notes default font 100mil',all([str(v) for v in node(node(t,'effects'),'font')[1][1:]]==['2.54','2.54'] and node(node(t,'effects'),'font') and node(node(node(t,'effects'),'font'),'face') is None for t in nodes(s,'text')))
    erc=json.loads((folder/'docs/erc.json').read_text(encoding='utf-8'))
    check(name+' ERC clean',not any(s['violations'] for s in erc['sheets']))
    drc=json.loads((folder/'docs/drc.json').read_text(encoding='utf-8'))
    check(name+' schematic parity clean',not drc['schematic_parity'])
    # Dangling ends are reported by native DRC while routing is incomplete.
    # Keep them in remaining_drc without treating them as placement collisions.
    check(name+' no placement shorts, overlaps or silk errors',all(v['type'] in ['hole_clearance','track_dangling'] for v in drc['violations']))
    tracks=list(board.GetTracks());zones=list(board.Zones())
    copper_zones=[z for z in zones if not z.GetIsRuleArea()]
    netnames={n.attrib['name'].replace('{slash}','/') for n in xml.findall('./nets/net')}
    check(name+' routed copper uses schematic nets',all(t.GetNetname().replace('{slash}','/') in netnames for t in tracks+copper_zones))
    stage='partially routed; not a fabrication release' if tracks or zones else 'unrouted placement template'
    expected_layers=2 if key=='C' else 4
    check(name+' '+str(expected_layers)+' layers 1.6mm',board.GetCopperLayerCount()==expected_layers and abs(pcb.ToMM(board.GetDesignSettings().GetBoardThickness())-1.6)<1e-6)
    erc_items=[v for s in erc['sheets'] for v in s['violations']]
    result={'project':name,'erc_errors':sum(v['severity']=='error' for v in erc_items),'erc_warnings':sum(v['severity']=='warning' for v in erc_items),'schematic_parity_issues':len(drc['schematic_parity']),'physical_footprints':len(fps),'mounted_components':len(bom),'model_bound_components':len(bom),'bare_board_features':bare,'generic_envelope_models':generic,'schematic_pages':len(manifest['pages']),'stage':stage,'track_items':len(tracks),'zones':len(zones),'numbered_pads_checked':sum(bool(p.GetNumber()) for f in fps.values() for p in f.Pads()),'retired_references':manifest.get('retired_references',{}),'remaining_erc':erc_items,'remaining_drc':drc['violations'],'unconnected_items':len(drc['unconnected_items']),'resources':resources}
    projects[key]={'folder':folder,'manifest':manifest,'board':board,'fps':fps,'components':components,'result':result}
    with (folder/'docs/BOM.csv').open('w',encoding='utf-8-sig',newline='') as out:
        w=csv.writer(out);w.writerow(['Reference','Value','Footprint','DNP','3D_model']);w.writerows(bom)

def C(ref,pin):return ('C',ref,str(pin))
def E(ref,pin):return ('E',ref,str(pin))
def same(title,*pins):check(title,len({netpin[p] for p in pins})==1)
def distinct(title,*pins):check(title,len({netpin[p] for p in pins})==len(pins))

interface=projects['C']['manifest']['interface']
shared_interface=json.loads((H/'Shared/interface.json').read_text(encoding='utf-8'))
check('Shared and both board contact contracts match',shared_interface['contacts']==interface==projects['E']['manifest']['interface'])
pinmap=projects['C']['manifest']['mcu_pinmap'];gpios={v:k for k,v in pinmap.items() if v.startswith('P')}
check('98 mating contacts',len(interface)==98)
exported_gpios=set(gpios)-{'PH0','PH1'}
check('84 GPIO exports; PH0/PH1 reserved for HSE',len(gpios)==86 and {v['gpio'] for v in interface.values() if v['gpio']}==exported_gpios)
for contact,d in interface.items():
    if d['net'] is None:
        check(contact+' explicit NC',netpin.get(C('J13',contact),'unconnected').startswith('unconnected') and netpin.get(E('J14',contact),'unconnected').startswith('unconnected'))
        continue
    same(contact+' Core-Expansion matching',C('J13',contact),E('J14',contact))
    check(contact+' named net',netpin[C('J13',contact)]==d['net'])
    if d['gpio']:
        if d['clock_link']:
            rr=d['clock_link'];pn=[netpin[C(rr,p)] for p in ['1','2']]
            check(contact+' DNP clock isolation',set(pn)=={d['gpio'],d['net']})
            check(rr+' is DNP',any(p.attrib.get('name')=='dnp' for p in projects['C']['components'][rr].findall('property')))
        else:same(contact+' MCU pad',C('U1',d['mcu_pin']),C('J13',contact))
    elif d['mcu_pin']:
        same(contact+' MCU special signal pad',C('U1',d['mcu_pin']),C('J13',contact))

# Use actual placed pad coordinates and copper-layer membership. PAD.GetLayer()
# reports a base layer for edge-connector pads and does not identify their face.
edge=projects['C']['fps']['J13'];mcu=projects['C']['fps']['U1']
edgepads={p.GetNumber():p for p in edge.Pads() if p.GetNumber()}
mcupads={p.GetNumber():p for p in mcu.Pads()}
cy=mcu.GetPosition().y
for contact,pad in edgepads.items():
    front=contact.startswith('B')
    check(contact+' correct contact copper face',pad.IsOnLayer(pcb.F_Cu if front else pcb.B_Cu) and not pad.IsOnLayer(pcb.B_Cu if front else pcb.F_Cu))
    d=interface[contact]
    if d['mcu_pin']:
        check(contact+' MCU physical half matches contact face',(mcupads[d['mcu_pin']].GetPosition().y>cy)==front)
for side,count in [('A',43),('B',43)]:
    contacts=sorted([k for k,d in interface.items() if k.startswith(side) and d['mcu_pin']],key=lambda k:edgepads[k].GetPosition().x)
    xs=[mcupads[interface[k]['mcu_pin']].GetPosition().x for k in contacts]
    check(side+' physical left-to-right signal ordering',len(xs)==count and xs==sorted(xs))
exported={d['mcu_pin'] for d in interface.values() if d['mcu_pin']}
expected={p.GetNumber() for p in mcu.Pads() if netpin[C('U1',p.GetNumber())] not in ['GND','+3V3',netpin[C('C7',1)],'PH0','PH1']}
check('All 86 non-power non-HSE MCU signals exported exactly once',len([d for d in interface.values() if d['mcu_pin']])==86 and exported==expected)

same('Core buck feedback',C('U2',1),C('L1',2),C('U1',11),E('J12',1))
same('Expansion buck feedback',E('U6',1),E('L2',2),E('U5',3),E('J7',4))
same('Shared protected 5V',E('D1',1),E('U6',3),C('U2',3),C('D2',1))
if 'JP1' in projects['C']['components']:
    same('USB jumper input',C('J2','A4'),C('JP1',1))
    same('USB jumper feeds D2 anode',C('JP1',2),C('D2',2))
else:
    same('USB directly feeds D2 anode after JP1 removal',C('J2','A4'),C('D2',2))
distinct('Both 3V3 domains, 5V and ground distinct',C('U2',1),E('U6',1),C('U2',3),C('U2',4))
check('Four-line SWD only on Expansion','J12' not in projects['C']['fps'] and 'J12' in projects['E']['fps'])
for pin,net in [('1','+3V3'),('2','PA13'),('3','PA14'),('4','GND')]:check('SWD '+pin,netpin[E('J12',pin)]==net)
for i,net in enumerate(['+3V3','PA13','PA14','GND'],1):
    ref='TP'+str(i)
    if ref in projects['C']['manifest'].get('removed_board_features',{}):
        check('Core removed debug '+ref,ref not in projects['C']['components'] and ref not in projects['C']['fps'])
    else:check('Core debug '+ref,netpin.get(C(ref,1))==net)
same('Reset RC and button',C('U1',14),C('SW1',1),C('R6',2),C('C16',1),E('TP5',1))
same('BOOT0 is PH2 pin94',C('U1',94),C('SW2',1),C('R7',1))
same('User key PD2',C('U1',83),C('SW5',1))
same('Yellow-green power LED',C('R50',1),C('U1',11))
same('Red user LED PE3',C('R51',1),C('U1',2))
for d,r,color in [('D3','R50','YELLOW-GREEN'),('D4','R51','RED')]:
    same(d+' series resistor',C(r,2),C(d,2));same(d+' cathode ground',C(d,1),C('U1',10))
    check(d+' correct color',projects['C']['components'][d].findtext('value')==color)
    check(r+' 1k current limit',projects['C']['components'][r].findtext('value')=='1k')
for pin,esd in [('A6',3),('B6',3),('A7',1),('B7',1)]:same('USB '+pin,C('J2',pin),C('U3',esd))
same('USB DM ESD to zero-ohm link',C('U3',6),C('R3',1))
same('USB DM link to MCU',C('R3',2),C('U1',70))
same('USB DP ESD to zero-ohm link',C('U3',4),C('R4',1))
same('USB DP link to MCU',C('R4',2),C('U1',71))
for ref in ['R3','R4']:
    check(ref+' USB zero-ohm link',projects['C']['components'][ref].findtext('value')=='0R' and projects['C']['fps'][ref].GetValue()=='0R')
check('VCAP separate from 3V3',netpin[C('C7',1)]!=netpin[C('U1',11)])
for ref in ['Y2','C19','C20']:
    check(ref+' removed with optional LSE',ref not in projects['C']['components'] and ref not in projects['C']['fps'])
for ref in ['R45','R46','R47','R48']:
    check(ref+' removed from schematic and PCB',ref not in projects['C']['components'] and ref not in projects['C']['fps'])
same('PC14 directly exported without LSE or link',C('U1',8),C('J13','A16'),E('J14','A16'),E('J9',31))
same('PC15 directly exported without LSE or link',C('U1',9),C('J13','A15'),E('J14','A15'),E('J9',32))
same('HSE PH0 retained locally',C('U1',12),C('Y1',1),C('C17',1))
same('HSE PH1 retained locally',C('U1',13),C('Y1',3),C('C18',1))
for pin in ['33','34']:
    check('J9.'+pin+' unused after HSE export removal',netpin[E('J9',pin)].startswith('unconnected'))

mapping=[('ETH_RXD0','U4',8),('ETH_RXD1','U4',7),('ETH_CRS_DV','U4',11),('ETH_MDIO','U4',12),('ETH_MDC','U4',13),('ETH_TXD0','U4',17),('ETH_TXD1','U4',18),('ETH_TXEN','U4',16),('PHY_NRST','U4',15),('PHY_INT','U4',14),('ETH_REFCLK','R27',2),('SD_SCK','R43',1),('SD_MOSI','R42',1),('SD_MISO','J7',7),('SD_CS','J7',2),('SD_DETECT','J7',10),('WIFI_TX','R35',1),('WIFI_RX','R36',1),('WIFI_EN','R30',1),('WIFI_BOOT','R34',1)]
assign=projects['C']['manifest']['assignments']
for func,ref,pin in mapping:same(func+' signal through edge',C('U1',gpios[assign[func]]),E(ref,pin))
same('PHY external oscillator fanout',E('Y3',3),E('R26',1),E('R27',1))
same('PHY clock input',E('R26',2),E('U4',5))
same('PHY address zero',E('U4',10),E('R13',1))
same('PHY 1.2V core bank',E('U4',6),E('C25',1),E('C26',1))
for phy,rj,res in [(21,1,'R17'),(20,2,'R18'),(23,3,'R19'),(22,6,'R20')]:same('Ethernet pair '+str(phy),E('U4',phy),E('J3',rj),E(res,2))
same('SD CMD',E('R42',2),E('J7',3));same('SD CLK',E('R43',2),E('J7',5))
same('ESP AT RX GPIO6',E('R35',2),E('U5',20));same('ESP AT TX GPIO7',E('R36',2),E('U5',21))
same('ESP RESET button',E('SW3',1),E('U5',8));same('ESP download button',E('SW4',1),E('U5',23))
check('Separate EXTI1 EXTI4 EXTI2',len({int(assign[n][2:]) for n in ['PHY_INT','SD_DETECT','USER_KEY']})==3)
check('14 verified alternate functions',len(projects['C']['manifest']['alternate_functions'])==14)
source=(H/'Shared/sources/DS15137_STM32C5Axxx_Datasheet_Rev3.pdf.txt').read_text(encoding='utf-8')
parsed={}
for line in source.splitlines():
    m=re.match(r'^\s*'+r'(-|\d+)\s+'*8+r'(\S+)\s+(I/O|S)\s',line)
    if m and m.group(7)!='-':
        raw=m.group(9);gpio=re.match(r'P[A-Z]\d+',raw);parsed[m.group(7)]=gpio.group(0) if gpio else raw
check('All 100 MCU pin numbers match DS15137 Table13',parsed==pinmap)
af=parse_alternate_functions(source)
functions={'ETH_REFCLK':'ETH1_RMII_REF_CLK','ETH_MDIO':'ETH1_MDIO','ETH_MDC':'ETH1_MDC','ETH_CRS_DV':'ETH1_RMII_CRS_DV','ETH_RXD0':'ETH1_RMII_RXD0','ETH_RXD1':'ETH1_RMII_RXD1','ETH_TXD0':'ETH1_RMII_TXD0','ETH_TXEN':'ETH1_RMII_TX_EN','ETH_TXD1':'ETH1_RMII_TXD1','SD_SCK':'SPI3_SCK','SD_MISO':'SPI3_MISO','SD_MOSI':'SPI3_MOSI','WIFI_TX':'USART3_TX','WIFI_RX':'USART3_RX'}
for func,signal in functions.items():check('DS15137 '+func+' AF',af[(assign[func],signal)]==projects['C']['manifest']['alternate_functions'][func])

rows=list(csv.DictReader((projects['E']['folder']/'docs/header_pinout.csv').open(encoding='utf-8-sig')))
check('Two 2x24 headers',len(rows)==96 and Counter(r['header'] for r in rows)=={'J8':48,'J9':48})
check('Header GPIO coverage excluding HSE',len({r['gpio'] for r in rows if r['gpio']})==84 and {r['gpio'] for r in rows if r['gpio']}==exported_gpios)
for r in rows:
    check(r['header']+'.'+r['pin']+' header mapping',netpin[E(r['header'],r['pin'])]==r['net'])
    if r['edge_contact']:check(r['header']+'.'+r['pin']+' current edge contact',interface[r['edge_contact']]['net']==r['net'])
check('Disjoint component references',not set(projects['C']['fps'])&set(projects['E']['fps']))
legacy=json.loads((H/'Shared/legacy_baseline.json').read_text(encoding='utf-8'))
oldrefs=set(legacy['component_references'])
retired={ref:reason for p in projects.values() for ref,reason in p['manifest'].get('retired_references',{}).items()}
currentrefs=set(projects['C']['fps'])|set(projects['E']['fps'])
check('Retired original components documented and absent',set(retired)<=oldrefs and not set(retired)&currentrefs and all(retired.values()))
check('All other original components preserved',len(oldrefs)==115 and oldrefs-set(retired)<=currentrefs)
before=legacy['pcb_sha256']
for k,refs in [('C',['SW1','SW2','SW5']),('E',['SW3','SW4'])]:
    for ref in refs:
        check(ref+' SMT two-terminal 2.5mm switch',projects[k]['fps'][ref].GetFPID().GetLibItemName()=='SW_SD1107_2P_6x3_H2.5' and len(list(projects[k]['fps'][ref].Pads()))==2)

# Compare every contact with the pinned upstream template, including side A/B.
original=pcb.FootprintLoad(str(H/'Shared/Streamline/sl.pretty'),'SL_BOARD')
edge=projects['C']['fps']['J13'];anchor=edge.GetPosition()
for p in original.Pads():
    if not p.GetNumber():continue
    q=next(q for q in edge.Pads() if q.GetNumber()==p.GetNumber())
    check('Mechanical '+p.GetNumber(),(q.GetPosition()-anchor)==p.GetPosition() and q.GetSize()==p.GetSize() and list(q.GetLayerSet().Seq())==list(p.GetLayerSet().Seq()))
for k,d in projects.items():
    d['result']['passed_checks_shared_total']=len(checks)
    d['result']['failed_checks_shared']=failures
    (d['folder']/'docs/verification.json').write_text(json.dumps(d['result'],ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
result={'passed_checks':len(checks),'failed_checks':failures,'checks':checks,'legacy_pcb_sha256':before,'retired_references':retired,'stage':'split-board connectivity audit; routing incomplete; inspect native ERC/DRC reports before continuing'}
(H/'Shared/verification.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(json.dumps({'passed_checks':len(checks),'failed_checks':failures,'Core':len(projects['C']['fps']),'Expansion':len(projects['E']['fps']),'remaining_drc_violations':sum(len(v['result']['remaining_drc']) for v in projects.values())}))
sys.exit(1 if failures else 0)
