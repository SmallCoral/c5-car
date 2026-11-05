"""Dual camera mast and adjustable head geometry, with explicitly assumed envelopes."""
import FreeCAD as App
import Part


def build_cameras(P,parent,group,add,box,cyl,link):
    V=App.Vector
    cfg=P['camera'];top=P['chassis']['plate_bottom']+P['chassis']['thickness']
    mx,my=cfg['mast_base_xy'];hz=top+cfg['head_plane_above_plate']
    cx,cy=cfg['maixcam_center_xy']
    foot=box(mx-14,my-14,top,28,28,3)
    for x in (mx-9,mx+9):
        for y in (my-9,my+9):foot=foot.cut(cyl(x,y,top-1,1.7,5))
    add(parent,'CameraMastFoot','双摄立柱底座 / 四孔安装',foot,'panel')
    outer=box(mx-10,my-10,top+3,20,20,250).cut(box(mx-8.5,my-8.5,top+2,17,17,252))
    add(parent,'CameraMastOuter','20mm方管 / 双摄升降外管',outer,'panel')
    inner_top=hz+46
    inner=box(mx-8,my-8,inner_top-224,16,16,224)
    inner=inner.cut(box(mx-6.5,my-6.5,inner_top-225,13,13,226))
    add(parent,'CameraMastInner','16mm方管 / 升降-60至+30mm参考',inner,'silver')
    collar=box(mx-12,my-12,top+244,24,24,14).cut(box(mx-10.2,my-10.2,top+243,20.4,20.4,16))
    add(parent,'CameraMastCollar','升降锁紧环',collar,'coral')
    add(parent,'CameraHeightKnob','升降锁紧旋钮',cyl(mx,my+12,top+251,5,9,(0,1,0)),'black')
    add(parent,'CameraBaseBrace','立柱斜撑',link((mx+12,my,top+4),(mx+10,my,top+50),2),'cyan')
    # The horizontal bridge sits above both cameras; neither lens looks through it.
    boom_z=hz+38
    arm_x=box(mx-10,my-8,boom_z-8,cx-mx+20,16,16)
    arm_x=arm_x.cut(box(mx-11,my-6.5,boom_z-6.5,cx-mx+22,13,13))
    arm_y=box(cx-8,-66,boom_z-8,16,my+74,16)
    arm_y=arm_y.cut(box(cx-6.5,-67,boom_z-6.5,13,my+76,13))
    add(parent,'CameraBoom','双摄悬臂 / 顶部横梁',arm_x.fuse(arm_y),'panel')
    for z in (top+90,top+170,top+255):
        width=20 if z<top+253 else 16
        clip=box(mx-12,my-11,z,24,22,4).cut(box(mx-width/2,my-width/2,z-1,width,width,6))
        add(parent,'CameraCableClip'+str(int(z)),'立柱线束夹 / 电源与UART',clip,'cyan')

    detect=group('MaixcamHead','检测摄像头 / MaixCAM可调夹座',parent)
    detect.Placement=App.Placement(V(cx,cy,hz),App.Rotation(V(1,0,0),cfg['maixcam_pitch_deg']))
    w,d,h=cfg['maixcam_body_envelope']
    tray=box(-38,-26,0,76,52,3).cut(box(-26,-16,-1,52,32,5))
    for x in (-31,31):
        for y in (-22,22):tray=tray.cut(cyl(x,y,-1,1.7,5))
    add(detect,'MaixcamTray','MaixCAM滑槽夹持托 / 孔位待实测',tray,'cyan')
    add(detect,'MaixcamBody','MaixCAM检测端 / 65×45×23占位待实测',box(-w/2,-d/2,-h,w,d,h),'black','unmeasured_camera_envelope')
    add(detect,'MaixcamScreen','MaixCAM屏幕参考 / 朝上留操作空间',box(-25,-17,.05,50,34,.2),'glass','unmeasured_camera_envelope')
    add(detect,'MaixcamLens','MaixCAM镜头 / 朝下检测钢球',cyl(0,0,-h-11,6,11),'black','unmeasured_camera_envelope')
    add(detect,'MaixcamOptic','MaixCAM光学面参考',cyl(0,0,-h-11.2,4.8,.2),'cyan','unmeasured_camera_envelope')
    for sign in (-1,1):
        x=sign*(w/2+1.5)
        clamp=box(x-1.5,-18,-h-2,3,36,h+5)
        clamp=clamp.fuse(box(x-4,-18,-h-2,8,36,2))
        add(detect,'MaixcamClamp'+str(sign).replace('-','N'),'MaixCAM可调侧夹 / 软垫待定',clamp,'panel')
        # Two short trunnions leave the camera body and screen free.
        add(detect,'MaixcamPivot'+str(sign).replace('-','N'),'MaixCAM俯仰短轴',cyl(sign*36 if sign>0 else -46,0,1.5,2.5,10,(1,0,0)),'silver')
        cheek=box(cx+sign*42-2,cy-9,hz,4,18,30)
        cheek=cheek.cut(cyl(cx+sign*42-3,cy,hz+1.5,2.6,6,(1,0,0)))
        add(parent,'MaixcamYoke'+str(sign).replace('-','N'),'MaixCAM俯仰支耳',cheek,'panel')
    add(parent,'MaixcamYokeBridge','检测端支架横梁',box(cx-44,cy-6,hz+26,88,12,4),'panel')
    add(parent,'MaixcamPitchKnob','检测相机调角旋钮',cyl(cx+44,cy,hz+1.5,5,6,(1,0,0)),'coral')

    sx,sy=cfg['stream_center_xy'];sh=hz-8
    stream=group('StreamHead','图传摄像头 / OV2640独立调角座',parent)
    stream.Placement=App.Placement(V(sx,sy,sh),App.Rotation(V(1,0,0),cfg['stream_pitch_deg']))
    tray=box(-23,-18,0,46,36,2).cut(box(-11,-8,-1,22,16,4))
    for x in (-18,18):
        for y in (-14,14):tray=tray.cut(cyl(x,y,-1,1.7,4))
    add(stream,'StreamCameraTray','OV2640可调夹座 / 具体模块孔距未锁定',tray,'coral')
    w,d,h=cfg['stream_board_envelope']
    add(stream,'StreamCameraPCB','OV2640图传端 / 36×26包络待实测',box(-w/2,-d/2,-h,w,d,h),'green','unmeasured_camera_envelope')
    add(stream,'StreamCameraLens','OV2640图传镜头 / 向球槽倾斜',cyl(0,0,-14,5,12.4),'black','unmeasured_camera_envelope')
    add(stream,'StreamCameraOptic','图传镜头光学面参考',cyl(0,0,-14.2,4,.2),'glass','unmeasured_camera_envelope')
    for sign in (-1,1):
        add(stream,'StreamClamp'+str(sign).replace('-','N'),'图传组件边夹',box(sign*19-1,-12,-3,2,24,5),'panel')
    # Side arms connect the small head to the overhead bridge, leaving its optical path open.
    for sign in (-1,1):
        add(parent,'StreamYoke'+str(sign).replace('-','N'),'图传相机调角支耳',box(sx+sign*25-2,sy-5,sh,4,10,hz+30-sh),'panel')
    add(parent,'StreamYokeBridge','图传端支架横梁',box(sx-27,sy-5,hz+26,54,10,4),'panel')
    for x in (-29,22):add(stream,'StreamPitchPin'+str(x).replace('-','N'),'图传相机俯仰短轴参考',cyl(x,0,1,2.5,7,(1,0,0)),'silver')
    # An electronics space close to the image sensor avoids assuming a long raw DVP cable.
    add(parent,'StreamProcessorTray','图传处理板近端托盘',box(mx-26,my+8,hz-49,52,36,3),'panel')
    add(parent,'StreamProcessorSpace','图传处理板就近安装空间 / 自研板未定',box(mx-24,my+15,hz-45,48,28,3),'green','unmeasured_electronics_envelope')
