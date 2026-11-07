"""Render the reviewed M2.7 interface tree as one PNG and editable SVG."""
from pathlib import Path
import html, json, hashlib, base64, xml.etree.ElementTree as ET
from PIL import Image, ImageDraw, ImageFont

OUT=Path(__file__).resolve().parent
ROOT=OUT.parents[1]
MOTOR=ROOT/'Hardware/stm32c5a3/Hardware/CoralC5-Motor'
W,H=4240,2520
im=Image.new('RGB',(W,H),'#F4F7FB');draw=ImageDraw.Draw(im)
svg=[f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">',
     '<title>CoralC5-Motor M2.7 扩展板接口树状图</title>',
     '<desc>依据当前原生网表和接口合同。树线表示功能归属，不表示实际布线；针号按扩展板端编号。</desc>',
     f'<rect width="{W}" height="{H}" fill="#F4F7FB"/>']
fonts={};text_boxes=[]
def font(size,bold=False):
    key=(size,bold)
    if key not in fonts:fonts[key]=ImageFont.truetype('C:/Windows/Fonts/msyhbd.ttc' if bold else 'C:/Windows/Fonts/msyh.ttc',size)
    return fonts[key]
def rect(box,fill,outline=None,r=18,width=2):
    x,y,x2,y2=box;draw.rounded_rectangle(box,radius=r,fill=fill,outline=outline,width=width)
    svg.append(f'<rect x="{x}" y="{y}" width="{x2-x}" height="{y2-y}" rx="{r}" fill="{fill}" stroke="{outline or "none"}" stroke-width="{width}"/>')
def line(points,color='#C2CCD9',width=4):
    draw.line(points,fill=color,width=width,joint='curve')
    svg.append('<polyline points="'+' '.join(f'{x},{y}' for x,y in points)+f'" fill="none" stroke="{color}" stroke-width="{width}" stroke-linecap="round" stroke-linejoin="round"/>')
def dot(x,y,color,r=6):
    draw.ellipse((x-r,y-r,x+r,y+r),fill=color)
    svg.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="{color}"/>')
def text(x,y,s,size=26,color='#24364B',bold=False,center=False,maxw=None):
    f=font(size,bold);width=draw.textlength(s,font=f)
    if maxw is not None and width>maxw:raise ValueError(f'Text too wide {width:.1f}>{maxw}: {s}')
    if center:x-=width/2
    draw.text((x,y),s,font=f,fill=color,anchor='lt')
    svg.append(f'<text x="{x:.2f}" y="{y}" font-family="Microsoft YaHei, Noto Sans CJK SC, sans-serif" font-size="{size}" font-weight="{700 if bold else 400}" fill="{color}" dominant-baseline="text-before-edge">{html.escape(s)}</text>')
    text_boxes.append((x,y,width,size,s))
def badge(right,y,s,color):
    width=draw.textlength(s,font=font(25,True))+36
    rect((right-width,y,right,y+44),color,r=11)
    text(right-width/2,y+7,s,25,'#FFFFFF',True,True)
def pinrow(x,y,labels,color,width=988,height=64):
    gap=8;cell=(width-gap*(len(labels)-1))/len(labels)
    for i,label in enumerate(labels):
        a=x+i*(cell+gap)
        rect((a,y,a+cell,y+height),'#F4F7FB','#DFE6EE',9,1)
        text(a+cell/2,y+7,str(i+1),18,color,True,True)
        text(a+cell/2,y+31,label,21,'#24364B',False,True,maxw=cell-6)

def appearance(x,y,h,items):
    """Embed a CAD thumbnail and a caption for each connector on this card."""
    panelx=x+1043;panelw=274;top=y+73;available=h-89
    rect((panelx,top,panelx+panelw,top+available),'#F7F9FC',r=13)
    slot=available/len(items)
    for i,(key,label) in enumerate(items):
        path=OUT/'connector-appearance'/(key+'.png')
        photo=Image.open(path).convert('RGBA')
        maxh=max(40,slot-31);maxw=panelw-12
        scale=min(maxw/photo.width,maxh/photo.height)
        iw,ih=round(photo.width*scale),round(photo.height*scale)
        px=panelx+(panelw-iw)/2;py=top+i*slot+(slot-26-ih)/2
        resized=photo.resize((iw,ih),Image.Resampling.LANCZOS)
        im.paste(resized,(round(px),round(py)),resized)
        encoded=base64.b64encode(path.read_bytes()).decode('ascii')
        svg.append(f'<image x="{px:.2f}" y="{py:.2f}" width="{iw}" height="{ih}" href="data:image/png;base64,{encoded}"/>')
        text(panelx+panelw/2,top+(i+1)*slot-26,label,18,'#63758C',center=True,maxw=panelw-10)

