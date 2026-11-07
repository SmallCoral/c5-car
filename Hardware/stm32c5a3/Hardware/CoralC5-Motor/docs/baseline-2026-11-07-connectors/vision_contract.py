"""Bounded M2.5 vision changes; intended connectivity is independent of exports."""
import copy,json
from sexpr import node,nodes,prop

def partitions(parts, record):
    """Apply only the pin removals, rail additions and new nets in the design brief."""
    if not record:return set(parts)
    edit=record['net_edit'];removed=set(edit['remove_pins'])
    result={frozenset(':'.join(p) if isinstance(p,tuple) else p for p in part)-removed for part in parts}
    result.discard(frozenset())
    for anchor,added in edit['augment'].items():
        matches=[p for p in result if anchor in p]
        if len(matches)!=1:raise ValueError('Revision anchor must exist exactly once: '+anchor)
        part=matches[0];result.remove(part);result.add(part|frozenset(added))
    result.update(frozenset(p) for p in edit['new_nets'])
    flat=[p for part in result for p in part]
    if len(flat)!=len(set(flat)):raise ValueError('A pin belongs to multiple intended nets')
    return {frozenset(tuple(p.split(':',1)) for p in part) for part in result}

def library(old,new,record,cache=False):
    """Normalize only authored additions and two header names, then compare fully."""
    if not record:return
    prefix='Motor:' if cache else ''
    if cache:
        for name,definition in record.get('extra_cached_library_definitions',{}).items():
            key=prefix+name
            if key not in old and key in new:
                expected=copy.deepcopy(definition);expected[1]=key
                if new[key]!=expected:raise ValueError('Existing library copied into new cache differs: '+key)
                new.pop(key)
    for name,definition in record['new_library_definitions'].items():
        key=prefix+name
        if key in new:
            expected=copy.deepcopy(definition);expected[1]=key
            if new[key]!=expected:raise ValueError('New library definition differs: '+key)
            new.pop(key)
    for name,definition in record['updated_library_definitions'].items():
        key=prefix+name
        if key not in old:continue
        for field in ['Value']:
            prop(old[key],field)[2]=prop(definition,field)[2]
        desired={z[1]:z for z in nodes(definition,'symbol')}
        for sub in nodes(old[key],'symbol'):sub[:]=copy.deepcopy(desired[sub[1]])

def checks(root, actual, schemas, record):
    if not record:return []
    result=[]
    def ck(name,ok,detail=None):result.append({'name':'vision: '+name,'pass':bool(ok),'detail':detail})
    def group(pin):return actual.get(pin,frozenset())
    byref={prop(z,'Reference')[2]:z for d in schemas.values() for z in nodes(d,'symbol')}
    for ref,fields in record['new_component_values'].items():
        ck(ref+' authorized values and package',all(prop(byref[ref],k)[2]==v for k,v in fields.items()))
    for ref,mpn in [('U6','AP63300WU-7'),('U8','AP63300WU-7'),('U9','TPS259470LRPWR'),('U10','TPS259470LRPWR'),('U11','SN74LV1T125DBVR'),('U12','SN74LV1T125DBVR'),('U13','SN74LVC2G125DCTR'),('F1','0448015.MR')]:
        ck(ref+' exact selected MPN',prop(byref[ref],'MPN')[2]==mpn)
    for ref in ['U9','U10']:
        ck(ref+' open AUXOFF/nFLT/ITIMER only',all(group(ref+':'+p)=={ref+':'+p} for p in ['3','4','10']))
        ck(ref+' ground pin 8','J1:2' in group(ref+':8'))
    for ref,expected in [('R65','49.9k / 0.1%'),('R66','15k / 0.1%'),('R70','49.9k / 0.1%'),('R71','15k / 0.1%'),('R67','953 / 1%'),('R72','1.5k / 1%')]:
        ck(ref+' protection setting',prop(byref[ref],'Value')[2]==expected)
    for ref in ['Q3','Q4']:ck(ref+' low gate voltage rated FET',prop(byref[ref],'MPN')[2]=='BSS138BK,215')
    ck('separate 5V rails',group('J25:1')!=group('J19:1')!=group('U7:1') and group('J25:1')!=group('U7:1'))
    ck('Lichee power connector after protection',group('J25:1')=={'J25:1','U9:6','C70:1'})
    ck('Lichee header sense is input only',group('J18:1')=={'J18:1','Q3:1','R74:1'})
    ck('Lichee UART TX 1.8V goes into low-threshold buffer',group('J18:4')=={'J18:4','U11:2','R75:1'})
    ck('Lichee divided RX gets 3.3V buffered drive',group('J18:3')=={'J18:3','U12:4'})
    ck('buffers powered from Core rail','J14:A47' in group('U11:5') and 'J14:A47' in group('U13:8'))
    ck('Maix input power and UART gates share protected rail','U10:6' in group('J19:1') and 'Q4:1' in group('J19:1'))
    b=record['budget']
    ck('current budget uses converted power',all(abs(b['input_current_a'][str(v)]-(7+5*6.25/(v*.85)))<.0001 for v in [10,12,12.6]))
    ck('15A fuse 75% continuous derating covers declared budget',15*.75>b['input_current_a']['10'])
    ck('threshold record includes resistor, threshold and input leakage tolerances',5.18<record['thresholds']['ovlo_typ_v']<5.20 and record['thresholds']['ovlo_with_threshold_resistor_and_leakage_tolerances_v'][1]<5.31)
    return result
