"""Extract selected reference shapes, retaining source metadata and originals."""
from pathlib import Path
import hashlib
import json
import FreeCAD as App
import Part

MECH = Path(__file__).resolve().parents[1]
REF = Path(r'C:/Users/sc/Documents/Dev/Hardware/EdgeMind-Robot/model')
SOURCE = REF/'chassis/v4-lidar-imu/EdgeMind_Chassis.FCStd'
OUT = MECH/'reference/reused-hardware'


def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()


def main():
    OUT.mkdir(exist_ok=True)
    before = sha(SOURCE)
    old = App.openDocument(str(SOURCE))
    names = ['MotorBracketL','MotorBracketR','RearLWheel','RearLHub','RearRWheel','RearRHub']
    names += ['MotorLPart'+str(i) for i in range(14,21)]
    names += ['MotorRPart'+str(i) for i in range(21,28)]
    names += ['CasterLAxle','CasterLWheel','CasterLSwivel']
    rows=[]
    for name in names:
        obj = old.getObject(name)
        shape = obj.Shape.copy()
        path=OUT/(name+'.brep')
        shape.exportBrep(str(path))
        box=shape.optimalBoundingBox()
        rows.append({'name':name,'source_object':obj.Name,'source_label':obj.Label,
                     'file':path.name,'sha256':sha(path),'volume_mm3':shape.Volume,
                     'bbox_mm':[box.XMin,box.YMin,box.ZMin,box.XMax,box.YMax,box.ZMax]})
    sources=[SOURCE,REF/'hardware/R3X-320x240-reference.step',
             REF/'hardware/直流电机-MG513X（霍尔编码器）/直流电机-MG513X（霍尔编码器）.stp']
    data={'date':'2026-11-05','source_files':[{'path':str(p),'sha256':sha(p)} for p in sources],
          'source_document_unchanged':sha(SOURCE)==before,'parts':rows,
          'battery_geometry_found':False,
          'note':'Reference geometry reused at 1:1 scale; battery remains an explicitly unmeasured envelope.'}
    (OUT/'manifest.json').write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    # Save the precise reference wheel size and existing mount plane in parameters.
    p=MECH/'parameters.yaml';params=json.loads(p.read_text(encoding='utf-8'))
    wheel=next(r for r in rows if r['name']=='RearLWheel')['bbox_mm']
    bracket=next(r for r in rows if r['name']=='MotorBracketL')['bbox_mm']
    params['chassis']['wheel_diameter']=wheel[5]-wheel[2]
    params['chassis']['wheel_width']=wheel[3]-wheel[0]
    params['chassis']['plate_bottom']=bracket[5]
    params['assumptions']['not_measured'][0]='210 mm wheel track; tyre geometry is reused from R3X'
    params['assumptions']['user_confirmed'].extend(['Reuse EdgeMind reference wheel motors and casters',
                                                  'Remove protruding short tops from mounting posts'])
    p.write_text(json.dumps(params,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    App.closeDocument(old.Name)
    print(json.dumps({'extracted':len(rows),'wheel_diameter':params['chassis']['wheel_diameter'],
                      'mount_plane_z':params['chassis']['plate_bottom'],
                      'unchanged':data['source_document_unchanged']}))


if __name__=='__main__':
    main()
