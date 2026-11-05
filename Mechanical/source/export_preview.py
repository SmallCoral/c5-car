"""Create a colored GLB, native appearance records, and a self-contained viewer."""
from pathlib import Path
import json
import struct
import shutil
import zipfile
import xml.etree.ElementTree as ET
import numpy as np

M=Path(__file__).resolve().parents[1]
data=json.loads((M/'preview/assembly-meshes.json').read_text(encoding='utf-8'))
parts=data['parts']
gltf={'asset':{'version':'2.0','generator':'Coral-C5 FreeCAD framework exporter'},
      'scene':0,'scenes':[{'nodes':[0]}],
      'nodes':[{'name':'Coral-C5 mm to metres / Z-up to Y-up',
                'matrix':[.001,0,0,0,0,0,-.001,0,0,.001,0,0,0,0,0,1],'children':[]}],
      'meshes':[],'materials':[],'buffers':[{'byteLength':0}],
      'bufferViews':[],'accessors':[]}
binary=bytearray()


def accessor(array,kind,component,target,bounds=False):
    while len(binary)%4:binary.append(0)
    a=np.ascontiguousarray(array)
    view=len(gltf['bufferViews'])
    gltf['bufferViews'].append({'buffer':0,'byteOffset':len(binary),'byteLength':a.nbytes,'target':target})
    binary.extend(a.tobytes())
    item={'bufferView':view,'componentType':component,'count':len(a),'type':kind}
    if bounds:item.update({'min':a.min(axis=0).astype(float).tolist(),'max':a.max(axis=0).astype(float).tolist()})
    idx=len(gltf['accessors']);gltf['accessors'].append(item);return idx


for p in parts:
    verts=np.asarray(p['vertices'],dtype='<f4');tri=np.asarray(p['triangles'],dtype='<u4')
    if len(verts)==0 or len(tri)==0:continue
    normals=np.zeros_like(verts)
    face=np.cross(verts[tri[:,1]]-verts[tri[:,0]],verts[tri[:,2]]-verts[tri[:,0]])
    for k in range(3):np.add.at(normals,tri[:,k],face)
    lengths=np.linalg.norm(normals,axis=1);lengths[lengths==0]=1;normals/=lengths[:,None]
    pos=accessor(verts,'VEC3',5126,34962,True)
    nor=accessor(normals,'VEC3',5126,34962)
    ind=accessor(tri.flatten(),'SCALAR',5125,34963)
    mat={'name':p['name'],'doubleSided':False,'pbrMetallicRoughness':{'baseColorFactor':p['color']+[p['opacity']],
          'metallicFactor':.55 if p['color'][0]>.5 and p['color'][1]>.5 else .12,'roughnessFactor':.48}}
    if p['opacity']<1:mat['alphaMode']='BLEND'
    gltf['materials'].append(mat)
    mi=len(gltf['meshes']);gltf['meshes'].append({'name':p['label'],'primitives':[{'attributes':{'POSITION':pos,'NORMAL':nor},'indices':ind,'material':mi}]})
    m=p['placement'];matrix=[m[r*4+c] for c in range(4) for r in range(4)]
    ni=len(gltf['nodes']);gltf['nodes'].append({'name':p['label'],'mesh':mi,'matrix':matrix,'extras':{'reference_status':p['role']}})
    gltf['nodes'][0]['children'].append(ni)
gltf['buffers'][0]['byteLength']=len(binary)
js=json.dumps(gltf,ensure_ascii=False,separators=(',',':')).encode('utf-8')
js+=b' '*((-len(js))%4);binary.extend(b'\0'*((-len(binary))%4))
glb=struct.pack('<4sII',b'glTF',2,12+8+len(js)+8+len(binary))+struct.pack('<I4s',len(js),b'JSON')+js+struct.pack('<I4s',len(binary),b'BIN\0')+binary
(M/'step/CoralC5_Framework.glb').write_bytes(glb)

# Native appearance only; BRep files and expression-based placements are unchanged.
native=M/'source/CoralC5_Framework.FCStd';tmp=native.with_suffix('.styled.FCStd')
gui=ET.Element('Document',{'SchemaVersion':'1','HasExpansion':'1'});providers=ET.SubElement(gui,'ViewProviderData',{'Count':str(len(parts))})
for p in parts:
    vp=ET.SubElement(providers,'ViewProvider',{'name':p['name'],'expanded':'0'});props=ET.SubElement(vp,'Properties',{'Count':'5'})
    for field,col in (('ShapeColor',p['color']),('LineColor',[.08,.10,.12])):
        prop=ET.SubElement(props,'Property',{'name':field,'type':'App::PropertyColor'})
        val=(round(col[0]*255)<<24)|(round(col[1]*255)<<16)|(round(col[2]*255)<<8)|255
        ET.SubElement(prop,'PropertyColor',{'value':str(val)})
    prop=ET.SubElement(props,'Property',{'name':'Visibility','type':'App::PropertyBool'});ET.SubElement(prop,'Bool',{'value':'true'})
    prop=ET.SubElement(props,'Property',{'name':'DisplayMode','type':'App::PropertyEnumeration'});ET.SubElement(prop,'Integer',{'value':'0'})
    prop=ET.SubElement(props,'Property',{'name':'Transparency','type':'App::PropertyPercent'});ET.SubElement(prop,'Integer',{'value':str(round(100*(1-p['opacity'])))})
with zipfile.ZipFile(native) as src,zipfile.ZipFile(tmp,'w',zipfile.ZIP_DEFLATED) as dst:
    for item in src.infolist():
        if item.filename!='GuiDocument.xml':dst.writestr(item,src.read(item.filename))
    dst.writestr('GuiDocument.xml',ET.tostring(gui,encoding='utf-8',xml_declaration=True))
tmp.replace(native)

vendor=M/'preview/vendor';vendor.mkdir(exist_ok=True)
old=Path(r'C:/Users/sc/Documents/Dev/Hardware/EdgeMind-Robot/model/chassis/v5-rgb-touch-display/vendor')
for filename in ('three.min.js','three.LICENSE.txt'):shutil.copy2(old/filename,vendor/filename)
template=(M/'source/viewer_template.html').read_text(encoding='utf-8')
(M/'preview/查看模型.html').write_text(template.replace('__MODEL_DATA__',json.dumps(data,ensure_ascii=False,separators=(',',':'))),encoding='utf-8')
print(json.dumps({'glb_bytes':len(glb),'viewer_parts':len(parts),'triangles':sum(len(p['triangles']) for p in parts)},ensure_ascii=False))
