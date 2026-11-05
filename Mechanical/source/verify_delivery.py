"""Check delivered formats, local documentation links and the saved input baseline."""
from pathlib import Path
import argparse
import hashlib
import json
import math
import re
import struct
from urllib.parse import unquote

M=Path(__file__).resolve().parents[1]
ROOT=M.parent
parser=argparse.ArgumentParser()
parser.add_argument('--baseline',type=Path,required=True)
args=parser.parse_args()
baseline=json.loads(args.baseline.read_text(encoding='utf-8'))
protected=[(name,digest) for name,digest in baseline['files'].items()
           if name.startswith(('Hardware/','Firmware/'))]
changed=[name for name,digest in protected if not (ROOT/name).is_file() or
         hashlib.sha256((ROOT/name).read_bytes()).hexdigest()!=digest]

glb=(M/'step/CoralC5_Framework.glb').read_bytes()
magic,version,length=struct.unpack_from('<4sII',glb)
json_length,json_type=struct.unpack_from('<I4s',glb,12)
gltf=json.loads(glb[20:20+json_length])
expected=len(json.loads((M/'preview/assembly-meshes.json').read_text(encoding='utf-8'))['parts'])
binary_length,binary_type=struct.unpack_from('<I4s',glb,20+json_length)
format_ok=(magic==b'glTF' and version==2 and length==len(glb) and
           json_type==b'JSON' and binary_type==b'BIN\0' and
           28+json_length+binary_length==len(glb) and
           len(gltf['meshes'])==expected and len(gltf['nodes'])==expected+1)
binary_start=28+json_length
for view in gltf['bufferViews']:
    format_ok &= view.get('byteOffset',0)+view['byteLength']<=binary_length
for node in gltf['nodes']:
    format_ok &= all(math.isfinite(x) for x in node.get('matrix',[]))
for mesh in gltf['meshes']:
    primitive=mesh['primitives'][0]
    indices=gltf['accessors'][primitive['indices']]
    vertices=gltf['accessors'][primitive['attributes']['POSITION']]['count']
    view=gltf['bufferViews'][indices['bufferView']]
    values=struct.unpack_from('<'+str(indices['count'])+'I',glb,binary_start+view['byteOffset'])
    format_ok &= bool(values) and max(values)<vertices

docs=['README.md','Mechanical/README.md','Mechanical/reference/README.md',
      'Documentation/README.md','Documentation/reference/pending-hardware-changes.md',
      'Documentation/reference/cameras.md','Documentation/reference/parts-selection.md',
      'Documentation/explanation/a-system-outline.md',
      'Documentation/explanation/mechanical-outline.md',
      'Documentation/validation/2026-11-05-mechanical-framework.md']
missing=[]
for name in docs:
    file=ROOT/name
    for target in re.findall(r'\]\(([^)]+)\)',file.read_text(encoding='utf-8')):
        target=unquote(target.strip('<>').split('#')[0])
        if not target or '://' in target or re.match(r'^[A-Za-z]:',target):continue
        resolved=(file.parent/target).resolve()
        if resolved==M/'validation/delivery-check.json':continue  # This invocation writes it below.
        if not resolved.exists():missing.append({'file':name,'target':target})

artifacts=['source/CoralC5_Framework.FCStd','step/CoralC5_Framework.step',
           'step/CoralC5_Framework.glb','preview/查看模型.html','preview/preview-isometric.png']
report={'date':'2026-11-05','source_revision':baseline['source_revision'],
        'baseline_path':str(args.baseline),'protected_files_checked':len(protected),
        'changed_protected_files':changed,'glb_structure_and_indices_valid':bool(format_ok),
        'missing_local_links':missing,'artifacts':[
            {'file':f,'bytes':(M/f).stat().st_size,'sha256':hashlib.sha256((M/f).read_bytes()).hexdigest()}
            for f in artifacts]}
snapshots=json.loads((M/'validation/pcb-reference-report.json').read_text(encoding='utf-8'))['sources']
report['pcb_export_snapshot_current']=[{'file':name,'matches_export_snapshot':
    hashlib.sha256((ROOT/name).read_bytes()).hexdigest()==digest} for name,digest in snapshots.items()]
report['pass']=not changed and bool(format_ok) and not missing
(M/'validation/delivery-check.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(json.dumps(report,ensure_ascii=False))
if not report['pass']:raise SystemExit(1)
