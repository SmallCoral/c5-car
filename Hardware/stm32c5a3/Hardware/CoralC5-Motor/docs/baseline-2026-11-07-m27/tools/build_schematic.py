"""Reproducible KiCad schematic and component-library authoring only.

Never reads, creates or modifies any board file. Rebuild only before editing
this new schematic manually; subsequent edits belong in KiCad.
"""
from pathlib import Path
import sys, math, copy, json, uuid, re, shutil, csv
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT.parent/'Shared/design_tools'))
from sexpr import parse,dump,node,nodes,prop,Atom
K=Path('D:/KiCad/10.0/share/kicad')
A=Atom
def n(tag,*items): return [A(tag),*items]
def num(v): return A(f'{float(v):.5f}'.rstrip('0').rstrip('.') or '0')
def s(v): return round(round(float(v)/1.27)*1.27,5)
def pt(x,y): return s(x),s(y)
def uid(key): return str(uuid.uuid5(uuid.NAMESPACE_URL,'CoralC5-Motor-R1/'+str(key)))
def xy(tag,x,y,*extra): return n(tag,num(x),num(y),*extra)
def effects(size=1.27,justify=None,hide=False):
    e=n('effects',n('font',xy('size',size,size)))
    if justify: e.append(n('justify',*[A(z) for z in justify.split()]))
    if hide: e.append(n('hide',A('yes')))
    return e
def property_(name,value,x=0,y=0,hide=False,justify=None):
    return n('property',name,value,xy('at',x,y,num(0)),effects(1.27,justify,hide))

LIB={}; FPMETA={}; PARTS=[]; PAGES=[]
fpdir=ROOT/'Motor.pretty'; md=ROOT/'Models'
fpdir.mkdir(exist_ok=True);md.mkdir(exist_ok=True)
def local_fp(source, name=None, source_root=K/'footprints', model_root=K/'3dmodels', kind='KiCad package geometry'):
    lib,filename=source.split(':'); name=name or lib+'__'+filename
    src=source_root/(lib+'.pretty')/(filename+'.kicad_mod')
    data=parse(src.read_text(encoding='utf-8')); data[1]=name
    models=[]
    for model in nodes(data,'model'):
        original=model[1]
        if original.startswith('${KIPRJMOD}'):
            srcmodel=(source_root / original.replace('${KIPRJMOD}/',''))
        elif original.startswith('${KICAD10_3DMODEL_DIR}'):
            srcmodel=model_root / original.replace('${KICAD10_3DMODEL_DIR}/','')
        else: srcmodel=Path(original)
        if not srcmodel.exists(): raise FileNotFoundError(srcmodel)
        destname=name+'__'+srcmodel.name
        shutil.copy2(srcmodel,md/destname)
        model[1]='${KIPRJMOD}/Models/'+destname;models.append(destname)
    (fpdir/(name+'.kicad_mod')).write_text(dump(data)+'\n',encoding='utf-8')
    FPMETA['Motor:'+name]={'source':str(src),'models':models,'model_status':kind}
    return 'Motor:'+name

FP={}
for key,src in {
 'R':'Resistor_SMD:R_0603_1608Metric', 'C':'Capacitor_SMD:C_0603_1608Metric',
 'C0805':'Capacitor_SMD:C_0805_2012Metric','C1206':'Capacitor_SMD:C_1206_3216Metric',
 'CP':'Capacitor_THT:CP_Radial_D10.0mm_P5.00mm',
 'F':'Fuse:Fuse_1206_3216Metric','SO8':'Package_SO:SOIC-8_3.9x4.9mm_P1.27mm',
 'DCK':'Package_TO_SOT_SMD:SOT-363_SC-70-6',
 'TSOT':'Package_TO_SOT_SMD:TSOT-23-6','SOD123':'Diode_SMD:D_SOD-123',
 'SMA':'Diode_SMD:D_SMA','LED':'LED_SMD:LED_0603_1608Metric',
 'L':'Inductor_SMD:L_Bourns_SRN6045TA',
 'XH4':'Connector_JST:JST_XH_B4B-XH-A_1x04_P2.50mm_Vertical',
 'PH2':'Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Vertical',
 'H10':'Connector_PinHeader_2.54mm:PinHeader_2x05_P2.54mm_Vertical',
 'M2':'TerminalBlock_Phoenix:TerminalBlock_Phoenix_MKDS-1,5-2-5.08_1x02_P5.08mm_Horizontal',
}.items(): FP[key]=local_fp(src)
FP['EDGE']=local_fp('DevBoard:Streamline_Socket','C5_EDGE_3_Socket',ROOT.parent/'CoralC5-Expansion',kind='existing project vendor model')

