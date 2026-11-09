"""Trace every module signal through Core to the LQFP100 pin and ST AF cell."""
from pathlib import Path
import csv
import json
import re
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
DOC = ROOT / 'docs'
SHARED = ROOT.parent / 'Shared'
RESERVED = {'PA9', 'PA11', 'PA12', 'PA13', 'PA14', 'PH2', 'PE3', 'PD2'}


def pin_groups(path):
    result = {}
    for net in ET.parse(path).findall('.//nets/net'):
        members = {n.get('ref') + ':' + n.get('pin') for n in net.findall('node')}
        for member in members:
            result[member] = members
    return result


def audit():
    plan = json.loads((DOC / 'layout-pinplan.json').read_text(encoding='utf8'))['plan']
    primary = json.loads((DOC / 'sources/pinplan-st-table14.json').read_text(encoding='utf8'))
    contract = {r['contact']: r for r in csv.DictReader((SHARED / 'interface.csv').open(encoding='utf-8-sig'))}
    motor = pin_groups(DOC / 'netlist.xml')
    core = pin_groups(DOC / 'core_netlist.xml')
    text = (SHARED / 'sources/DS15137_STM32C5Axxx_Datasheet_Rev3.pdf.txt').read_text(encoding='utf8')
    checks = []
    rows = []

    def ck(name, ok, detail=None):
        checks.append({'name': name, 'pass': bool(ok), 'detail': detail})

    ck('39 unique module signals, contacts and GPIOs', len(plan) == len({r['signal'] for r in plan}) == len({r['contact'] for r in plan}) == len({r['gpio'] for r in plan}) == 39)
    ck('Core USB, SWD, boot and board functions reserved', not RESERVED & {r['gpio'] for r in plan})
    for contact, c in contract.items():
        if c['gpio'] and c['mcu_pin']:
            ck(contact + ' fixed package pin matches DS15137 Table13 LQFP100', primary['lqfp100_pins_from_local_rev3_table13'].get(c['gpio']) == int(c['mcu_pin']))
            ck(contact + ' Core native net reaches actual MCU pin', 'U1:' + c['mcu_pin'] in core.get('J13:' + contact, set()))
    for r in plan:
        c = contract[r['contact']]
        prefix = r['signal'] + ': '
        ck(prefix + 'contact maps to selected GPIO', c['gpio'] == r['gpio'])
        ck(prefix + 'native Motor net reaches module endpoint', r['endpoint'] in motor.get('J14:' + r['contact'], set()))
        # Detect a GPIO already loaded by a Core circuit, beyond just the printed contact label.
        ck(prefix + 'Core GPIO is a dedicated passthrough', core.get('J13:' + r['contact']) == {'J13:' + r['contact'], 'U1:' + c['mcu_pin']})
        cell = ''
        if r['af'] is not None:
            cell = primary['af_cells'][r['gpio']][r['af']]
            ck(prefix + 'function occurs in independent ST HTML AF cell', re.search(r'(?<![A-Z0-9_])' + re.escape(r['function']) + r'(?![A-Z0-9_])', cell) is not None, {'gpio': r['gpio'], 'af': r['af'], 'cell': cell})
        if r['function'].startswith('ADC'):
            blocks = [b for b in text.split('\n\n') if re.search(r'\b' + r['gpio'] + r'\b[^\n]*\bI/O\b', b)]
            ck(prefix + 'ADC instance and channel occur in Table13', any(re.search(r'\b' + r['function'] + r'\b', b) for b in blocks))
        if r['function'].startswith('EXTI'):
            ck(prefix + 'EXTI line equals GPIO pin index', r['function'] == 'EXTI' + re.search(r'\d+', r['gpio'])[0])
        rows.append({'signal': r['signal'], 'endpoint': r['endpoint'], 'contact': r['contact'], 'gpio': r['gpio'], 'mcu_lqfp100_pin': c['mcu_pin'], 'function': r['function'], 'af': r['af'], 'st_table14_cell': cell})
    by_name = {r['signal']: r for r in plan}
    serials = []
    for name, tx, rx in [('Bluetooth', 'BT_TX', 'BT_RX'), ('MaixCAM2', 'VIDEO_TX', 'VIDEO_RX'), ('stepper', 'STEP_UART_TX', 'STEP_UART_RX'), ('Lichee', 'LICHEE_TX', 'LICHEE_RX')]:
        a, b = by_name[tx]['function'], by_name[rx]['function']
        ck(name + ' has matching hardware TX/RX on one peripheral', a.endswith('_TX') and b == a[:-2] + 'RX')
        serials.append(a.split('_')[0])
    ck('four serial devices use independent peripherals', len(set(serials)) == 4)
    sck, mosi = by_name['TFT_SCK']['function'], by_name['TFT_MOSI']['function']
    ck('TFT clock and MOSI use the same SPI peripheral', sck.endswith('_SCK') and mosi == sck[:-3] + 'MOSI')
    counters = []
    for side in ['L', 'R']:
        functions = [by_name[side + '_ENC_' + phase]['function'] for phase in ['A', 'B']]
        parsed = [re.fullmatch(r'(TIM[123458])_CH([12])', f) for f in functions]
        ok = all(parsed) and parsed[0][1] == parsed[1][1] and {p[2] for p in parsed} == {'1', '2'}
        ck(side + ' encoder uses CH1/CH2 of an encoder-capable timer', ok)
        if ok:
            counters.append(parsed[0][1])
    pwm = [by_name[side + '_PWM']['function'] for side in ['L', 'R']]
    ck('wheel PWM uses separate channels of one timer', pwm[0].split('_')[0] == pwm[1].split('_')[0] and pwm[0] != pwm[1])
    counters.extend([pwm[0].split('_')[0], by_name['TFT_BL']['function'].split('_')[0]])
    ck('both encoders, wheel PWM and backlight have independent counters', len(set(counters)) == 4)
    ck('motor fault EXTI lines do not conflict', by_name['L_nFAULT']['function'] != by_name['R_nFAULT']['function'])
    ck('eight grayscale inputs stay on PE7 through PE14', [by_name['GRAY_CH' + str(i)]['gpio'] for i in range(1, 9)] == ['PE' + str(i) for i in range(7, 15)])
    report = {'date': '2026-11-08', 'pass': all(c['pass'] for c in checks), 'checks': len(checks), 'failed': [c for c in checks if not c['pass']], 'source': primary['source_url'], 'source_sha256': primary['source_sha256'], 'rows': rows, 'results': checks, 'scope': 'Schematic functional pinout only; firmware setup, PCB pad synchronization, copper routing, DRC and hardware operation are separate.'}
    return report


def main():
    report = audit()
    (DOC / 'functional-pinout-verification.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf8')
    with (DOC / 'functional-pinout.csv').open('w', encoding='utf-8-sig', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=list(report['rows'][0]))
        writer.writeheader()
        writer.writerows(report['rows'])
    print(json.dumps({k: v for k, v in report.items() if k not in {'results', 'rows'}}, ensure_ascii=False, indent=2))
    return 0 if report['pass'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