# Card content is intentionally explicit; no interfaces are inferred from graphics.
groups=[
 dict(id='01',title='电源与核心板',sub='电池 / 总降压 / 核心板',color='#C77B18',soft='#FFF3DE',side='L',cy=500,cards=[
  dict(y=250,h=250,title='电池输入与总电源',tag='J1 · SW1',refs=['J1'],lines=[
   ('J1：XT60PW-M / 2P · 12V 电池',27),
   ('1 BAT+    2 GND',27),
   ('F1 15A → SS12D10G5 控制 Q1/Q2 → VM',26),
   ('U1 TPS56837：VM → 4.992V（8A 芯片等级）',26),
   ('U7 AP2112K：总 5V → TFT / 蓝牙 3.3V',26)]),
  dict(y=530,h=220,title='Core 核心板插座',tag='J14 · 98P',refs=['J14'],lines=[
   ('A48 / B48：经 D3 隔离后的 5V_SYS 输入',27),
   ('Core AP63203 → 3.3V；A47 / B47 引出',27),
   ('A1 / A49 / B1 / B49：GND；保留 USB 隔离',26),
   ('Core 3.3V 对外预算 100mA，不与外设 LDO 并联',26)])]),
 dict(id='02',title='运动驱动',sub='左右轮 / 摆杆步进',color='#3479D7',soft='#EAF2FF',side='L',cy=1185,cards=[
  dict(y=860,h=270,title='左右 MG513 编码电机',tag='J4 左 · J6 右',refs=['J4','J6'],intro='每侧 XH / 6P · 动力与编码器同一个插口',pins=['M−','VENC','A','B','GND','M+'],lines=[
   ('U2 / U4：DRV8874；VENC 当前取 Core 3.3V',26),
   ('M−、M+ 均为 H 桥输出；M− 不是系统地',26)]),
  dict(y=1160,h=350,title='ZDT_XS TTL 一体步进套件',tag='J23 · XH 8P',refs=['J23'],intro='仅接 1 / 2 / 7 / 8；不直接连接电机线圈',pins=['VM_STEP','GND','NC','NC','NC','NC','模块 RX','模块 TX'],lines=[
   ('7 RX ← PB6 / USART1_TX；8 TX → PB7 / RX',26),
   ('VM → F2 4A → Q6/Q7 电源开关 → 1 脚',26),
   ('PD12 / B30 高电平供电，硬件默认断电',26),
   ('U15 串口隔离；启动稳定后再发送命令',26)])]),
 dict(id='03',title='调试与停机',sub='SWD / 轮电机使能',color='#596C85',soft='#EBF0F6',side='L',cy=1820,cards=[
  dict(y=1570,h=230,title='SWD 下载与调试',tag='J8 · XH 4P',refs=['J8'],pins=['3V3','SWDIO','SWCLK','GND'],lines=[
   ('3V3 为 Core 电源参考；与调试器共地',26)]),
  dict(y=1830,h=220,title='左右轮 RUN / STOP 跳帽',tag='J10 · 2P',refs=['J10'],lines=[
   ('1：MOTOR_EN_REQ 经 R6；2：MOTOR_RUN',27),
   ('插帽：允许 PE8 控制左右轮驱动',27),
   ('拔帽：下拉 MOTOR_RUN，左右轮进入休眠',26),
   ('只控制轮驱动，不控制步进或整车总电源',26)])]),
 dict(id='04',title='视觉系统',sub='MaixCAM2 识别 / Action 4 图传',color='#8155C8',soft='#F0EBFB',side='R',cy=557,cards=[
  dict(y=250,h=350,title='MaixCAM2 · 钢球识别',tag='J19 · J27',refs=['J19','J27'],intro='J19：MX1.25 / 6P · 保护后 5V · USART6',pins=['GND','5V','NC','NC','模块 TX','模块 RX'],lines=[
   ('5 TX → PC7 / RX；6 RX ← PC6 / TX',26),
   ('J27：1=5V，2=EN_FEED；短接使能，带电 USB 前拔帽',26),
   ('保留过压、限流、软启动和防反灌',26),
   ('六针座额定 1A，识别叠加充电须实测',26)]),
  dict(y=630,h=235,title='Action 4 · 图传与录像',tag='J28 · Type-C',refs=['J28'],lines=[
   ('Type-C 母座，使用 C-C 线；仅供电',27),
   ('VBUS：A9/B9；GND：A12/B12/外壳',26),
   ('CC1：A5；CC2：B5；固定 5V / 3A 通告',26),
   ('U16 TPS25810；无 PD/PPS，不接 MCU 数据',26)])]),
 dict(id='05',title='灰度循迹',sub='感为 8 路 + MSPM0L1306 辅助板',color='#178D7B',soft='#E7F6F1',side='R',cy=1105,cards=[
  dict(y=970,h=310,title='底部八路灰度组件',tag='J20 · J21',refs=['J20','J21'],intro='J20：XH / 8P 数字信号口，按 1～8 脚排列',pins=['CH1','CH2','CH3','CH4','CH5','CH6','CH7','CH8'],lines=[
   ('CH1～6 → PE10～15；CH7/8 → PD10/11',26),
   ('J21：XH / 2P；1 GND，2 总 5V',26),
   ('两口一起接；GPIO 无上下拉；KEY/Err/CLK/DAT 不接',25)])]),
 dict(id='06',title='显示与操作',sub='TFT / HC-08 / 四个按键',color='#BB4778',soft='#FCEBF2',side='R',cy=1790,cards=[
  dict(y=1370,h=310,title='TFT 任务显示屏',tag='J17 · 排母 8P',refs=['J17'],intro='2.54mm 排母 · 外设 LDO 3.3V · SPI1',pins=['GND','3V3','SCL','SDA','RES','DC','CS','BL'],lines=[
   ('SCL=SCK / PA5；SDA=MOSI / PA7',26),
   ('RES=PB0；DC=PB1；CS=PA4；BL=PB4',26),
   ('H1/H2：两个 D2.0 安装孔，孔距须实物复核',26)]),
  dict(y=1710,h=240,title='HC-08 蓝牙模块',tag='J15 · 排母 4P',refs=['J15'],intro='2.54mm 排母 · 固定外设 3.3V',pins=['3V3','GND','模块 RXD','模块 TXD'],lines=[
   ('3 RXD ← PA2 / TX；4 TXD → PA3 / RX',26)]),
  dict(y=1980,h=230,title='四个板载操作按键',tag='SW2～SW5',refs=[],lines=[
   ('SW2 模式 PC4；SW3 下一项 PC5',27),
   ('SW4 确认 PB2；SW5 启动 PE7',27),
   ('6×6×4.3mm 贴片；低有效，10k 上拉、软件消抖',26)])])]

