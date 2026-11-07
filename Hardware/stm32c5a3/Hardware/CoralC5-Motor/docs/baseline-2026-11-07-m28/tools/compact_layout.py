"""Explicit compact A4 schematic layout; symbols and notes retain normal size."""
# Compress only the custom socket symbol's row pitch, keeping normal-sized text.
for sub in nodes(edge,'symbol'):
    if sub[1].endswith(('_1_1','_2_1')):
        for pin in nodes(sub,'pin'):
            pos=node(pin,'at');pos[2]=num(float(pos[2])*.5)
        for rect in nodes(sub,'rectangle'):
            for tag in ['start','end']:
                pos=node(rect,tag);pos[2]=num(float(pos[2])*.5)
LIB['Motor:CoralC5_Streamline_Socket']=edge

# Rebuild the interface page around its compact connector representation.
oldcore=core
PARTS[:]=[part for part in PARTS if part['page']!=3]
core=Page('03_Core_Interface.kicad_sch','C5-EDGE-3 核心板接口与调试',3,paper='A4')
PAGES.remove(oldcore);PAGES.remove(core);PAGES.insert(2,core)
core.frame('核心板接口',12,14,218,110)
core.frame('调试与扩展',223,14,284,116)
port_names=set(ports)-{'MOTOR_RUN'};port_added=set()
for unit,x,y in [(1,62,65),(2,172,65),(3,253,37)]:
    pins=core.place('Motor:CoralC5_Streamline_Socket','J14','C5-EDGE-3 / 98P',x,y,FP['EDGE'],unit=unit,mpn='X1611WVS-98-9TV01',refpos=pt(x,y-43 if unit<3 else y-11),valpos=pt(x,y-39 if unit<3 else y-7))
    for pn,pp in pins.items():
        net=contactmap.get(pn,'GND' if pn in ['50','51'] else '')
        if net and net in used:
            name=used[net];dx=(3.81 if name in port_names and name not in port_added else 13.97)*(1 if pp[0]>s(x) else -1)
            end=pt(pp[0]+dx,pp[1]);core.wire(pp,end)
            if name in port_names and name not in port_added:
                direction='input' if name in ['V5_SYS','L_nFAULT','R_nFAULT','L_ISENSE','R_ISENSE','L_ENC_A','L_ENC_B','R_ENC_A','R_ENC_B'] else 'output'
                core.port(name,end,direction,180 if dx>0 else 0);port_added.add(name)
            else:core.label(name,end,180 if dx>0 else 0)
        else:core.nc(pp)
jd=core.place(J4,'J8','SWD / XH2.50',264,65,FP['XH4'],mpn='B4B-XH-A',refpos=pt(258,53),valpos=pt(258,57));terminal(core,jd,{'1':'V3V3','2':'SWDIO','3':'SWCLK','4':'GND'})
jh=core.place(H10,'J9','AUX',253,99,FP['H10'],refpos=pt(253,86),valpos=pt(253,90))
for pn,net in {'1':'V3V3','2':'GND','3':'AUX_I2C_SCL','4':'AUX_I2C_SDA','5':'AUX_UART_TX','6':'AUX_UART_RX','7':'AUX_ADC','8':'AUX_GPIO','9':'V5_SYS','10':'NRST'}.items():core.netpin(jh[pn],net,-13.97 if int(pn)%2 else 13.97)
core.frame('硬件停机回路',12,118,162,181)
stop=core.place(J2,'J10','RUN / STOP_NC',67,145,FP['PH2'],refpos=pt(67,132),valpos=pt(67,136))
ren=res(core,'R6','100R',45.72,145,rot=90);core.netpin(ren['1'],'MOTOR_EN_REQ',-20.32);core.wire(ren['2'],(54,145),(54,stop['1'][1]),stop['1'])
core.wire(stop['2'],(54,stop['2'][1]),(54,160),(94,160));core.port('MOTOR_RUN',pt(94,160),'output',180)
pd=res(core,'R7','10k',123,145);core.wire(pd['1'],(123,136));core.label('MOTOR_RUN',pt(123,136));core.wire(pd['2'],(123,160));core.gnd(pt(123,160))
core.flag(pt(184,128));core.label('V3V3',pt(184,128))
core.note('J10装短路帽，或接常闭停机开关\n开路、复位时默认停止电机',18,166)
core.note('严格沿用C5-EDGE-3针序\n空闲触点明确标记不连接\n该母座不能插计算机PCIe',176,137)
core.note('编码器及三伏三外接负载\n合计不得超过100mA',176,158)

