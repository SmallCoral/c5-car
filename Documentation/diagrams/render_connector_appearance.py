"""Render existing CAD meshes into transparent connector thumbnails, without GUI."""
from pathlib import Path
import json
import numpy as np
from PIL import Image,ImageDraw,ImageFilter,ImageFont

ROOT=Path(__file__).resolve().parents[2]
DATA=ROOT/'.scratch/interface-tree-v2/connector-meshes.json'
OUT=Path(__file__).resolve().parent/'connector-appearance'
OUT.mkdir(exist_ok=True)
models=json.loads(DATA.read_text(encoding='utf8'))
eye=np.array([1.6,-2.7,2.2]);eye/=np.linalg.norm(eye)
right=np.cross([0,0,1],eye);right/=np.linalg.norm(right)
up=np.cross(eye,right)
light=np.array([-.25,-.7,1.5]);light/=np.linalg.norm(light)
size=(720,450)
for key,model in models.items():
    triangles=[];positions=[]
    for part in model['parts']:
        verts=np.asarray(part['v']);faces=np.asarray(part['f']);tri=verts[faces]
        norm=np.cross(tri[:,1]-tri[:,0],tri[:,2]-tri[:,0]);length=np.linalg.norm(norm,axis=1);norm/=np.maximum(length[:,None],1e-12)
        visible=(norm@eye)>0
        tri=tri[visible];norm=norm[visible]
        projected=np.stack([tri@right,-tri@up],axis=-1)
        depth=tri.mean(axis=1)@eye
        base=np.tile(np.array(part['color']),(len(tri),1))
        if key.startswith(('xh','female','header')):
            # Standard KiCad STEP files can be one solid with face-level colors.
            # Distinguish exposed through-hole tails with illustrative metal color.
            metal=tri.mean(axis=1)[:,2]<-.05
            base[metal]=[.72,.67,.48]
        intensity=.64+.36*np.maximum(norm@light,0)
        rgb=np.clip(base*intensity[:,None]*255,0,255).astype(int)
        triangles.extend(zip(depth,projected,rgb));positions.append(projected.reshape(-1,2))
    pos=np.concatenate(positions);lo=pos.min(axis=0);hi=pos.max(axis=0)
    scale=min((size[0]-95)/(hi[0]-lo[0]),(size[1]-105)/(hi[1]-lo[1]))
    center=(lo+hi)/2
    out=Image.new('RGBA',size,(255,255,255,0))
    shadow=Image.new('RGBA',size,(0,0,0,0));d=ImageDraw.Draw(shadow)
    dw=min((hi[0]-lo[0])*scale,size[0]-100)
    d.ellipse((size[0]/2-dw*.44,size[1]-68,size[0]/2+dw*.44,size[1]-34),fill=(31,50,72,40))
    out.alpha_composite(shadow.filter(ImageFilter.GaussianBlur(15)))
    d=ImageDraw.Draw(out)
    for _,p,c in sorted(triangles,key=lambda x:x[0]):
        pts=(p-center)*scale+np.array([size[0]/2,size[1]/2-12])
        d.polygon([tuple(q) for q in pts],fill=tuple(c)+(255,))
    out.resize((360,225),Image.Resampling.LANCZOS).save(OUT/(key+'.png'))
    print(key,len(triangles),'visible triangles')

record={k:{field:value for field,value in m.items() if field!='parts'} for k,m in models.items()}
(OUT/'sources.json').write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
# A contact sheet is only a review artifact, not an additional user deliverable.
sheet=Image.new('RGB',(1200,1040),'#F3F6FA');d=ImageDraw.Draw(sheet)
f=ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',20)
for i,key in enumerate(models):
    x=(i%3)*400;y=(i//3)*260
    img=Image.open(OUT/(key+'.png'));sheet.paste(img,(x+20,y+25),img)
    d.text((x+25,y+230),key,font=f,fill='#24364B')
sheet.save(ROOT/'.scratch/interface-tree-v2/appearance-review.png')
