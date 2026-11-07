"""Refresh current inventory/BOM metadata from verified exports; no design edits."""
from pathlib import Path
from datetime import datetime
import json,csv,hashlib,xml.etree.ElementTree as ET
DOC=Path(__file__).resolve().parents[1]/'docs';ROOT=DOC.parent
def read(name):return json.loads((DOC/name).read_text(encoding='utf-8-sig'))
def write(name,obj):(DOC/name).write_text(json.dumps(obj,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
    if (DOC/'f8_association_repair.json').exists():
        raise SystemExit('The original M2.8 metadata inputs are historical after the F8 repair/current user layout. Use the latest association report; do not regenerate current state from the old PCB snapshot.')
    check=read('verification.json');pcb=read('pcb_sync_verification.json');visual=read('visual_check.json');rev=read('m28_revision.json');proc=read('procurement_m28.json')
    assert check['revision']=='M2.8' and check['pass'] and pcb['pass']
    assert visual['pages']==10 and not visual['wire_text_intersections'] and not visual['text_text_overlaps']
    tree=ET.parse(DOC/'netlist.xml');comps=tree.findall('.//components/comp');rows=[]
    byref={ref:p for p in proc['parts'] for ref in p['references']};bympn={p['mpn']:p for p in proc['parts']}
    for c in sorted(comps,key=lambda c:c.get('ref')):
        ref=c.get('ref')
        if ref in ['H1','H2']:continue
        f={p.get('name'):p.text or '' for p in c.findall('fields/field')};p=byref.get(ref) or bympn.get(f.get('MPN'))
        rows.append(dict(reference=ref,value=c.findtext('value'),mpn=f.get('MPN',''),lcsc=f.get('LCSC',''),tolerance=f.get('Tolerance',''),footprint=c.findtext('footprint'),datasheet=c.findtext('datasheet') or '',sheet=c.find('sheetpath').get('names'),purchase_url=p['url'] if p else '',stock_checked='2026-11-07' if p else '',stock=p['stock'] if p else '',price_cny_default_tier=p['price_cny'] if p else '',procurement_status='domestic in-stock snapshot; recheck before ordering' if p else 'legacy selection; current domestic availability not audited',status='M2.8 schematic/PCB component draft; routing DRC FAIL; physical untested; not production BOM'))
    for name in ['BOM_M2_SCHEMATIC_DRAFT.csv','BOM.csv']:
        with (DOC/name).open('w',encoding='utf-8-sig',newline='') as out:
            w=csv.DictWriter(out,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
    actual=list(csv.DictReader((DOC/'BOM_M2_SCHEMATIC_DRAFT.csv').open(encoding='utf-8-sig')))
    errors=[]
    if {r['reference'] for r in actual}!={c.get('ref') for c in comps}-{'H1','H2'}:errors.append('reference set mismatch')
    for r in actual:
        c=next(c for c in comps if c.get('ref')==r['reference']);fields={p.get('name'):p.text or '' for p in c.findall('fields/field')}
        if (r['value'],r['footprint'],r['mpn'],r['lcsc'])!=(c.findtext('value'),c.findtext('footprint'),fields.get('MPN',''),fields.get('LCSC','')):errors.append(r['reference']+' mismatch')
    write('bom_verification.json',dict(date='2026-11-07',revision='M2.8',source='fresh native netlist; H1/H2 excluded',native_netlist_sha256=sha(DOC/'netlist.xml'),physical_bom_references=178,csv_rows=len(actual),errors=errors,csv_sha256=sha(DOC/'BOM_M2_SCHEMATIC_DRAFT.csv'),pass_=not errors))
    b=read('bom_verification.json');b['pass']=b.pop('pass_');write('bom_verification.json',b)
    state=dict(date='2026-11-07',revision='M2.8',source_commit=check['source_commit'],working_tree_edits=True,schematic_pages=10,paper=rev['paper'],native_nets=check['native_nets'],checks=check['checks'],native_ERC_errors=check['native_erc_errors'],native_ERC_warnings=check['native_erc_warnings'],physical_references=180,electrical_bom_references=178,model_bound_references=check['model_bound_references'],unbound_models=check['unbound_models'],generic_model_references=check['generic_model_references'],missing_exact_models=check['missing_exact_models'],source_sha256=check['artifacts_sha256'],pdf_sha256=visual['pdf_sha256'],gpio_roles=40,authoritative_interfaces='m2_pin_assignment.csv +98-contact audit +native schematic',baseline=check['baseline'],pcb_updated=True,pcb_sha256=pcb['pcb_sha256'],pcb_sync_checks=pcb['checks'],pcb_schematic_parity=pcb['schematic_parity'],pcb_drc_violations=pcb['drc_violations'],pcb_unconnected_items=pcb['unconnected_items'],pcb_retained_segments=151,pcb_retained_vias=3,pcb_new_staged=33,power_conversion='Two AP64500SP-13 5A-rated converters, nominal5.088V; A4.2A Lichee/Core/gray/TFT-BT; B4A Maix/Action4; ratings/budgets unmeasured',power_budget=rev['power_budget'],current_revision_records=['m28_revision.json','m28_power_analysis.json','procurement_m28.json','pcb_sync_m28.json','verification.json','pcb_sync_verification.json','visual_check.json'],recognition='MaixCAM2 UART4 viaJ19; Lichee application division not implemented',lichee='J25 Type-C fixed5V3A source; J18 original2x6 pin order, UART1 RX3.3V/TX1.8V toSTM32 USART3 PD8/PD9',streaming='DJI Action4 J28 fixed5V3A; no MCU data or PD/PPS',stepper='ZDT_XS TTL J23 ordinary XH8P,1/2/7/8 active; PD12/B30 active high, hardware default off',bluetooth_supply='fixed3V3_EXT via AP2112K; no tie to Core3V3',gray='J20 CH1-8; J21 GND/5V',removed_interfaces=['J3','J5','J22','J24','J26'],BOM_rows=len(rows)+1,procurement_new_and_revised_skus=len(proc['parts']),procurement_unresolved=proc['unresolved'],legacy_procurement_not_audited=True,validation='Documentation/validation/2026-11-07-m28-resume.md',production_release=False,firmware_modified=False,physical_validation=False,limitations=check['limitations']+pcb['limitations'])
    state['metadata_updated_at']=datetime.now().astimezone().isoformat(timespec='seconds')
    write('current_design.json',state)
    # These are historical authoring manifests, not current native inventories.
    for name in ['design_manifest.json','m2_design.json']:
        r=read(name);r.update(current_revision='M2.8',historical_authoring_manifest=True,record_scope='Historical authoring data retained. Current inventory/checks/BOM: current_design.json, m28_revision.json and fresh native netlist; never rerun old generators.',current_revision_records=state['current_revision_records']);write(name,r)
    if errors:raise ValueError(errors)
    print('M2.8 current metadata and two draft BOM aliases refreshed:',len(rows),'electrical references;',len(proc['parts']),'verified new/revised purchase SKUs')
if __name__=='__main__':main()