# TI PWP0016 PowerPAD: manufacturer drawing pages 39-41, no board file.
FP['DRV']=local_fp('Package_SO:HTSSOP-16-1EP_4.4x5mm_P0.65mm_EP3.4x5mm_Mask2.78x3.37mm','DRV8874_PWP0016',kind='generic HTSSOP body; manufacturer pad geometry in footprint')
drvfp=parse((fpdir/'DRV8874_PWP0016.kicad_mod').read_text(encoding='utf-8'))
for pad in nodes(drvfp,'pad'):
    layers=node(pad,'layers')[1:]
    if layers==['F.Mask']: node(pad,'size')[1:]=[num(2.46),num(3.55)]
    if layers==['F.Paste']:
        x,y=map(float,node(pad,'at')[1:3]);node(pad,'at')[1:3]=[num(math.copysign(.6,x)),num(math.copysign(.9,y))]
        node(pad,'size')[1:]=[num(.95),num(1.45)]
    if pad[1] and pad[1]!='17':
        x,y=map(float,node(pad,'at')[1:3]);node(pad,'at')[1:3]=[num(math.copysign(2.9,x)),num(y)]
        node(pad,'size')[1:]=[num(1.5),num(.45)]
(fpdir/'DRV8874_PWP0016.kicad_mod').write_text(dump(drvfp)+'\n',encoding='utf-8')

def box_wrl(name,boxes):
    # VRML uses 0.1 inch units in KiCad.
    text=['#VRML V2.0 utf8','# Generic external envelope, not a manufacturer assembly model.']
    for center,size,color in boxes:
        c=' '.join(str(round(v/2.54,6)) for v in center);z=' '.join(str(round(v/2.54,6)) for v in size)
        text.append(f'Transform {{ translation {c} children [ Shape {{ appearance Appearance {{ material Material {{ diffuseColor {color} }} }} geometry Box {{ size {z} }} }} ] }}')
    (md/(name+'.wrl')).write_text('\n'.join(text)+'\n',encoding='utf-8')

def custom_fp(name,body,pads,boxes,descr):
    data=n('footprint',name,n('version',num(20241229)),n('generator','pcbnew'),n('layer','F.Cu'),n('descr',descr),n('attr',A('through_hole')))
    data += [property_('Reference','REF**',0,-4),property_('Value',name,0,5)]
    x1,y1,x2,y2=body
    for lay,margin,width in [('F.Fab',0,.1),('F.SilkS',.15,.12),('F.CrtYd',.5,.05)]:
        data.append(n('fp_rect',xy('start',x1-margin,y1-margin),xy('end',x2+margin,y2+margin),n('stroke',n('width',num(width)),n('type',A('default'))),n('fill',A('none')),n('layer',lay)))
    for number,x,y,w,h,dw,dh in pads:
        drill=n('drill',num(dw)) if dw==dh else n('drill',A('oval'),num(dw),num(dh))
        data.append(n('pad',str(number),A('thru_hole'),A('oval'),xy('at',x,y),xy('size',w,h),drill,n('layers','*.Cu','*.Mask')))
    box_wrl(name,boxes)
    data.append(n('model','${KIPRJMOD}/Models/'+name+'.wrl',n('offset',n('xyz',num(0),num(0),num(0))),n('scale',n('xyz',num(1),num(1),num(1))),n('rotate',n('xyz',num(0),num(0),num(0)))))
    (fpdir/(name+'.kicad_mod')).write_text(dump(data)+'\n',encoding='utf-8')
    FPMETA['Motor:'+name]={'source':descr,'models':[name+'.wrl'],'model_status':'generic external envelope derived from manufacturer drawing'}
    return 'Motor:'+name