# Match all actual connectors, including auxiliary control and debug headers.
tree=ET.parse(MOTOR/'docs/netlist.xml')
actual={c.get('ref') for c in tree.findall('.//components/comp') if c.get('ref').startswith('J')}
listed={r for g in groups for c in g['cards'] for r in c['refs']}
assert actual==listed,{'missing':sorted(actual-listed),'extra':sorted(listed-actual)}
appearances={
 'J1':[('xt60','J1 · XT60 卧式示意')],
 'J14':[('core98','J14 · 98P 核心板插座')],
 'J4':[('xh6','J4/J6 · XH 6P')],
 'J23':[('xh8','J23 · XH 8P')],
 'J8':[('xh4','J8 · XH 4P')],
 'J10':[('header2','J10 · 2.54mm 排针')],
 'J19':[('mx6','J19 · MX1.25 示意'),('header2','J27 · 2.54mm 排针')],
 'J28':[('usb_c','J28 · Type-C 母座')],
 'J20':[('xh8','J20 · XH 8P'),('xh2','J21 · XH 2P')],
 'J17':[('female8','J17 · 2.54mm 8P 排母')],
 'J15':[('female4','J15 · 2.54mm 4P 排母')],
 'buttons':[('button','按键 · 贴片四脚示意')],
}

text(60,54,'CoralC5-Motor 扩展板接口树状图',58,'#162C45',True)
text(64,145,'功能分支 · 板端针序 · 接口外观参考',29,'#63758C')
badge(4180,70,'M2.7  /  2026-11-07','#244564')
text(3740,143,'14 个连接器全部列入',26,'#63758C',maxw=440)

# Shared trunk: a rooted tree, intentionally without arrows or electrical cross-links.
line([(1960,1200),(1870,1200)],'#B4C4D8',6)
line([(1870,500),(1870,1820)],'#B4C4D8',6)
line([(2340,1200),(2385,1200)],'#B4C4D8',6)
line([(2385,557),(2385,1790)],'#B4C4D8',6)
for g in groups:
    side=g['side'];cy=g['cy'];col=g['color']
    if side=='L':
        cx=1480;line([(1800,cy),(1870,cy)],col,5);dot(1870,cy,col)
        leafedge=1400;branch=1445;categoryedge=1480
    else:
        cx=2430;line([(2385,cy),(2430,cy)],col,5);dot(2385,cy,col)
        leafedge=2840;branch=2790;categoryedge=2750
    centers=[c['y']+c['h']/2 for c in g['cards']]
    line([(branch,min(centers+[cy])),(branch,max(centers+[cy]))],col,4)
    line([(categoryedge,cy),(branch,cy)],col,4)
    for c,yy in zip(g['cards'],centers):line([(branch,yy),(leafedge,yy)],col,4)
    rect((cx,cy-68,cx+320,cy+68),g['soft'],None,23)
    text(cx+22,cy-46,g['id'],21,col,True)
    text(cx+22,cy-17,g['title'],33,col,True,maxw=280)
    # The subtitle is a technical cue; use two lines if it exceeds the node width.
    sub=g['sub'];parts=sub.split(' / ')
    if draw.textlength(sub,font=font(20))>280:
        sub='MaixCAM2 / Action 4' if g['id']=='04' else '感为 8 路 + MSPM0 辅助板'
    text(cx+22,cy+30,sub,20,'#66778B',maxw=282)