# Rebuild the power and motor modules on explicit compact grids. The connector
# page above keeps the full finger contract; no physical component is omitted.
def fresh(old):
    PARTS[:]=[part for part in PARTS if part['page']!=old.pageno]
    page=Page(old.file,old.title,old.pageno,'A4');PAGES.remove(old);PAGES.remove(page);PAGES.insert(old.pageno-1,page);return page
qrev=copy.deepcopy(LIB[PMOS]);qrev[1]='AO4407A_Mirrored'
def reflect(obj):
    if not isinstance(obj,list):return
    if obj and obj[0] in ['at','start','end','mid','center'] and len(obj)>=3:
        obj[2]=num(-float(obj[2]))
        if obj[0]=='at' and len(obj)>3:obj[3]=num((-float(obj[3]))%360)
    for part in obj:
        if isinstance(part,list):reflect(part)
for sub in nodes(qrev,'symbol'):sub[1]=sub[1].replace('AO4407A_','AO4407A_Mirrored_');reflect(sub)
LIB['Motor:AO4407A_Mirrored']=qrev

p=fresh(p);p.frame('十二伏入口、拨动总开关与保护',12,14,285,100)
j=p.place(JACK,'J1','DC-005-5A-2.0',25.4,34.29,FP['DC'],mpn='DC-005-5A-2.0',refpos=pt(25.4,21.59),valpos=pt(31.75,25.4),fields={'LCSC':'C381116'})
p.nc(j['2']);p.wire(j['3'],(40.64,j['3'][1]),(40.64,50.8));p.gnd(pt(40.64,50.8));p.flag(pt(40.64,50.8))
f=p.place(FUSE,'F1','4A / 32V',46.99,j['1'][1],FP['F'],rot=90,mpn='0466004.NR');p.wire(j['1'],f['1'])
q1=p.place(PMOS,'Q1','AO4407A',71.12,26.67,FP['SO8'],rot=90,mpn='AO4407A',refpos=pt(71.12,16.51),valpos=pt(71.12,20.32))
q2=p.place('Motor:AO4407A_Mirrored','Q2','AO4407A',99.06,26.67,FP['SO8'],rot=90,mpn='AO4407A',refpos=pt(99.06,16.51),valpos=pt(99.06,20.32))
p.wire(f['2'],(60.96,f['2'][1]),(60.96,q1['5'][1]),q1['5']);p.wire(q1['1'],q2['1']);p.wire((85.09,q1['1'][1]),(85.09,17.78));p.label('SW_SOURCE',pt(85.09,17.78))
p.wire(q2['5'],(269.24,q2['5'][1]));p.port('VM',pt(269.24,q2['5'][1]),'output',180);p.wire((269.24,q2['5'][1]),(269.24,19.05));p.flag(pt(269.24,19.05))
p.wire(q1['4'],(71.12,44.45),(109.22,44.45));p.wire(q2['4'],(99.06,44.45))
rg=res(p,'R1','1M',85.09,57.15);p.wire(rg['1'],(85.09,44.45));p.wire(rg['2'],(85.09,69.85));p.label('SW_SOURCE',pt(85.09,69.85))
dz=p.place(DZEN,'D1','BZT52C12',109.22,57.15,FP['SOD123'],rot=90,mpn='BZT52C12',refpos=pt(124.46,55.88),valpos=pt(124.46,58.42));p.wire(dz['2'],(109.22,44.45));p.wire(dz['1'],(109.22,69.85),(85.09,69.85))
sw=p.place(SW,'SW1','SS12D07VG4',143.51,57.15,FP['SW'],mpn='SS12D07VG4',refpos=pt(143.51,43.18),valpos=pt(143.51,46.99),fields={'LCSC':'C431546'})
p.wire(sw['2'],(130.81,sw['2'][1]),(130.81,44.45),(109.22,44.45));p.wire(sw['1'],(165.1,sw['1'][1]));p.label('SW_SOURCE',pt(165.1,sw['1'][1]),180)
p.wire(sw['4'],(143.51,81.28));p.gnd(pt(143.51,81.28))
rr=res(p,'R2','100k',180.34,69.85);p.wire(sw['3'],(180.34,sw['3'][1]),rr['1']);p.wire(rr['2'],(180.34,81.28));p.gnd(pt(180.34,81.28));p.note('关',172.72,49.53);p.note('开',165.1,63.5)
tv=p.place(DZEN,'D2','SMAJ15A',207.01,50.8,FP['SMA'],rot=270,mpn='SMAJ15A');p.wire(tv['1'],(207.01,q2['5'][1]));p.wire(tv['2'],(207.01,66.04));p.gnd(pt(207.01,66.04))
bulk=p.place(CP,'C1','470u / 25V',240.03,50.8,FP['CP'],mpn='EEU-FR1E471');p.wire(bulk['1'],(240.03,q2['5'][1]));p.wire(bulk['2'],(240.03,66.04));p.gnd(pt(240.03,66.04))
p.note('三串锂电池9～12.6V，中心正极；拨动开关控制双MOS主通路\n关闭后电机和降压均断电，USB仍可单独供核心板',18,85.09)
p.frame('五伏同步降压与USB供电隔离',12,108,285,163.83)
u=p.place(AP,'U1','AP63205WU-7',127,137.16,FP['TSOT'],mpn='AP63205WU-7',refpos=pt(127,118.11),valpos=pt(127,121.92))
p.wire(u['3'],(104.14,u['3'][1]),(104.14,120.65),(45.72,120.65));p.label('VM',pt(45.72,120.65))
for ref,x,value,fp,mpn in [('C2',86.36,'10u / 50V','C0805','GRM21BR61H106KE43L'),('C3',102.87,'100n / 50V','C','')]:
    c=cap(p,ref,value,x,130.81,fp,mpn=mpn);p.wire(c['1'],(x,120.65));p.wire(c['2'],(x,140.97));p.gnd(pt(x,140.97))