FP['DC']=custom_fp('DC005_XKB_5A_2mm',(-13.6,-4.5,.4,4.5),[
 ('1',0,0,1.7,4.2,1.0,3.5),('2',-6.5,0,1.7,4.2,1.0,3.5),('3',-2.9,4.7,4.2,1.7,3.5,1.0)],
 [((-6.6,0,5.5),(14,9,11),'0.12 0.12 0.14')]+[((x,-y,-1.25),(w,h,2.5),'0.65 0.65 0.65') for _,x,y,w,h,_,_ in [('1',0,0,1,3.5,1,3.5),('2',-6.5,0,1,3.5,1,3.5),('3',-2.9,4.7,3.5,1,3.5,1)]],
 'XKB DC-005-5A-2.0 manufacturer top-view drilling: 1=center, 3=sleeve, 2=switched sleeve. Verify mating plug 5.5/2.1mm.')
FP['SW']=custom_fp('SS12D07VG4',(-4.4,-2.3,4.4,2.3),[
 ('1',-2,0,1.4,1.4,.8,.8),('2',0,0,1.4,1.4,.8,.8),('3',2,0,1.4,1.4,.8,.8),
 ('4',-4.2,0,2.2,1.4,1.5,.7),('5',4.2,0,2.2,1.4,1.5,.7)],
 [((0,0,2.35),(8.8,4.6,4.7),'0.5 0.5 0.5'),((-1,0,6.7),(2,2.2,4),'0.08 0.08 0.08')],
 'SHOU HAN SS12D07VG4: 2=common, 1/3=throws; 4/5=shell mounting. Local generic model; see source drawing.')

def std(lib,name):
    key=lib+':'+name
    if key in LIB:return key
    data=parse((K/'symbols'/(lib+'.kicad_sym')).read_text(encoding='utf-8'))
    def resolve(name):
        x=copy.deepcopy(next(x for x in nodes(data,'symbol') if x[1]==name));e=node(x,'extends')
        if e:
            parent=resolve(e[1]);old=parent[1];parent[1]=name
            for sub in nodes(parent,'symbol'): sub[1]=sub[1].replace(old+'_',name+'_')
            for fld in nodes(x,'property'):
                oldfld=prop(parent,fld[1])
                if oldfld:parent.remove(oldfld)
                parent.append(copy.deepcopy(fld))
            return parent
        return x
    x=resolve(name);localname=lib+'__'+name;old=x[1];x[1]=localname
    for sub in nodes(x,'symbol'):sub[1]=sub[1].replace(old+'_',localname+'_')
    LIB['Motor:'+localname]=x
    return 'Motor:'+localname

def chip(name,ref,value,pins,fp,rect=(-12.7,20.32,12.7,-20.32),datasheet=''):
    x=n('symbol',name,n('pin_names',n('offset',num(.5))),n('in_bom',A('yes')),n('on_board',A('yes')),
        property_('Reference',ref,0,25.4),property_('Value',value,0,22.86),property_('Footprint',fp,hide=True),property_('Datasheet',datasheet,hide=True))
    b=n('symbol',name+'_0_1',n('rectangle',xy('start',rect[0],rect[1]),xy('end',rect[2],rect[3]),n('stroke',n('width',num(.254)),n('type',A('default'))),n('fill',n('type',A('background')))))
    u=n('symbol',name+'_1_1')
    for number,pname,px,py,angle,kind in pins:
        u.append(n('pin',A(kind),A('line'),xy('at',px,py,num(angle)),n('length',num(5.08)),n('name',pname,effects(1.016)),n('number',str(number),effects(1.016))))
    x += [b,u];LIB['Motor:'+name]=x;return 'Motor:'+name

