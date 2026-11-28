"""检查两只已放置的 XT60 及其本地资源，无需编辑板图。"""
from pathlib import Path
import hashlib
import json
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT.parent / 'Shared/design_tools'))
from sexpr import parse, node, nodes, prop


# 检查两只 XT60 的连通、极性和本地资源，写出核对结果。
def main():
    sch = parse((ROOT / 'Transfer.kicad_sch').read_text(encoding='utf-8'))
    library = parse((ROOT / 'Transfer.kicad_sym').read_text(encoding='utf-8'))
    resources = json.loads((ROOT / 'docs/xt60-resources.json').read_text(encoding='utf-8'))
    netlist = ET.parse(ROOT / 'docs/Transfer.net.xml').getroot()
    erc = json.loads((ROOT / 'docs/Transfer.erc.json').read_text(encoding='utf-8'))
    checks = []

    # 登记每项核对的名称和是否通过。
    def check(name, passed):
        checks.append({'name': name, 'passed': bool(passed)})

    instances = nodes(sch, 'symbol')
    check('exactly J1 and J2', sorted(prop(s, 'Reference')[2] for s in instances) == ['J1', 'J2'])
    check('original sheet UUID preserved', node(sch, 'uuid')[1] == '70d16d07-c26c-4e1c-a3ff-32a8766b85e2')
    check('placement only; no wires or artificial NC flags', not nodes(sch, 'wire') and not nodes(sch, 'no_connect'))
    check('local libraries registered', '${KIPRJMOD}/Transfer.kicad_sym' in (ROOT / 'sym-lib-table').read_text()
          and '${KIPRJMOD}/Transfer.pretty' in (ROOT / 'fp-lib-table').read_text())
    for part in resources['parts']:
        ref, mpn = part['reference'], part['mpn']
        inst = next(s for s in instances if prop(s, 'Reference')[2] == ref)
        sym = next(s for s in nodes(library, 'symbol') if s[1] == mpn)
        embedded = next(s for s in nodes(node(sch, 'lib_symbols'), 'symbol') if s[1] == 'Transfer:' + mpn)
        check(ref + ' embedded and local symbol agree', embedded[2:] == sym[2:])
        pins = [p for unit in nodes(sym, 'symbol') for p in nodes(unit, 'pin')]
        check(ref + ' explicit polarity 1 negative / 2 positive',
              {node(p, 'number')[1]: node(p, 'name')[1] for p in pins} == {'1': '-', '2': '+'})
        check(ref + ' footprint property matches local symbol', prop(inst, 'Footprint')[2] == prop(sym, 'Footprint')[2] == part['footprint'])
        fp = parse((ROOT / 'Transfer.pretty' / (part['footprint'].split(':')[1] + '.kicad_mod')).read_text(encoding='utf-8'))
        pads = {p[1]: p for p in nodes(fp, 'pad') if p[1]}
        check(ref + ' two electrical pads', set(pads) == {'1', '2'})
        direction = 1 if mpn.endswith('-M') else -1
        check(ref + ' correct gender and 7.2mm pitch', [float(v) for v in node(pads['1'], 'at')[1:]] == [0, 0]
              and [float(v) for v in node(pads['2'], 'at')[1:]] == [direction * 7.2, 0])
        check(ref + ' 2.7mm drills', all(float(node(p, 'drill')[1]) == 2.7 for p in pads.values()))
        check(ref + ' two unnumbered retaining tabs', len([p for p in nodes(fp, 'pad') if not p[1]]) == 2)
        model = node(fp, 'model')
        check(ref + ' portable STEP binding', model[1] == '${KIPRJMOD}/Models/' + part['model'])
        check(ref + ' model offset and rotation', [float(v) for v in node(node(model, 'offset'), 'xyz')[1:]] == part['offset']
              and [float(v) for v in node(node(model, 'rotate'), 'xyz')[1:]] == part['rotation'])
        data = (ROOT / 'Models' / part['model']).read_bytes()
        check(ref + ' source STEP bytes preserved', data.startswith(b'ISO-10303-21;') and b'END-ISO-10303-21;' in data
              and hashlib.sha256(data).hexdigest() == part['model_sha256'])
        comp = netlist.find("./components/comp[@ref='" + ref + "']")
        check(ref + ' native netlist footprint', comp is not None and comp.findtext('footprint') == part['footprint'])
    check('native netlist has exactly two components', len(netlist.findall('./components/comp')) == 2)
    violations = [v for sheet in erc['sheets'] for v in sheet['violations']]
    check('ERC has only the four intentionally unwired pins', len(violations) == 4 and all(v['type'] == 'pin_not_connected' for v in violations))
    result = {'date': '2026-11-14', 'source_commit': resources['source_commit'], 'kicad_version': erc['kicad_version'],
              'commands_from_project_directory': [
                  'kicad-cli sch export netlist --format kicadxml -o docs/Transfer.net.xml Transfer.kicad_sch',
                  'kicad-cli sch erc --format json --exit-code-violations -o docs/Transfer.erc.json Transfer.kicad_sch',
                  'kicad-cli sch export pdf -o docs/Transfer.pdf Transfer.kicad_sch',
                  'python tools/verify_schematic.py'],
              'command_exit_codes': [0, 1, 0, 0 if all(c['passed'] for c in checks) else 1],
              'passed': all(c['passed'] for c in checks),
              'checks': checks, 'footprint_bindings': 2, 'model_bindings': 2,
              'erc_passed': False, 'erc_errors': len(violations), 'erc_reason': 'Placement only; J1/J2 pins are not wired.',
              'limitations': ['No board/DRC or physical fit verification.', 'Domestic live inventory could not be confirmed.']}
    (ROOT / 'docs/verification.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'resource_checks_passed': result['passed'], 'checks': len(checks), 'failures': [c for c in checks if not c['passed']],
                      'footprints': 2, 'models': 2, 'erc_errors': len(violations)}, ensure_ascii=False))
    return 0 if result['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