rt=res(p,'R3','698k / 1%',45.72,130.81);rb=res(p,'R4','100k / 1%',45.72,153.67)
p.wire(rt['1'],(45.72,120.65));p.wire(rt['2'],(45.72,142.24),rb['1']);p.wire(rb['2'],(45.72,160.02));p.gnd(pt(45.72,160.02));p.wire((45.72,142.24),(77.47,142.24),(77.47,153.67),(109.22,153.67),(109.22,u['2'][1]),u['2'])
bst=cap(p,'C4','100n / 16V',151.13,120.65);p.wire(u['6'],(151.13,u['6'][1]),bst['2']);p.wire(bst['1'],(165.1,bst['1'][1]),(165.1,u['5'][1]),u['5'])
ind=p.place(L,'L1','4.7u / 4.5A',177.8,u['5'][1],FP['L'],rot=90,mpn='SRN6045TA-4R7M');p.wire(u['5'],ind['1']);p.wire(ind['2'],(241.3,u['5'][1]))
for ref,x in [('C5',201.93),('C6',224.79)]:
    c=cap(p,ref,'22u / 10V',x,142.24,'C1206',mpn='GRM31CR71A226KE15L');p.wire(c['1'],(x,u['5'][1]));p.wire(c['2'],(x,151.13));p.gnd(pt(x,151.13))