DRV=chip('DRV8874PWPR','U','DRV8874PWPR',[
 (1,'EN/IN1',-17.78,15.24,0,'input'),(2,'PH/IN2',-17.78,10.16,0,'input'),
 (3,'nSLEEP',-17.78,5.08,0,'input'),(4,'nFAULT',-17.78,0,0,'open_collector'),
 (5,'VREF',-17.78,-5.08,0,'input'),(6,'IPROPI',-17.78,-10.16,0,'output'),
 (7,'IMODE',-5.08,-25.4,90,'input'),(8,'OUT1',17.78,10.16,180,'power_out'),
 (9,'PGND',0,-25.4,90,'power_in'),(10,'OUT2',17.78,5.08,180,'power_out'),
 (11,'VM',0,25.4,270,'power_in'),(12,'VCP',17.78,15.24,180,'passive'),
 (13,'CPH',17.78,-5.08,180,'passive'),(14,'CPL',17.78,-10.16,180,'passive'),
 (15,'GND',5.08,-25.4,90,'power_in'),(16,'PMODE',-10.16,-25.4,90,'input'),
 (17,'EP',10.16,-25.4,90,'power_in')],FP['DRV'],datasheet='https://www.ti.com/lit/ds/symlink/drv8874.pdf')
BUF=chip('SN74LVC2G17DCKR','U','SN74LVC2G17DCKR',[
 (1,'1A',-12.7,2.54,0,'input'),(6,'1Y',12.7,2.54,180,'output'),
 (3,'2A',-12.7,-2.54,0,'input'),(4,'2Y',12.7,-2.54,180,'output'),
 (5,'VCC',0,12.7,270,'power_in'),(2,'GND',0,-12.7,90,'power_in')],FP['DCK'],rect=(-7.62,7.62,7.62,-7.62),datasheet='https://www.ti.com/lit/ds/symlink/sn74lvc2g17.pdf')
PMOS=std('Transistor_FET','Q_PMOS_GSD'); q=copy.deepcopy(LIB[PMOS]);old=q[1];q[1]='AO4407A'
for sub in nodes(q,'symbol'):
    sub[1]=sub[1].replace(old+'_','AO4407A_')
    pins=nodes(sub,'pin')
    for p in pins:
        name=node(p,'name')[1]
        nums={'G':['4'],'S':['1','2','3'],'D':['5','6','7','8']}[name]
        node(p,'number')[1]=nums[0]
        for pn in nums[1:]:
            extra=copy.deepcopy(p);node(extra,'number')[1]=pn;extra.append(n('hide',A('yes')));sub.append(extra)
prop(q,'Value')[2]='AO4407A';prop(q,'Footprint')[2]=FP['SO8'];prop(q,'Datasheet')[2]='https://aosmd.com/sites/default/files/res/datasheets/AO4407A.pdf'
LIB['Motor:AO4407A']=q;PMOS='Motor:AO4407A'

# Existing connector symbol and exact C5-EDGE-3 footprint/model.
existing=parse((ROOT.parent/'CoralC5-Expansion/02_Interface.kicad_sch').read_text(encoding='utf-8'))
edge=copy.deepcopy(next(x for x in nodes(node(existing,'lib_symbols'),'symbol') if x[1]=='DevBoard:CoralC5_Streamline_Socket'))
edge[1]='CoralC5_Streamline_Socket';prop(edge,'Footprint')[2]=FP['EDGE'];LIB['Motor:CoralC5_Streamline_Socket']=edge

