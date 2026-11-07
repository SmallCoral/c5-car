"""Read-only STEP mesh export for diagram thumbnails. Run with FreeCAD Python.

No PCB or schematic is loaded or modified. Missing exact models are represented
by explicitly illustrative connector geometry, never manufacturing resources.
"""
from pathlib import Path
import json,hashlib
import FreeCAD as App
import Part

ROOT=Path(__file__).resolve().parents[2]
MODELS=ROOT/'Hardware/stm32c5a3/Hardware/CoralC5-Motor/Models'
OUT=ROOT/'.scratch/interface-tree-v2'
OUT.mkdir(parents=True,exist_ok=True)
V=App.Vector
native={
 'xh2':('Connector_JST__JST_XH_B2B-XH-A_1x02_P2.50mm_Vertical.step','white'),
 'xh4':('Connector_JST__JST_XH_B4B-XH-A_1x04_P2.50mm_Vertical__JST_XH_B4B-XH-A_1x04_P2.50mm_Vertical.step','white'),
 'xh6':('JST_XH_B6B-XH-A_1x06_P2.50mm_Vertical.step','white'),
 'xh8':('Connector_JST__JST_XH_B8B-XH-A_1x08_P2.50mm_Vertical.step','white'),
 'female4':('Connector_PinSocket_2.54mm__PinSocket_1x04_P2.54mm_Vertical.step','black'),
 'female8':('Connector_PinSocket_2.54mm__PinSocket_1x08_P2.54mm_Vertical.step','black'),
 'header2':('Connector_PinHeader_2.54mm__PinHeader_1x02_P2.54mm_Vertical__PinHeader_1x02_P2.54mm_Vertical.step','black'),
 'usb_c':('Connector_USB__USB_C_Receptacle_GCT_USB4125-xx-x_6P_TopMnt_Horizontal__USB_C_Receptacle_GCT_USB4125-xx-x_6P_TopMnt_Horizontal.step','usb'),
 'core98':('C5_EDGE_3_Socket__Streamline_Socket.step','black'),
}
models={}
def mesh(shape):
    verts,faces=shape.tessellate(.10)
    return {'v':[[round(a.x,5),round(a.y,5),round(a.z,5)] for a in verts],'f':[list(t) for t in faces]}
for key,(file,kind) in native.items():
    path=MODELS/file;shape=Part.read(str(path));solids=list(shape.Solids)
    if not solids:solids=[shape]
    maximum=max(s.Volume for s in solids)
    items=[]
    for i,s in enumerate(solids):
        body=s.Volume>.2*maximum
        color=([.92,.92,.87] if kind=='white' else [.10,.13,.17]) if body else [.72,.67,.48]
        if kind=='usb':color=[.68,.72,.77] if s.BoundBox.ZLength>1.6 else [.12,.15,.19]
        items.append({**mesh(s),'color':color,'volume':round(s.Volume,3)})
    models[key]={'status':'project STEP model; illustrative material colors','source':str(path.relative_to(ROOT)),'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'parts':items}
    print(key,len(solids),'solids',sum(len(p['f']) for p in items),'triangles',flush=True)

def box(x,y,z,dx,dy,dz):return Part.makeBox(dx,dy,dz,V(x,y,z))
def custom(key,parts,note):models[key]={'status':'illustrative geometry, not exact manufacturer model','note':note,'parts':[{**mesh(s),'color':c} for s,c in parts]}
cream=[.94,.91,.81];silver=[.70,.73,.77];gold=[.83,.67,.29];black=[.11,.14,.18]
# MX1.25: only a body envelope exists in the project. Show side-entry cavity and
# six contacts as an explanatory illustration, based on the connector drawing.
housing=box(-5.25,-2.15,0,10.5,4.3,3.6).cut(box(-4.55,-2.2,.65,9.1,3.35,2.25))
parts=[(housing,cream)]
for x in [-3.125+i*1.25 for i in range(6)]:
    parts.append((box(x-.15,-1.55,.75,.30,2.9,.25),gold))
    parts.append((box(x-.15,1.35,.05,.30,1.5,.30),silver))
for x in [-6.125,4.325]:parts.append((box(x,-1.3,0,1.8,2.5,.28),silver))
custom('mx6',parts,'MX1.25 six-contact low-profile side-entry appearance only; source dimensions from ZX-MX1.25-6PWT drawing Rev2.0.')
# XT60: exact right-angle project model is absent. This yellow two-pole sketch
# conveys connector family only; polarity and keyed shape follow the actual part.
housing=box(-8,-6,0,16,12,8)
for x in [-3.6,3.6]:housing=housing.cut(Part.makeCylinder(2.35,9,V(x,-6.1,4),V(0,1,0)))
parts=[(housing,[.98,.70,.08])]
for x in [-3.6,3.6]:
    parts.append((Part.makeCylinder(1.85,7,V(x,-4.5,4),V(0,1,0)),gold))
    parts.append((Part.makeCylinder(.9,5,V(x,4.0,-3)),silver))
custom('xt60',parts,'Generic yellow two-pole right-angle connector appearance, not an exact XT60PW-M model or polarity drawing.')
parts=[(box(-3,-3,0,6,6,1.5),black),(box(-2.9,-2.9,1.5,5.8,5.8,.35),silver),(Part.makeCylinder(1.7,2.45,V(0,0,1.85)),black)]
for x in [-3.7,2.9]:
    for y in [-2.4,1.8]:parts.append((box(x,y,0,.8,.6,.3),silver))
custom('button',parts,'Generic 6x6x4.3mm four-leg SMD tactile switch; actual unbranded lead span is unverified.')
(OUT/'connector-meshes.json').write_text(json.dumps(models,separators=(',',':')),encoding='utf8')
print('Saved',OUT/'connector-meshes.json',flush=True)
