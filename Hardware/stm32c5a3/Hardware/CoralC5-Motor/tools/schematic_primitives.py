"""Schematic/library primitives only. No generation at import, no PCB access.
Selected definitions extracted once from the pre-M2 authoring utility.
"""
from pathlib import Path
import math,copy,re,shutil,uuid
from sexpr import parse,dump,node,nodes,prop,Atom
A=Atom
K=Path('C:/Program Files/KiCad/10.0/share/kicad')
LIB={}; FPMETA={}; PARTS=[]; PAGES=[]
POWERCOUNT=200; FLAGCOUNT=200
def uid(key): return str(uuid.uuid5(uuid.NAMESPACE_URL,'CoralC5-Motor-M2/'+str(key)))
def n(tag, *items):
    return [A(tag), *items]

def num(v):
    return A(f'{float(v):.5f}'.rstrip('0').rstrip('.') or '0')

def s(v):
    return round(round(float(v) / 1.27) * 1.27, 5)

def pt(x, y):
    return (s(x), s(y))

def xy(tag, x, y, *extra):
    return n(tag, num(x), num(y), *extra)

def effects(size=1.27, justify=None, hide=False):
    e = n('effects', n('font', xy('size', size, size)))
    if justify:
        e.append(n('justify', *[A(z) for z in justify.split()]))
    if hide:
        e.append(n('hide', A('yes')))
    return e

def property_(name, value, x=0, y=0, hide=False, justify=None):
    return n('property', name, value, xy('at', x, y, num(0)), effects(1.27, justify, hide))

def local_fp(source, name=None, source_root=K / 'footprints', model_root=K / '3dmodels', kind='KiCad package geometry'):
    lib, filename = source.split(':')
    name = name or lib + '__' + filename
    src = source_root / (lib + '.pretty') / (filename + '.kicad_mod')
    data = parse(src.read_text(encoding='utf-8'))
    data[1] = name
    models = []
    for model in nodes(data, 'model'):
        original = model[1]
        if original.startswith('${KIPRJMOD}'):
            srcmodel = source_root / original.replace('${KIPRJMOD}/', '')
        elif original.startswith('${KICAD10_3DMODEL_DIR}'):
            srcmodel = model_root / original.replace('${KICAD10_3DMODEL_DIR}/', '')
        else:
            srcmodel = Path(original)
        if not srcmodel.exists():
            raise FileNotFoundError(srcmodel)
        destname = name + '__' + srcmodel.name
        shutil.copy2(srcmodel, md / destname)
        model[1] = '${KIPRJMOD}/Models/' + destname
        models.append(destname)
    (fpdir / (name + '.kicad_mod')).write_text(dump(data) + '\n', encoding='utf-8')
    FPMETA['Motor:' + name] = {'source': str(src), 'models': models, 'model_status': kind}
    return 'Motor:' + name

def std(lib, name):
    key = lib + ':' + name
    if key in LIB:
        return key
    data = parse((K / 'symbols' / (lib + '.kicad_sym')).read_text(encoding='utf-8'))

    def resolve(name):
        x = copy.deepcopy(next((x for x in nodes(data, 'symbol') if x[1] == name)))
        e = node(x, 'extends')
        if e:
            parent = resolve(e[1])
            old = parent[1]
            parent[1] = name
            for sub in nodes(parent, 'symbol'):
                sub[1] = sub[1].replace(old + '_', name + '_')
            for fld in nodes(x, 'property'):
                oldfld = prop(parent, fld[1])
                if oldfld:
                    parent.remove(oldfld)
                parent.append(copy.deepcopy(fld))
            return parent
        return x
    x = resolve(name)
    localname = lib + '__' + name
    old = x[1]
    x[1] = localname
    for sub in nodes(x, 'symbol'):
        sub[1] = sub[1].replace(old + '_', localname + '_')
    LIB['Motor:' + localname] = x
    return 'Motor:' + localname

def chip(name, ref, value, pins, fp, rect=(-12.7, 20.32, 12.7, -20.32), datasheet=''):
    x = n('symbol', name, n('pin_names', n('offset', num(0.5))), n('in_bom', A('yes')), n('on_board', A('yes')), property_('Reference', ref, 0, 25.4), property_('Value', value, 0, 22.86), property_('Footprint', fp, hide=True), property_('Datasheet', datasheet, hide=True))
    b = n('symbol', name + '_0_1', n('rectangle', xy('start', rect[0], rect[1]), xy('end', rect[2], rect[3]), n('stroke', n('width', num(0.254)), n('type', A('default'))), n('fill', n('type', A('background')))))
    u = n('symbol', name + '_1_1')
    for number, pname, px, py, angle, kind in pins:
        u.append(n('pin', A(kind), A('line'), xy('at', px, py, num(angle)), n('length', num(5.08)), n('name', pname, effects(1.016)), n('number', str(number), effects(1.016))))
    x += [b, u]
    LIB['Motor:' + name] = x
    return 'Motor:' + name