class Page:
    def __init__(self,file,title,pageno,paper='A3'):
        self.file=file;self.title=title;self.pageno=pageno;self.uuid=uid(file);self.sheetuuid=uid('sheet/'+file)
        self.data=n('kicad_sch',n('version',num(20260306)),n('generator','eeschema'),n('generator_version','10.0'),n('uuid',self.uuid),n('paper',paper),
          n('title_block',n('title',title),n('date','2026-10-27'),n('rev','M1'),n('company','SmallCoral / CoralC5-Motor'),n('comment',num(1),'原理图设计；实板电流、温升与线束待验证'))),
        self.data=self.data[0];self.used={};self.components={};self.wires=[];self.labels=[];self.ncpoints=[];self.count=0;PAGES.append(self)
    def add(self,obj):self.data.append(obj);return obj
    def wire(self,*points):
        pp=[pt(*p) for p in points]
        for a,b in zip(pp,pp[1:]):
            if a==b:continue
            if a[0]!=b[0] and a[1]!=b[1]:
                corner=(b[0],a[1]);self.wires.extend([(a,corner),(corner,b)])
            else:self.wires.append((a,b))
    def label(self,name,at,angle=0):
        p=pt(*at);self.labels.append((name,p))
        self.add(n('label',name,xy('at',*p,num(angle)),effects(1.27,'left bottom' if angle==0 else 'right bottom'),n('uuid',uid(f'{self.file}/label/{name}/{p}'))))
    def port(self,name,at,shape='input',angle=0):
        # The endpoint faces the wire. Put the label body on the opposite side.
        p=pt(*at);angle=(angle+180)%360
        self.add(n('hierarchical_label',name,n('shape',A(shape)),xy('at',*p,num(angle)),effects(1.27,'left' if angle==0 else 'right'),n('uuid',uid(f'{self.file}/port/{name}'))))
    def nc(self,p):
        p=pt(*p);self.ncpoints.append(p);self.add(n('no_connect',xy('at',*p),n('uuid',uid(f'{self.file}/NC/{p}'))))
    def note(self,text,x,y):self.add(n('text',text,xy('at',*pt(x,y),num(0)),effects(2.54,'left top'),n('uuid',uid(f'{self.file}/note/{text}'))))
    def frame(self,title,x1,y1,x2,y2):
        stroke=n('stroke',n('width',num(.254)),n('type',A('dash')))
        if node(self.data,'paper')[1]=='A4' and x2>175.26 and y2>165.1:
            points=[pt(x1,y1),pt(x2,y1),pt(x2,165.1),pt(175.26,165.1),pt(175.26,y2),pt(x1,y2),pt(x1,y1)]
            self.add(n('polyline',n('pts',*[xy('xy',*q) for q in points]),stroke,n('fill',n('type',A('none'))),n('uuid',uid(f'{self.file}/frame/{title}'))))
        else:self.add(n('rectangle',xy('start',*pt(x1,y1)),xy('end',*pt(x2,y2)),stroke,n('fill',n('type',A('none'))),n('uuid',uid(f'{self.file}/frame/{title}'))))
        self.note(title,x1+3,y1+2)
    def place(self,key,ref,value,x,y,fp='',rot=0,unit=1,mpn='',datasheet='',fields=None,refpos=None,valpos=None):
        x,y=pt(x,y);lib=LIB[key];self.used[key]=lib;self.count+=1
        z=n('symbol',n('lib_id',key),xy('at',x,y,num(rot)),n('unit',num(unit)),n('in_bom',A('no' if ref.startswith('#') else 'yes')),n('on_board',A('no' if ref.startswith('#') else 'yes')),n('dnp',A('no')),n('uuid',uid(f'{self.file}/{ref}/{unit}')))
        simple=key.endswith(('__R','__C','__C_Polarized','__L','__Fuse'))
        vertical_diode=rot in [90,270] and key.endswith(('__D','__D_Zener','__D_Schottky','__LED'))
        if refpos is None:
            refpos=(x+2.54,y-1.27) if (simple and rot==0) or vertical_diode else (x,y-7.62)
        if valpos is None:
            valpos=(x+2.54,y+1.27) if (simple and rot==0) or vertical_diode else (x,y-5.08)
        z += [property_('Reference',ref,*refpos,hide=ref.startswith('#'),justify='left' if (simple and rot==0) or vertical_diode else None),property_('Value',value,*valpos,hide=ref.startswith('#'),justify='left' if (simple and rot==0) or vertical_diode else None),
          property_('Footprint',fp,x,y,hide=True),property_('Datasheet',datasheet or (prop(lib,'Datasheet')[2] if prop(lib,'Datasheet') else ''),x,y,hide=True)]
        if mpn:z.append(property_('MPN',mpn,x,y,hide=True))
        if rot in [90,270]:
            for pn in ['Reference','Value']:node(prop(z,pn),'at')[3]=num(90)
        for k,v in (fields or {}).items():z.append(property_(k,v,x,y,hide=True))
        pins={}
        for sub in nodes(lib,'symbol'):
            m=re.search(r'_(\d+)_(\d+)$',sub[1]);u=int(m.group(1)) if m else 1
            if u not in [0,unit]:continue
            for pin in nodes(sub,'pin'):
                px,py,pa=map(float,node(pin,'at')[1:4]);a=math.radians(rot)
                pos=pt(x+px*math.cos(a)-py*math.sin(a),y-(px*math.sin(a)+py*math.cos(a)))
                pins[node(pin,'number')[1]]=pos
                z.append(n('pin',node(pin,'number')[1],n('uuid',uid(f'{self.file}/{ref}/{unit}/pin{node(pin,"number")[1]}'))))
        parent=ROOT_UUID if self.pageno==1 else ROOT_UUID+'/'+self.sheetuuid
        z.append(n('instances',n('project','CoralC5-Motor',n('path','/'+parent,n('reference',ref),n('unit',num(unit))))))
        self.add(z);self.components[(ref,unit)]={'pins':pins,'key':key,'symbol':z}
        if not ref.startswith('#') and unit==1:PARTS.append({'reference':ref,'value':value,'mpn':mpn,'footprint':fp,'page':self.pageno,'datasheet':prop(z,'Datasheet')[2]})
        return pins
    def gnd(self,p):
        global POWERCOUNT
        POWERCOUNT+=1;return self.place(std('power','GND'),f'#PWR{POWERCOUNT:03}','GND',*p,refpos=(s(p[0]),s(p[1])),valpos=(s(p[0]),s(p[1]+3.81)))
    def flag(self,p):
        global FLAGCOUNT
        FLAGCOUNT+=1;return self.place(std('power','PWR_FLAG'),f'#FLG{FLAGCOUNT:03}','PWR_FLAG',*p,refpos=pt(*p),valpos=pt(*p))
    def netpin(self,p,name,dx=-7.62):
        end=(s(p[0]+dx),p[1]);self.wire(p,end);self.label(name,end,0 if dx<0 else 180)
    def save(self):
        # Split T junctions at endpoints; do not connect unrelated wire crossings.
        endpoints=set(p for a,b in self.wires for p in [a,b]);segs=set()
        for a,b in self.wires:
            cuts=[p for p in endpoints if (a[0]==b[0]==p[0] and min(a[1],b[1])<=p[1]<=max(a[1],b[1])) or (a[1]==b[1]==p[1] and min(a[0],b[0])<=p[0]<=max(a[0],b[0]))]
            cuts.sort()
            for aa,bb in zip(cuts,cuts[1:]):segs.add((aa,bb))
        degree={}
        for a,b in sorted(segs):
            self.add(n('wire',n('pts',xy('xy',*a),xy('xy',*b)),n('stroke',n('width',num(0)),n('type',A('default'))),n('uuid',uid(f'{self.file}/wire/{a}/{b}'))))
            for p in (a,b):degree[p]=degree.get(p,0)+1
        for p,d in degree.items():
            if d>=3:self.add(n('junction',xy('at',*p),n('diameter',num(0)),n('color',num(0),num(0),num(0),num(0)),n('uuid',uid(f'{self.file}/junction/{p}'))))
        embeds=[]
        for key,lib in self.used.items():
            x=copy.deepcopy(lib);x[1]=key;embeds.append(x)
        self.data.insert(7,n('lib_symbols',*embeds));self.add(n('embedded_fonts',A('no')))
        (ROOT/self.file).write_text(dump(self.data)+'\n',encoding='utf-8')

