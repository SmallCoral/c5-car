"""Reuse the lower-layer protrusion identified in the supplied assembly."""
from pathlib import Path
import hashlib
import json
import FreeCAD as App
import Part

M=Path(__file__).resolve().parents[1]
given=Path(r'C:/Users/sc/Downloads/EdgeMind_Assembly.step')
source=Path(r'C:/Users/sc/Documents/Dev/Hardware/EdgeMind-Robot/model/chassis/v5-rgb-touch-display')
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(given)==sha(source/'EdgeMind_Assembly.step')
doc=App.openDocument(str(source/'EdgeMind_Chassis.FCStd'))
tray=doc.getObject('LowerTray').Shape.copy()
floor=62.7811671820691
bay=tray.common(Part.makeBox(53,91,3,App.Vector(-26.5,-10.5,floor)))
bay.translate(App.Vector(0,-35,-floor))
bay.exportBrep(str(M/'reference/reused-hardware/BatteryBay.brep'))
data={'date':'2026-11-05','supplied_step':str(given),'supplied_sha256':sha(given),
      'matches_native_companion_export':True,'native_source':str(source/'EdgeMind_Chassis.FCStd'),
      'native_sha256':sha(source/'EdgeMind_Chassis.FCStd'),'object':'LowerTray',
      'extraction_source_box_mm':[-26.5,26.5,-10.5,80.5,floor,floor+3],
      'outer_mm':[53,91,3],'inner_xy_mm':[49,87],
      'interpretation':'The identified protrusion is a battery locating rim integral with the bottom tray, not a separate cell solid. Reuse the exact rim; cell height stays unmeasured.',
      'volume_mm3':bay.Volume,'valid':bay.isValid()}
(M/'reference/reused-hardware/battery-bay-source.json').write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
p=M/'parameters.yaml';cfg=json.loads(p.read_text(encoding='utf-8'))
cfg['electronics']['motor_board_center_xy']=[43,0]
cfg['electronics']['board_bottom']=cfg['chassis']['plate_bottom']+cfg['chassis']['thickness']+10
cfg['beam']['stepper_center_xz'][1]=cfg['chassis']['plate_bottom']+cfg['chassis']['thickness']+56.5
cfg['battery']={'rim_outer_reference':[91,53,3],'cell_xy_from_rim':[87,49],'height_placeholder':25,'center_xy':[-57,25],'source':'reference/reused-hardware/battery-bay-source.json'}
cfg['assumptions']['not_measured'].append('Battery height is 25 mm only for layout; its 87 x 49 mm footprint follows the supplied locating rim.')
p.write_text(json.dumps(cfg,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
App.closeDocument(doc.Name)
print(json.dumps(data,ensure_ascii=False))