p.wire(u['1'],(157.48,u['1'][1]),(157.48,157.48),(238.76,157.48),(238.76,u['5'][1]));p.wire(u['4'],(127,157.48));p.gnd(pt(127,157.48))
iso=p.place(D,'D3','SS34',245.11,u['5'][1],FP['SMA'],rot=180,mpn='SS34');p.wire(iso['2'],(232.41,u['5'][1]));p.wire(iso['1'],(269.24,u['5'][1]));p.port('V5_SYS',pt(269.24,u['5'][1]),'output',180);p.wire((262.89,u['5'][1]),(262.89,128.27));p.flag(pt(262.89,128.27))
rl=res(p,'R5','2.2k',266.7,143.51);led=p.place(LED,'D4','LED_GREEN',266.7,153.67,FP['LED'],rot=90,mpn='LTST-C190KGKT',refpos=pt(251.46,153.67),valpos=pt(251.46,157.48));p.wire(rl['1'],(266.7,u['5'][1]));p.wire(rl['2'],led['2']);p.wire(led['1'],(266.7,161.29));p.gnd(pt(266.7,161.29))
p.note('五伏负载预算0.75A，核心板入口约4.6～4.9V\n按原厂配置4.7uH及两颗22uF，D3隔离USB反向供电',18,169.55)