POWERCOUNT=0;FLAGCOUNT=0;ROOT_UUID=uid('CoralC5-Motor.kicad_sch')
R=std('Device','R');C=std('Device','C');CP=std('Device','C_Polarized');L=std('Device','L')
DZEN=std('Device','D_Zener');D=std('Device','D_Schottky');LED=std('Device','LED');FUSE=std('Device','Fuse')
J2=std('Connector_Generic','Conn_01x02');J4=std('Connector_Generic','Conn_01x04');H10=std('Connector_Generic','Conn_02x05_Odd_Even')
SW=std('Switch','SW_SPDT');AP=std('Regulator_Switching','AP63205WU')
swunit=next(x for x in nodes(LIB[SW],'symbol') if x[1].endswith('_1_1'))
for pn in ['4','5']:
    pp=n('pin',A('passive'),A('line'),xy('at',0,-12.7,num(90)),n('length',num(2.54)),n('name','CASE',effects(1.016)),n('number',pn,effects(1.016)))
    if pn=='5':pp.append(n('hide',A('yes')))
    swunit.append(pp)
# Jack contact numbering differs from KiCad generic: XKB 2=switch, 3=sleeve.
jack=copy.deepcopy(LIB[std('Connector','Barrel_Jack_Switch')]);old=jack[1];jack[1]='DC005_XKB'
for sub in nodes(jack,'symbol'):
    sub[1]=sub[1].replace(old+'_','DC005_XKB_')
    for pin in nodes(sub,'pin'):
        pn=node(pin,'number')[1]
        if pn=='2':node(pin,'number')[1]='3'
        elif pn=='3':node(pin,'number')[1]='2'
