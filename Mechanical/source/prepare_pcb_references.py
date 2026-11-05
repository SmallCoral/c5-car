"""Read-only KiCad inputs; a temporary Motor copy gets a virtual 99 mm outline."""
import hashlib
import json
import subprocess
import tempfile
from pathlib import Path

import pcbnew

ROOT = Path(__file__).resolve().parents[2]
MECH = ROOT / 'Mechanical'
HW = ROOT / 'Hardware/stm32c5a3/Hardware'
CLI = Path(r'C:/Program Files/KiCad/10.0/bin/kicad-cli.exe')


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def export(board_path, output, origin, component_filter=None):
    cmd = [str(CLI), 'pcb', 'export', 'step', '--force', '--subst-models',
           '--user-origin', origin, '--output', str(output)]
    if component_filter:
        cmd += ['--component-filter', ','.join(component_filter)]
    cmd.append(str(board_path))
    result = subprocess.run(cmd, capture_output=True, encoding='utf-8', errors='replace')
    (MECH / 'validation' / (output.stem + '-export.log')).write_text(
        'COMMAND: ' + subprocess.list2cmdline(cmd) + '\nEXIT_CODE: ' + str(result.returncode)
        + '\n' + result.stdout + '\n' + result.stderr, encoding='utf-8')
    if not output.exists():
        raise RuntimeError(result.stdout + result.stderr)
    return {'exit_code': result.returncode, 'stderr': result.stderr.strip(),
            'output': output.relative_to(ROOT).as_posix(), 'sha256': sha(output)}


def main():
    for d in ('reference', 'validation'):
        (MECH / d).mkdir(exist_ok=True)
    core = HW / 'CoralC5-Core/CoralC5-Core.kicad_pcb'
    motor = HW / 'CoralC5-Motor/CoralC5-Motor.kicad_pcb'
    before = {str(p): sha(p) for p in (core, motor)}
    core_board = pcbnew.LoadBoard(str(core))
    motor_board = pcbnew.LoadBoard(str(motor))
    origin = (132.5, 80.375)
    included, outside = [], []
    for fp in motor_board.GetFootprints():
        p = fp.GetPosition()
        x, y = pcbnew.ToMM(p.x), pcbnew.ToMM(p.y)
        item = {'reference': fp.GetReference(), 'value': fp.GetValue(), 'xy_mm': [x, y]}
        (included if abs(x-origin[0]) <= 49.5 and abs(y-origin[1]) <= 49.5 else outside).append(item)
        models = list(fp.Models())
        fp.Models().clear()
        for model in models:
            model.m_Filename = model.m_Filename.replace('${KIPRJMOD}', motor.parent.as_posix())
            fp.Add3DModel(model)
    for graphic in list(motor_board.GetDrawings()):
        if graphic.GetLayer() == pcbnew.Edge_Cuts:
            motor_board.Remove(graphic)
    corners = [(83,30.875),(182,30.875),(182,129.875),(83,129.875)]
    for p, q in zip(corners, corners[1:]+corners[:1]):
        edge = pcbnew.PCB_SHAPE(motor_board)
        edge.SetShape(pcbnew.SHAPE_T_SEGMENT)
        edge.SetStart(pcbnew.VECTOR2I(pcbnew.FromMM(p[0]),pcbnew.FromMM(p[1])))
        edge.SetEnd(pcbnew.VECTOR2I(pcbnew.FromMM(q[0]),pcbnew.FromMM(q[1])))
        edge.SetLayer(pcbnew.Edge_Cuts)
        edge.SetWidth(pcbnew.FromMM(0.05))
        motor_board.Add(edge)
    results = {}
    with tempfile.TemporaryDirectory(prefix='coralc5-mechanical-') as folder:
        virtual = Path(folder) / 'CoralC5-Motor99-VIRTUAL-NOT-FOR-FAB.kicad_pcb'
        pcbnew.SaveBoard(str(virtual), motor_board)
        results['core'] = export(core, MECH/'reference/CoralC5-Core-current.step', '90.95x85.5mm')
        results['motor99'] = export(virtual, MECH/'reference/CoralC5-Motor99-planning.step',
                                    '132.5x80.375mm', [p['reference'] for p in included])
    pads = []
    for fp in core_board.GetFootprints():
        if fp.GetReference() != 'J13':
            continue
        for pad in fp.Pads():
            pads.append({'number':pad.GetNumber(),
                         'xy':[pcbnew.ToMM(pad.GetPosition().x),pcbnew.ToMM(pad.GetPosition().y)],
                         'size':[pcbnew.ToMM(pad.GetSize().x),pcbnew.ToMM(pad.GetSize().y)],
                         'front':pad.IsOnLayer(pcbnew.F_Cu), 'back':pad.IsOnLayer(pcbnew.B_Cu)})
    (MECH/'reference/core-edge-pads.json').write_text(json.dumps({'pads':pads},ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    report = {'date':'2026-11-05','kicad_version':pcbnew.GetBuildVersion(),
              'sources':{str(p.relative_to(ROOT)): before[str(p)] for p in (core,motor)},
              'original_boards_unchanged':all(sha(p)==before[str(p)] for p in (core,motor)),
              'virtual_motor_board_mm':[99,99], 'virtual_origin_pcb_mm':origin,
              'included_parts':included,'outside_parts_not_relocated':outside,
              'exports':results,
              'limitations':['Motor outline is a mechanical planning copy only.',
                            'Outlying layout components are omitted, never moved into the real board.',
                            'KiCad may omit VRML-only models; nonzero exporter status is retained.']}
    (MECH/'validation/pcb-reference-report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'included_parts':len(included),'outside_parts':len(outside),
                      'unchanged':report['original_boards_unchanged'],
                      'export_status':{k:v['exit_code'] for k,v in results.items()}},ensure_ascii=False))


if __name__ == '__main__':
    main()
