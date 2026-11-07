"""M2.6 intent, checked against native KiCad connectivity, without PCB access."""
import copy,json,hashlib,xml.etree.ElementTree as ET
from sexpr import parse,node,nodes,prop
from vision_contract import partitions
from schematic_semantic import canonical

def normalize_library(old,new,record,cache=False,file=None):
    prefix='Motor:' if cache else ''
    for name,definition in record['new_library_definitions'].items():
        key=prefix+name
        if key in new:
            expected=copy.deepcopy(definition);expected[1]=key
            if canonical(new[key])!=canonical(expected):raise ValueError('M2.6 library definition differs: '+key)
            new.pop(key)
    if cache:
        for key,definition in record['extra_cached_library_definitions'].get(file,{}).items():
            if key not in old and key in new:
                if canonical(new[key])!=canonical(definition):raise ValueError('Copied source definition differs: '+key)
                new.pop(key)

def checks(root,actual,schemas,record):
    result=[]
    def ck(name,ok,detail=None):result.append({'name':'M2.6: '+name,'pass':bool(ok),'detail':detail})
    def group(pin):return actual.get(pin,frozenset())
    base=root/'docs'/record['baseline']
    before={frozenset((p.attrib['ref'],p.attrib['pin']) for p in n.findall('node')) for n in ET.parse(base/'live-netlist.xml').findall('.//nets/net')}
    intended=partitions(before,record)
    observed={frozenset(tuple(p.split(':',1)) for p in part) for part in actual.values()}
    ck('all native nets exactly match independent incremental intent',intended==observed,{'missing':[sorted(p) for p in intended-observed],'extra':[sorted(p) for p in observed-intended]})
    current={prop(z,'Reference')[2]:z for d in schemas.values() for z in nodes(d,'symbol')}
    ck('all explicitly removed references absent',not set(record['removed_references'])&current.keys())
    ck('Lichee sheet and all five obsolete signals removed',not (root/'10_Lichee_Power.kicad_sch').exists() and all(not set(record['removed_signals'])&{z[1] for z in nodes(d,'hierarchical_label')} for d in schemas.values()))
    for ref,fields in record['new_parts'].items():ck(ref+' authorized fields',ref in current and all(prop(current[ref],k) is not None and prop(current[ref],k)[2]==v for k,v in fields.items()))
    for file,tree in schemas.items():
        old=parse((base/file).read_text(encoding='utf8'))
        oldrefs={prop(z,'Reference')[2]:z for z in nodes(old,'symbol')};newrefs={prop(z,'Reference')[2]:z for z in nodes(tree,'symbol')}
        for ref,z in oldrefs.items():
            if ref in record['removed_references']:continue
            now=newrefs.get(ref)
            ck(ref+' live user UUID retained',now is not None and node(z,'uuid')==node(now,'uuid'))
            if now is None:continue
            expected={p[1]:node(p,'uuid')[1] for p in nodes(z,'pin')}
            if ref=='U10':expected={record['pin_map_from_prior'].get(ref+':'+p,ref+':'+p).split(':')[1]:u for p,u in expected.items() if record['pin_map_from_prior'].get(ref+':'+p,ref+':'+p) is not None}
            actualpins={p[1]:node(p,'uuid')[1] for p in nodes(now,'pin')}
            ck(ref+' existing pin UUIDs retained through explicit renumbering',all(actualpins.get(p)==u for p,u in expected.items()))
        if file not in record['changed_sheets']:ck(file+' user file byte-for-byte preserved',(root/file).read_bytes()==(base/file).read_bytes())
    ck('PD12 B30 powers the NPN through a resistor',group('J14:B30')=={'J14:B30','R83:1'})
    ck('NPN base has hard pull-down',group('Q8:1')=={'Q8:1','R83:2','R84:1'} and 'J1:2' in group('R84:2'))
    ck('NPN emitter grounded','J1:2' in group('Q8:2'))
    ck('source and gate pull-up guarantees off with floating GPIO',group('Q6:3')=={'Q6:3','Q7:3','R81:1','D5:1'} and group('Q6:1')=={'Q6:1','Q7:1','R81:2','R82:1','D5:2'})
    ck('gate pull-down isolated from MCU pin',group('R82:2')=={'R82:2','Q8:3'} and 'Q8:3' not in group('J14:B30'))
    for ref,value in [('R81','100k'),('R82','4.7k'),('R83','10k'),('R84','100k')]:ck(ref+' power-control setting',prop(current[ref],'Value')[2]==value)
    ck('fused input precedes power switch',group('F2:2')=={'F2:2','Q6:2'})
    ck('8P output after power switch',group('J23:1')=={'J23:1','Q7:2','R86:1'})
    for pin in range(3,7):ck('J23:'+str(pin)+' genuinely isolated',group('J23:'+str(pin))=={'J23:'+str(pin)})
    ck('TTL TX to module RX through buffer',group('J23:7')=={'J23:7','U15:6','R90:1'} and group('J14:A30')=={'J14:A30','U15:2','R88:2'})
    ck('TTL RX from module TX through buffer',group('J23:8')=={'J23:8','U15:5','R89:1'} and group('J14:A29')=={'J14:A29','U15:3'})
    ck('UART OE defaults high from Core supply',group('U15:1')=={'U15:1','U15:7','Q9:3','R85:2'} and 'J14:A47' in group('R85:1'))
    ck('UART OE senses actual protected step rail via divider',group('Q9:1')=={'Q9:1','R86:2','R87:1'} and 'J1:2' in group('R87:2'))
    ck('Maix ideal diode direction',group('U10:5')=={'U10:5','Q5:2','U14:1','C79:1'} and group('Q5:3')=={'Q5:3','U14:6','J19:1','C62:1','Q4:1','R77:1'})
    ck('ideal-diode controller on and gate connected','J1:2' in group('U14:3') and group('U14:5')=={'U14:5','Q5:1'})
    ck('SGM ground and exposed pad grounded',all('J1:2' in group('U10:'+p) for p in ['1','9']))
    ck('Maix current is a nominal formula result, not guaranteed ±5% at 2A',abs(record['SGM']['ilim_formula_typ_a']-(.0103*215+.017))<1e-8 and 'not tabulated' in record['SGM']['current_tolerance_note'])
    b=record['power_budget'];ck('new current budget excludes Lichee load',all(abs(b['input_current_a'][str(v)]-(7+5*3.25/(v*.85)))<.0001 for v in [10,12,12.6]))
    ck('15A fuse derated at 75 percent exceeds budget',15*.75>b['input_current_a']['10'])
    fp=parse((root/'Motor.pretty/SGM_PS8_SOIC8_EP2.33x3.22_HandSolder.kicad_mod').read_text(encoding='utf8'))
    pads={p[1]:p for p in nodes(fp,'pad') if p[1]}
    ck('SGM pins and EP follow manufacturer land pattern',set(pads)==set(map(str,range(1,10))) and list(map(float,node(pads['9'],'size')[1:3]))==[2.33,3.22] and all(list(map(float,node(pads[str(i)],'size')[1:3]))==[1.91,.61] for i in range(1,9)))
    ck('SGM 1.27mm lead pitch',all(abs(abs(float(node(pads[str(i)],'at')[2])-float(node(pads[str(i+1)],'at')[2]))-1.27)<1e-8 for i in [1,2,3,5,6,7]))
    courtyard=[q for line in nodes(fp,'fp_line') if node(line,'layer')[1]=='F.CrtYd' for q in [node(line,'start'),node(line,'end')]]
    ck('SGM courtyard clears outer leads by at least 0.25mm',courtyard and min(float(q[1]) for q in courtyard)<=-3.985 and max(float(q[1]) for q in courtyard)>=3.985)
    return result