LIB['Motor:DC005_XKB']=jack;JACK='Motor:DC005_XKB'

root=Page('CoralC5-Motor.kicad_sch','Coral-C5 双电机扩展板总览',1)
p=Page('02_Power.kicad_sch','十二伏入口、拨动总开关与五伏降压',2)
core=Page('03_Core_Interface.kicad_sch','C5-EDGE-3 核心板接口与调试',3)
left=Page('04_Left_Motor.kicad_sch','左电机驱动与霍尔编码器',4)
right=Page('05_Right_Motor.kicad_sch','右电机驱动与霍尔编码器',5)

def res(page,ref,value,x,y,rot=0,**kw):return page.place(R,ref,value,x,y,FP['R'],rot=rot,mpn=kw.pop('mpn',''),**kw)
def cap(page,ref,value,x,y,fp='C',rot=0,**kw):return page.place(C,ref,value,x,y,FP[fp],rot=rot,**kw)
def terminal(page,pins,netmap):
    for pn,name in netmap.items():page.netpin(pins[pn],name)

used={
 'PD12':'L_PWM','PD10':'L_DIR','PD13':'R_PWM','PD11':'R_DIR','PE2':'MOTOR_EN_REQ',
 'PE4':'L_nFAULT','PE5':'R_nFAULT','PA0':'L_ISENSE','PA1':'R_ISENSE',
 'PA15':'L_ENC_A','PB3':'L_ENC_B','PC6':'R_ENC_A','PC7':'R_ENC_B',
 'PA13':'SWDIO','PA14':'SWCLK','PB6':'AUX_I2C_SCL','PB7':'AUX_I2C_SDA',
 'PA2':'AUX_UART_TX','PA3':'AUX_UART_RX','PA4':'AUX_ADC','PA5':'AUX_GPIO',
 '+3V3':'V3V3','+5V_SYS':'V5_SYS','GND':'GND','NRST':'NRST'}