ENC=chip('ENCODER_4P','J','ENCODER / XH2.50',[(1,'GND',7.62,3.81,180,'passive'),(2,'3V3',7.62,1.27,180,'passive'),(3,'A',7.62,-1.27,180,'passive'),(4,'B',7.62,-3.81,180,'passive')],FP['XH4'],rect=(-2.54,6.35,2.54,-6.35))
def compact_motor(old,side,drvref,bufref,mj,ej,base):
    page=fresh(old);page.frame('全桥驱动与电流检测',12,14,285,114)
    u=page.place(DRV,drvref,'DRV8874PWPR',139.7,55.88,FP['DRV'],mpn='DRV8874PWPR',refpos=pt(139.7,18.9),valpos=pt(139.7,22.86))
    page.wire(u['11'],(139.7,27.94),(245.11,27.94));page.port('VM',pt(245.11,27.94),'input',180)
    for ref,x,key,value in [(f'C{base+2}',195.58,C,'100n / 50V'),(f'C{base+3}',224.79,CP,'470u / 25V')]:
        c=page.place(key,ref,value,x,36.83,FP['CP'] if key==CP else FP['C'],mpn='EEU-FR1E471' if key==CP else '');page.wire(c['1'],(x,27.94));page.wire(c['2'],(x,41.91));page.gnd(pt(x,41.91))
    cv=cap(page,f'C{base+1}','100n / 16V',175.26,33.02);page.wire(cv['1'],(175.26,27.94));page.wire(cv['2'],(175.26,u['12'][1]),u['12'])
    cf=cap(page,f'C{base}','22n / 50V',175.26,63.5);page.wire(u['13'],(167.64,u['13'][1]),(167.64,cf['1'][1]),cf['1']);page.wire(u['14'],(167.64,u['14'][1]),(167.64,cf['2'][1]),cf['2'])
    jm=page.place(J2,mj,'MOTOR / 5.08',238.76,58.42,FP['M2'],mpn='MKDS 1,5/ 2-5,08',refpos=pt(238.76,50.8),valpos=pt(238.76,54.61))
    page.wire(u['8'],(208.28,u['8'][1]),(208.28,jm['1'][1]),jm['1']);page.wire(u['10'],(200.66,u['10'][1]),(200.66,jm['2'][1]),jm['2'])
    for pn,name in [('1',side+'_PWM'),('2',side+'_DIR'),('3','MOTOR_RUN'),('4',side+'_nFAULT')]:
        end=pt(109.22,u[pn][1]);page.wire(u[pn],end);page.port(name,end,'output' if pn=='4' else 'input')
    for pn in ['16','7','9','15','17']:page.wire(u[pn],(u[pn][0],85.09))
    page.wire((u['16'][0],85.09),(u['17'][0],85.09));page.gnd(pt(139.7,88.9));page.wire((139.7,85.09),(139.7,88.9))
    rf=res(page,f'R{base+3}','10k',113.03,67.31,refpos=pt(104.14,66.04),valpos=pt(104.14,68.58));page.wire(rf['1'],(113.03,u['4'][1]));page.wire(rf['2'],(113.03,76.2),(104.14,76.2));page.label('V3V3',pt(104.14,76.2))
    sl=res(page,f'R{base+4}','100k',88.9,99.06);page.wire(sl['1'],(88.9,90.17));page.label('MOTOR_RUN',pt(88.9,90.17));page.wire(sl['2'],(88.9,107.95));page.gnd(pt(88.9,107.95))
    ra=res(page,f'R{base}','10k / 1%',60.96,57.15);rb=res(page,f'R{base+1}','10k / 1%',60.96,78.74)
    page.wire(ra['1'],(60.96,48.26));page.label('V3V3',pt(60.96,48.26));page.wire(ra['2'],(60.96,68.58),rb['1']);page.wire(rb['2'],(60.96,88.9));page.gnd(pt(60.96,88.9))
    cr=cap(page,f'C{base+4}','100n / 16V',83.82,78.74,valpos=pt(86.36,83.82));page.wire(cr['1'],(83.82,68.58),(60.96,68.58));page.wire(cr['2'],(83.82,88.9));page.gnd(pt(83.82,88.9));page.wire((83.82,68.58),(96.52,68.58),(96.52,80.01),(116.84,80.01),(116.84,u['5'][1]),u['5'])
    page.wire(u['6'],(120.65,u['6'][1]),(120.65,92.71),(180.34,92.71))
    ir=res(page,f'R{base+2}','1.82k / 1%',139.7,100.33);page.wire(ir['1'],(139.7,92.71));page.wire(ir['2'],(139.7,109.22));page.gnd(pt(139.7,109.22))
    ci=cap(page,f'C{base+5}','10n / 16V',163.83,100.33);page.wire(ci['1'],(163.83,92.71));page.wire(ci['2'],(163.83,109.22));page.gnd(pt(163.83,109.22))
    rs=res(page,f'R{base+5}','1k',184.15,92.71,rot=90);page.wire(rs['2'],(208.28,92.71));adc=cap(page,f'C{base+6}','100n / 16V',208.28,101.6);page.wire(adc['1'],(208.28,92.71));page.wire(adc['2'],(208.28,110.49));page.gnd(pt(208.28,110.49));page.wire((208.28,92.71),(224.79,92.71));page.port(side+'_ISENSE',pt(224.79,92.71),'output',180)
    page.note('限流约2.0A，反馈0.819V/A\n固定关断时间限流，PWM建议20kHz\n停止时拉低nSLEEP进入滑行',196.85,76.2)
    page.frame('编码器供电、滤波与整形',12,120,285,180)
    je=page.place(ENC,ej,'ENC / XH2.50',25.4,142.24,FP['XH4'],mpn='B4B-XH-A',refpos=pt(25.4,129.54),valpos=pt(25.4,133.35))
    page.wire(je['1'],(40.64,je['1'][1]),(40.64,154.94));page.gnd(pt(40.64,154.94))
    page.wire(je['2'],(55.88,je['2'][1]),(55.88,127),(210.82,127));page.port('V3V3',pt(210.82,127),'input',180)
    bf=page.place(BUF,bufref,'SN74LVC2G17DCKR',160.02,149.86,FP['DCK'],mpn='SN74LVC2G17DCKR',refpos=pt(143.51,130.81),valpos=pt(143.51,134.62))
    page.wire(bf['5'],(160.02,127));page.wire(bf['2'],(160.02,173.99));page.gnd(pt(160.02,173.99))
    ar=res(page,f'R{base+6}','1k',88.9,146.05,rot=90);br=res(page,f'R{base+7}','1k',88.9,162.56,rot=90)
    page.wire(je['3'],(60.96,je['3'][1]),(60.96,146.05),ar['1']);page.wire(je['4'],(50.8,je['4'][1]),(50.8,162.56),br['1'])
    page.wire(ar['2'],(130.81,146.05),(130.81,bf['1'][1]),bf['1']);page.wire(br['2'],(130.81,162.56),(130.81,bf['3'][1]),bf['3'])
    pa=res(page,f'R{base+8}','10k',68.58,134.62);page.wire(pa['1'],(68.58,127));page.wire(pa['2'],(68.58,146.05))
    pb=res(page,f'R{base+9}','10k',68.58,170.18);page.wire(pb['1'],(68.58,162.56));page.wire(pb['2'],(68.58,177.8));page.label('V3V3',pt(68.58,177.8))
    ca=cap(page,f'C{base+7}','1n / 16V',111.76,142.24,rot=90);page.wire(ca['2'],(115.57,146.05));page.wire(ca['1'],(101.6,142.24));page.gnd(pt(101.6,142.24))
    cb=cap(page,f'C{base+8}','1n / 16V',114.3,171.45);page.wire(cb['1'],(114.3,162.56));page.wire(cb['2'],(114.3,179.07));page.gnd(pt(114.3,179.07))
    for pn,name in [('6',side+'_ENC_A'),('4',side+'_ENC_B')]:page.wire(bf[pn],(195.58,bf[pn][1]));page.port(name,pt(195.58,bf[pn][1]),'output',180)
    for ref,x,y,gy,value in [(f'C{base+9}',182.88,135.89,142.24,'100n / 16V'),(f'C{base+10}',83.82,134.62,140.97,'1u / 10V')]:
        c=cap(page,ref,value,x,y);page.wire(c['1'],(x,127));page.wire(c['2'],(x,gy));page.gnd(pt(x,gy))
    page.note('接口依次为地、三伏三、A、B\n先核对原电机线束\n禁止直接套用六针线序',215.9,139.7)
    return page
