"""Check local PCB model resources and downloaded-file provenance, without editing CAD."""
from pathlib import Path
import argparse
import hashlib
import json
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT.parent / 'Shared/design_tools'))
from sexpr import parse, node, nodes, prop


def sha(data):
    return hashlib.sha256(data).hexdigest()


def without_models(tree):
    if not isinstance(tree, list):
        return tree
    return [without_models(v) for v in tree
            if not (isinstance(v, list) and v and v[0] == 'model')]


def canonical(tree):
    return json.dumps(tree, ensure_ascii=False, separators=(',', ':')).encode('utf8')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', type=Path, help='Saved pre-edit PCB; checks every non-model field')
    parser.add_argument('--baseline-library', type=Path, help='Saved pre-edit Motor.pretty directory')
    parser.add_argument('--output', type=Path, default=ROOT / 'docs/models-2026-11-09-check.json')
    args = parser.parse_args()
    board_path = ROOT / 'CoralC5-Motor.kicad_pcb'
    raw = board_path.read_bytes()
    board = parse(raw.decode('utf8'))
    catalog = json.loads((ROOT / 'docs/models-2026-11-09.json').read_text(encoding='utf8'))
    checks = []

    def check(name, passed, detail=None):
        checks.append(dict(name=name, passed=bool(passed), detail=detail))

    holes = []
    rows = []
    for fp in nodes(board, 'footprint'):
        ref = prop(fp, 'Reference')[2]
        models = nodes(fp, 'model')
        if fp[1].split(':', 1)[-1].startswith('MountingHole_'):
            holes.append(dict(reference=ref, uuid=node(fp, 'uuid')[1]))
            continue
        check(ref + ' model bound', bool(models))
        for model in models:
            path = model[1]
            resolved = ROOT / path.removeprefix('${KIPRJMOD}/')
            check(ref + ' portable model exists', path.startswith('${KIPRJMOD}/Models/') and resolved.is_file(), path)
        rows.append(dict(reference=ref, footprint=fp[1], models=models))

    for item in catalog['models']:
        path = ROOT / 'Models' / item['file']
        check(item['file'] + ' downloaded bytes unchanged', path.is_file() and sha(path.read_bytes()) == item['sha256'])
        check(item['file'] + ' native STEP header', path.is_file() and path.read_bytes().startswith(b'ISO-10303-21;'))
        for ref in item['refs']:
            matches = [r for r in rows if r['reference'] == ref]
            check(ref + ' unique footprint', len(matches) == 1)
            if len(matches) != 1:
                continue
            row = matches[0]
            library = ROOT / 'Motor.pretty' / (row['footprint'].split(':', 1)[1] + '.kicad_mod')
            library_tree = parse(library.read_text(encoding='utf8'))
            check(ref + ' PCB and library model agree', row['models'] == nodes(library_tree, 'model'))
            check(ref + ' expected downloaded model', len(row['models']) == 1 and row['models'][0][1] == '${KIPRJMOD}/Models/' + item['file'])
            if args.baseline_library:
                original = args.baseline_library / library.name
                check(ref + ' library non-model fields preserved', original.is_file() and without_models(parse(original.read_text(encoding='utf8'))) == without_models(library_tree))

    baseline_hash = None
    if args.baseline:
        baseline_hash = sha(args.baseline.read_bytes())
        check('PCB all non-model fields preserved', without_models(parse(args.baseline.read_text(encoding='utf8'))) == without_models(board))
    result = dict(date='2026-11-09', scope='3D resources only', passed=all(x['passed'] for x in checks),
                  checks=len(checks), failures=[x for x in checks if not x['passed']],
                  footprints=len(nodes(board, 'footprint')), electrical_footprints=len(rows),
                  model_bound_electrical_footprints=sum(bool(x['models']) for x in rows),
                  mechanical_hole_exceptions=holes, downloaded_models=len(catalog['models']),
                  updated_references=[ref for item in catalog['models'] for ref in item['refs']],
                  pcb_sha256=sha(raw), baseline_pcb_sha256=baseline_hash,
                  non_model_data_sha256=sha(canonical(without_models(board))),
                  segments=len(nodes(board, 'segment')), vias=len(nodes(board, 'via')),
                  limitations=['Resource existence and unchanged bytes do not prove mechanical accuracy.',
                               'SW1 and C1/C13/C33 are explicitly identified external package references.',
                               'U1/U6/U10 retain generic SOIC exterior models; exact EP geometry is unresolved.',
                               'Native PCB DRC and physical validation are separate.'], results=checks)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf8')
    print(json.dumps({k:result[k] for k in ('passed','checks','failures','footprints','electrical_footprints','model_bound_electrical_footprints','downloaded_models','updated_references','segments','vias')}, ensure_ascii=False))
    return 0 if result['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