rows=list(csv.DictReader((ROOT.parent/'Shared/interface.csv').open(encoding='utf-8-sig')))
contactmap={x['contact']:x['net'] for x in rows}
ports=['V5_SYS','V3V3','L_PWM','L_DIR','L_nFAULT','L_ISENSE','L_ENC_A','L_ENC_B','R_PWM','R_DIR','R_nFAULT','R_ISENSE','R_ENC_A','R_ENC_B','MOTOR_RUN']

def sheet(page,title,x,y,w,h,pindefs):
    endx,endy=pt(x+w,y+h);x,y=pt(x,y);pins={}
    z=n('sheet',xy('at',x,y),xy('size',endx-x,endy-y),n('stroke',n('width',num(.254)),n('type',A('default'))),n('fill',n('color',num(0),num(0),num(0),num(0))),n('uuid',page.sheetuuid),
        property_('Sheetname',title,x,y-5.08),property_('Sheetfile',page.file,x,y+h+2.54))
    for name,direction,px,py,angle in pindefs:
        pp=pt(px,py);pins[name]=pp
        z.append(n('pin',name,A(direction),xy('at',*pp,num(angle)),effects(1.27,'left' if angle==180 else 'right'),n('uuid',uid('sheetpin/'+page.file+'/'+name))))
    z.append(n('instances',n('project','CoralC5-Motor',n('path','/'+ROOT_UUID,n('page',str(page.pageno))))))
    root.add(z);return pins


exec((ROOT/'tools/compact_layout.py').read_text(encoding='utf-8'),globals())
for pg in PAGES:pg.save()
lib=n('kicad_symbol_lib',n('version',num(20241209)),n('generator','kicad_symbol_editor'),*[copy.deepcopy(v) for v in LIB.values()])
(ROOT/'Motor.kicad_sym').write_text(dump(lib)+'\n',encoding='utf-8')
(ROOT/'sym-lib-table').write_text('(sym_lib_table (lib (name "Motor")(type "KiCad")(uri "${KIPRJMOD}/Motor.kicad_sym")(options "")(descr "CoralC5 Motor local symbols")))\n',encoding='utf-8')
(ROOT/'fp-lib-table').write_text('(fp_lib_table (lib (name "Motor")(type "KiCad")(uri "${KIPRJMOD}/Motor.pretty")(options "")(descr "CoralC5 Motor portable footprints")))\n',encoding='utf-8')
(ROOT/'CoralC5-Motor.kicad_pro').write_text(json.dumps({'meta':{'filename':'CoralC5-Motor.kicad_pro','version':3},'text_variables':{'AUTHOR':'SmallCoral'},'schematic':{'meta':{'version':1}},'erc':{'erc_exclusions':[],'meta':{'version':0},'pin_map':[]}},indent=2)+'\n',encoding='utf-8')
(ROOT/'docs/design_manifest.json').write_text(json.dumps({'revision':'M1','date':'2026-10-27','pages':[{'file':p.file,'title':p.title,'page':p.pageno} for p in PAGES],'parts':PARTS,'footprints':FPMETA,'finger_map':contactmap,'gpio_functions':used},ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
with (ROOT/'docs/BOM.csv').open('w',encoding='utf-8-sig',newline='') as f:
    w=csv.DictWriter(f,fieldnames=['reference','value','mpn','footprint','page','datasheet']);w.writeheader();w.writerows(PARTS)
print('Created',len(PAGES),'schematic pages,',len(PARTS),'physical references,',len(FPMETA),'footprints')