class Page:

    def __init__(self, file, title, pageno, paper='A3'):
        self.file = file
        self.title = title
        self.pageno = pageno
        self.uuid = uid(file)
        self.sheetuuid = uid('sheet/' + file)
        self.data = (n('kicad_sch', n('version', num(20260306)), n('generator', 'eeschema'), n('generator_version', '10.0'), n('uuid', self.uuid), n('paper', paper), n('title_block', n('title', title), n('date', '2026-10-27'), n('rev', 'M1'), n('company', 'SmallCoral / CoralC5-Motor'), n('comment', num(1), '原理图设计；实板电流、温升与线束待验证'))),)
        self.data = self.data[0]
        self.used = {}
        self.components = {}
        self.wires = []
        self.labels = []
        self.ncpoints = []
        self.count = 0
        PAGES.append(self)

    def add(self, obj):
        self.data.append(obj)
        return obj

    def wire(self, *points):
        pp = [pt(*p) for p in points]
        for a, b in zip(pp, pp[1:]):
            if a == b:
                continue
            if a[0] != b[0] and a[1] != b[1]:
                corner = (b[0], a[1])
                self.wires.extend([(a, corner), (corner, b)])
            else:
                self.wires.append((a, b))

    def label(self, name, at, angle=0):
        p = pt(*at)
        self.labels.append((name, p))
        self.add(n('label', name, xy('at', *p, num(angle)), effects(1.27, 'left bottom' if angle == 0 else 'right bottom'), n('uuid', uid(f'{self.file}/label/{name}/{p}'))))

    def port(self, name, at, shape='input', angle=0):
        p = pt(*at)
        angle = (angle + 180) % 360
        self.add(n('hierarchical_label', name, n('shape', A(shape)), xy('at', *p, num(angle)), effects(1.27, 'left' if angle == 0 else 'right'), n('uuid', uid(f'{self.file}/port/{name}'))))

    def nc(self, p):
        p = pt(*p)
        self.ncpoints.append(p)
        self.add(n('no_connect', xy('at', *p), n('uuid', uid(f'{self.file}/NC/{p}'))))

    def note(self, text, x, y):
        self.add(n('text', text, xy('at', *pt(x, y), num(0)), effects(2.54, 'left top'), n('uuid', uid(f'{self.file}/note/{text}'))))

    def frame(self, title, x1, y1, x2, y2):
        stroke = n('stroke', n('width', num(0.254)), n('type', A('dash')))
        if node(self.data, 'paper')[1] == 'A4' and x2 > 175.26 and (y2 > 165.1):
            points = [pt(x1, y1), pt(x2, y1), pt(x2, 165.1), pt(175.26, 165.1), pt(175.26, y2), pt(x1, y2), pt(x1, y1)]
            self.add(n('polyline', n('pts', *[xy('xy', *q) for q in points]), stroke, n('fill', n('type', A('none'))), n('uuid', uid(f'{self.file}/frame/{title}'))))
        else:
            self.add(n('rectangle', xy('start', *pt(x1, y1)), xy('end', *pt(x2, y2)), stroke, n('fill', n('type', A('none'))), n('uuid', uid(f'{self.file}/frame/{title}'))))
        self.note(title, x1 + 3, y1 + 2)

    def place(self, key, ref, value, x, y, fp='', rot=0, unit=1, mpn='', datasheet='', fields=None, refpos=None, valpos=None):
        x, y = pt(x, y)
        lib = LIB[key]
        self.used[key] = lib
        self.count += 1
        z = n('symbol', n('lib_id', key), xy('at', x, y, num(rot)), n('unit', num(unit)), n('in_bom', A('no' if ref.startswith('#') else 'yes')), n('on_board', A('no' if ref.startswith('#') else 'yes')), n('dnp', A('no')), n('uuid', uid(f'{self.file}/{ref}/{unit}')))
        simple = key.endswith(('__R', '__C', '__C_Polarized', '__L', '__Fuse'))
        vertical_diode = rot in [90, 270] and key.endswith(('__D', '__D_Zener', '__D_Schottky', '__LED'))
        if refpos is None:
            refpos = (x + 2.54, y - 1.27) if simple and rot == 0 or vertical_diode else (x, y - 7.62)
        if valpos is None:
            valpos = (x + 2.54, y + 1.27) if simple and rot == 0 or vertical_diode else (x, y - 5.08)
        z += [property_('Reference', ref, *refpos, hide=ref.startswith('#'), justify='left' if simple and rot == 0 or vertical_diode else None), property_('Value', value, *valpos, hide=ref.startswith('#'), justify='left' if simple and rot == 0 or vertical_diode else None), property_('Footprint', fp, x, y, hide=True), property_('Datasheet', datasheet or (prop(lib, 'Datasheet')[2] if prop(lib, 'Datasheet') else ''), x, y, hide=True)]
        if mpn:
            z.append(property_('MPN', mpn, x, y, hide=True))
        if rot in [90, 270]:
            for pn in ['Reference', 'Value']:
                node(prop(z, pn), 'at')[3] = num(90)
        for k, v in (fields or {}).items():
            z.append(property_(k, v, x, y, hide=True))
        pins = {}
        for sub in nodes(lib, 'symbol'):
            m = re.search('_(\\d+)_(\\d+)$', sub[1])
            u = int(m.group(1)) if m else 1
            if u not in [0, unit]:
                continue
            for pin in nodes(sub, 'pin'):
                px, py, pa = map(float, node(pin, 'at')[1:4])
                a = math.radians(rot)
                pos = pt(x + px * math.cos(a) - py * math.sin(a), y - (px * math.sin(a) + py * math.cos(a)))
                pins[node(pin, 'number')[1]] = pos
                z.append(n('pin', node(pin, 'number')[1], n('uuid', uid(f"{self.file}/{ref}/{unit}/pin{node(pin, 'number')[1]}"))))
        parent = ROOT_UUID if self.pageno == 1 else ROOT_UUID + '/' + self.sheetuuid
        z.append(n('instances', n('project', 'CoralC5-Motor', n('path', '/' + parent, n('reference', ref), n('unit', num(unit))))))
        self.add(z)
        self.components[ref, unit] = {'pins': pins, 'key': key, 'symbol': z}
        if not ref.startswith('#') and unit == 1:
            PARTS.append({'reference': ref, 'value': value, 'mpn': mpn, 'footprint': fp, 'page': self.pageno, 'datasheet': prop(z, 'Datasheet')[2]})
        return pins

    def gnd(self, p):
        global POWERCOUNT
        POWERCOUNT += 1
        return self.place(std('power', 'GND'), f'#PWR{POWERCOUNT:03}', 'GND', *p, refpos=(s(p[0]), s(p[1])), valpos=(s(p[0]), s(p[1] + 3.81)))

    def flag(self, p):
        global FLAGCOUNT
        FLAGCOUNT += 1
        return self.place(std('power', 'PWR_FLAG'), f'#FLG{FLAGCOUNT:03}', 'PWR_FLAG', *p, refpos=pt(*p), valpos=pt(*p))

    def netpin(self, p, name, dx=-7.62):
        end = (s(p[0] + dx), p[1])
        self.wire(p, end)
        self.label(name, end, 0 if dx < 0 else 180)

    def save(self):
        endpoints = set((p for a, b in self.wires for p in [a, b]))
        segs = set()
        for a, b in self.wires:
            cuts = [p for p in endpoints if a[0] == b[0] == p[0] and min(a[1], b[1]) <= p[1] <= max(a[1], b[1]) or (a[1] == b[1] == p[1] and min(a[0], b[0]) <= p[0] <= max(a[0], b[0]))]
            cuts.sort()
            for aa, bb in zip(cuts, cuts[1:]):
                segs.add((aa, bb))
        degree = {}
        for a, b in sorted(segs):
            self.add(n('wire', n('pts', xy('xy', *a), xy('xy', *b)), n('stroke', n('width', num(0)), n('type', A('default'))), n('uuid', uid(f'{self.file}/wire/{a}/{b}'))))
            for p in (a, b):
                degree[p] = degree.get(p, 0) + 1
        for p, d in degree.items():
            if d >= 3:
                self.add(n('junction', xy('at', *p), n('diameter', num(0)), n('color', num(0), num(0), num(0), num(0)), n('uuid', uid(f'{self.file}/junction/{p}'))))
        embeds = []
        for key, lib in self.used.items():
            x = copy.deepcopy(lib)
            x[1] = key
            embeds.append(x)
        self.data.insert(7, n('lib_symbols', *embeds))
        self.add(n('embedded_fonts', A('no')))
        (ROOT / self.file).write_text(dump(self.data) + '\n', encoding='utf-8')