# Root card.
rect((1963,1047,2343,1367),'#E1E9F2',r=32)
rect((1960,1040,2340,1360),'#173653',r=30)
text(2150,1070,'CoralC5-Motor',36,'#FFFFFF',True,True,maxw=350)
text(2150,1126,'小车扩展板',34,'#FFFFFF',True,True)
line([(2004,1181),(2296,1181)],'#40607D',2)
text(2150,1201,'一路总 5V 降压',27,'#D8E8F7',center=True)
text(2150,1247,'一颗外设 3.3V LDO',26,'#D8E8F7',center=True)
text(2150,1301,'所有模块共地',25,'#A8C9E7',center=True)

for g in groups:
    for c in g['cards']:
        x=60 if g['side']=='L' else 2840;y=c['y'];w=1340;h=c['h'];col=g['color']
        rect((x+2,y+6,x+w+2,y+h+6),'#E6ECF3',r=22)
        rect((x,y,x+w,y+h),'#FFFFFF','#DCE4EE',22,2)
        rect((x,y+20,x+7,y+h-20),col,r=3)
        text(x+26,y+19,c['title'],34,'#1D334B',True,maxw=740)
        badge(x+w-24,y+18,c['tag'],col)
        yy=y+74
        if c.get('intro'):
            text(x+26,yy,c['intro'],25,'#68788D',maxw=988);yy+=40
        if c.get('pins'):
            pinrow(x+26,yy,c['pins'],col);yy+=80
        for s,size in c['lines']:
            text(x+26,yy,s,size,'#374B62',maxw=988);yy+=35
        assert yy<=y+h+10,(c['title'],yy,y+h)
        appearance(x,y,h,appearances[c['refs'][0] if c['refs'] else 'buttons'])

line([(60,2290),(4180,2290)],'#D7E0EB',2)
text(65,2323,'读图说明',25,'#244564',True)
text(212,2323,'树线表示功能归属，不代表实际电气布线。TX / RX 相对外接模块命名，主控收发交叉连接。',25,'#65768B')
text(65,2370,'针序说明',25,'#244564',True)
text(212,2370,'按扩展板针号标注，不代表插头正面从左到右。XH 当前封装间距 2.50mm；1 脚和安装方向按 PCB 标记、线束导通确认。',25,'#65768B')
text(65,2418,'外观说明',25,'#244564',True)
text(212,2418,'外观仅供认型，不按比例。XH、排母、排针、Type-C、98P 插座参考项目模型；XT60、MX1.25、按键为外形示意。',25,'#65768B')
text(65,2465,'资料依据',25,'#244564',True)
text(212,2465,'当前原生网表 + M2 接线表 + MG513 六针合同。8A 为降压芯片等级；MaixCAM2 六针额定 1A，实际负载须实测。',25,'#65768B')

assert all(0<=x and x+tw<W and 0<=y and y+size<H for x,y,tw,size,_ in text_boxes),'Text outside canvas'
svg.append('</svg>')
base=OUT/'CoralC5-Motor-M2.7-interface-tree-with-connectors'
outputs={ext:Path(str(base)+ext) for ext in ['.svg','.png','.json']}
outputs['.svg'].write_text('\n'.join(svg)+'\n',encoding='utf8')
ET.parse(outputs['.svg'])
im.save(outputs['.png'],optimize=True)
record={'date':'2026-11-07','revision':'M2.7','size':[W,H],'connector_count':len(actual),'covered_connectors':sorted(listed),'missing_connectors':sorted(actual-listed),'connector_appearances':appearances,'appearance_sources':'connector-appearance/sources.json','netlist_sha256':hashlib.sha256((MOTOR/'docs/netlist.xml').read_bytes()).hexdigest(),'sources':['Documentation/reference/m2-interfaces.md','Documentation/reference/motor-interface.md','Hardware/stm32c5a3/Hardware/CoralC5-Motor/docs/netlist.xml'],'outputs':[str(outputs[ext].relative_to(ROOT)) for ext in ['.png','.svg']]}
outputs['.json'].write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
print(json.dumps(record,ensure_ascii=False,indent=2))