left=compact_motor(left,'L','U2','U3','J3','J4',10);right=compact_motor(right,'R','U4','U5','J5','J6',30)

root=fresh(root);root.note('双电机扩展板',17.78,15.24);root.note('MG513XP28_12V ×2，配套CoralC5-Core\n三串锂电池2800mAh，目标约一小时',17.78,24.13)
cp=[]
for i,name in enumerate(['L_PWM','L_DIR','MOTOR_RUN','L_nFAULT','L_ISENSE','L_ENC_A','L_ENC_B']):cp.append((name,'output' if name in ['L_PWM','L_DIR','MOTOR_RUN'] else 'input',95.25,53.34+i*3.81,0))
for i,name in zip([0,1,3,4,5,6],['R_PWM','R_DIR','R_nFAULT','R_ISENSE','R_ENC_A','R_ENC_B']):cp.append((name,'output' if name in ['R_PWM','R_DIR'] else 'input',95.25,123.19+i*3.81,0))
cp += [('V5_SYS','input',40.64,100.33,180),('V3V3','output',95.25,91.44,0)]
cc=sheet(core,'核心板接口',40.64,44.45,54.61,110.49,cp)
for child,side,y in [(left,'L',44.45),(right,'R',114.3)]:
    names=[side+'_PWM',side+'_DIR','MOTOR_RUN',side+'_nFAULT',side+'_ISENSE',side+'_ENC_A',side+'_ENC_B']
    defs=[(name,'output' if name.endswith(('nFAULT','ISENSE','ENC_A','ENC_B')) else 'input',175.26,y+8.89+i*3.81,180) for i,name in enumerate(names)]
    defs += [('VM','input',250.19,y+8.89,0),('V3V3','input',250.19,y+24.13,0)]
    mm=sheet(child,'左电机与编码器' if side=='L' else '右电机与编码器',175.26,y,74.93,45.72,defs)
    for name in names:
        if name=='MOTOR_RUN' and side=='R':root.wire(cc[name],(142.24,cc[name][1]),(142.24,104.14),(154.94,104.14),(154.94,mm[name][1]),mm[name])
        else:root.wire(cc[name],mm[name])
    for name in ['VM','V3V3']:root.wire(mm[name],(270.51,mm[name][1]));root.label(name,pt(270.51,mm[name][1]),180)
ps=sheet(p,'输入、总开关与降压',25.4,166.37,69.85,22.86,[('V5_SYS','output',25.4,173.99,180),('VM','output',95.25,181.61,0)])
root.wire(ps['V5_SYS'],(15.24,173.99),(15.24,100.33),cc['V5_SYS']);root.wire(ps['VM'],(113.03,181.61));root.label('VM',pt(113.03,181.61),180)
root.wire(cc['V3V3'],(119.38,91.44));root.label('V3V3',pt(119.38,91.44),180)
root.note('拨动开关切断主电源\n两路电机各自限流\n编码器用三伏三',120.65,170.18)
