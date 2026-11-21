"""MaixCAM2 绿管小球识别：YOLO + 一维卡尔曼 + UART V2。

流程：采集640x480 -> 绿管重叠窗口 -> YOLO与球体轮廓 -> 连续确认/跟踪
      -> 带实测年龄的 UART V2 -> 全宽预览。
默认模型输入为原始裁切；显示中值滤波只改善预览，不代表原始传感器噪声消失。
"""

from maix import camera, display, image, nn, app, uart, gpio, pinmap, err
from math import sqrt
from struct import pack
import cv2
import numpy as np
import os
import signal
import time


# ===== 模型与采集：先核对启动日志，不要只看文件名是否上传 =====
MODEL_PATH = "/root/models/wuhaha/feball/model_331496.mud"  # 新训练模型；须与其引用的权重一起部署
IMG_W, IMG_H = 640, 480  # 原始采集宽、高，单位像素
CAMERA_FPS = 60  # 保留现场设置；这是请求值，常见MaixCAM2模组先对比60，不保证实际120
CAMERA_BUFFER_NUM = 1  # 采集缓存数量；小缓存减轻排队，过小可能丢帧
MANUAL_EXPOSURE_US = 2300  # 曝光微秒；80cm/s、20px/cm时理论拖影约2.4px
MANUAL_GAIN = 15  # 保留当前增益；单位由模组/API决定，不是这里假定的dB。光照足够时对比20/30

# ===== 现场几何标定：保留你最新的零点偏移与裁切 =====
PIXEL_SCALE = IMG_W / 320.0  # 相对旧320宽画面的比例；只适用于视野不变、等比例成像
CENTER_X_PX = 137.0 * PIXEL_SCALE + 10  # 当前机械零点X=284；蓝线、串口零点都用它
PIXELS_PER_CM = 10.0 * PIXEL_SCALE  # 当前20px/cm；重新实测时填“球心像素变化/实际厘米数”
MAX_ABS_SPEED_CM_S = 80.0  # 物理最高速度；过小会拒绝快速运动的真实球
CROP_Y, CROP_H = 192, 128  # 为钢球上下保留训练所需的背景，全宽预览仍使用原图 X
TILE_W = 320  # 224x224 模型；局部输入使球在模型中的直径约为原来的两倍
SEARCH_ORIGINS = (0, (IMG_W - TILE_W) // 2, IMG_W - TILE_W)

# ===== YOLO：网络、置信度及NMS参数保持现场版本 =====
TRACK_CONF_THRESHOLD = 0.2  # 传给detect的最低置信度；更低的框不会出现在诊断计数里
ACQUIRE_CONF_THRESHOLD = 0.3  # 首次连续确认所需最低置信度
REACQUIRE_GLOBAL_CONF_THRESHOLD = 0.25  # 失锁后全图重新确认所需最低置信度
IOU_THRESHOLD = 0.45  # NMS重叠框抑制阈值，与卡尔曼门限无关
MIN_BOX_SIZE_PX = 4 * PIXEL_SCALE  # 当前8；检测框宽、高都必须满足，过滤小噪点
MAX_BOX_SIZE_PX = 55 * PIXEL_SCALE  # 宽高上限；几何复核还要求球左右两侧有管面

# ===== 输入预处理：绿色管道重新训练后，先以原始RGB作基线 =====
ENABLE_YOLO_LIGHT_NORMALIZATION = False  # 默认关闭旧Gamma，避免背景变色导致训练/推理外观不一致
YOLO_DENOISE = False  # 仅在AB对比后开启：3x3中值会改变模型输入，也可能抹掉小球细节
LIGHT_TARGET_LUMA = 115.0  # Gamma开启时的参考亮度0~255，必须取训练图，不代表绿管应有的亮度
LIGHT_DEADBAND = 8.0  # 与目标亮度差小于此值时，不做Gamma
LIGHT_EMA_ALPHA = 0.1  # 背景亮度EMA权重；越大越快响应光照变化
LIGHT_GAMMA_MIN = 0.80  # Gamma最小值；小于1提亮，过小会放大暗部颗粒
LIGHT_GAMMA_MAX = 1.75  # Gamma最大值；大于1压暗

# ===== 显示与诊断：显示去噪不进入模型，不改变卡尔曼/串口结果 =====
DISPLAY_DENOISE = True  # 3x3中值滤波显示副本，减轻随机亮点/暗点/彩色颗粒
SHOW_DETECTION_BOXES = True  # 红框=YOLO原始框，绿框=尺寸筛选通过；都不代表已锁定
MAX_DEBUG_BOXES = 8  # 屏幕最多绘制多少框，避免噪声误框大量拖慢显示
DIAGNOSTIC_PERIOD_S = 1.0  # 串口控制台诊断输出间隔，单位秒

# ===== 卡尔曼与关联门限：长度量乘比例，方差量乘比例平方 =====
KF_MEAS_STD_PX = 2.5 * PIXEL_SCALE  # 当前5px，检测球心抖动标准差；增大会更稳但滞后
KF_ACCEL_PSD = 60000.0 * PIXEL_SCALE ** 2  # 当前240000px²/s³；增大后更能跟随加速/转向
KF_START_SPEED_STD = 300.0 * PIXEL_SCALE  # 当前600px/s，初锁时速度不确定性
KF_GATE_SIGMA = 3.0  # 预测门宽与位置/测量标准差之间的倍数
GATE_MIN_PX = 18.0 * PIXEL_SCALE  # 当前36，预测关联门最小半径，误吸噪点时可减小
GATE_MAX_PX = 64.0 * PIXEL_SCALE  # 当前128，预测关联门最大半径，限制远处误框
CONFIRM_FRAMES = 3  # 至少2；连续确认帧数，越大越不易偶发误锁，但更慢锁定
CONFIRM_MARGIN_PX = 8.0 * PIXEL_SCALE  # 当前16，连续确认时额外允许的位置偏差
CONFIRM_MAX_GAP_S = 0.10  # 连续确认的最大帧间隔；低于10Hz时需要检查此项
PREDICT_VALID_S = 0.15  # 真实测量丢失超过150ms，输出不再标记有效
TRACK_RESET_S = 0.24  # 超过240ms仍未实测，放弃旧滤波状态重新确认

# ===== UART V2：单位及帧格式与原控制器兼容 =====
UART_BAUD = 115200  # UART4波特率
TX_RATE_HZ = 50.0  # 发送上限Hz，实际还取决于主循环速度
STATUS_VALID = 0x01  # 位置可用于控制
STATUS_MEASURED = 0x02  # 本轮成功融合真实YOLO测量
STATUS_PREDICTED = 0x04  # 本轮无实测，但处在允许的短时预测期
STATUS_OUTLIER = 0x08  # 本轮有异常候选且未采纳实测；原CV兜底位0x10不再使用
MAX_SPEED_PX_S = MAX_ABS_SPEED_CM_S * PIXELS_PER_CM  # 物理速度上限换成px/s
MEAS_VARIANCE = KF_MEAS_STD_PX ** 2  # 基础位置测量方差，单位px²


def clamp(value, low, high):
    """把数值value限制在闭区间[low, high]。"""
    return max(low, min(high, value))


GAMMA_LEVELS = np.arange(256, dtype=np.float32) / 255.0  # 归一化灰度级查表输入
TARGET_LOG_LUMA = np.log(clamp(LIGHT_TARGET_LUMA, 5.0, 250.0) / 255.0)  # Gamma公式的固定目标项


def crop_pipe(frame):
    """从原始frame裁出现场设置的水管带；不缩放、不移动X零点。"""
    return frame.crop(0, CROP_Y, IMG_W, CROP_H)


def crop_to_gray(crop_img):
    """读取RGB像素并返回独立灰度图；只用于亮度诊断和可选Gamma。"""
    rgb = image.image2cv(crop_img, ensure_bgr=False, copy=False)  # 仅读取的RGB数组视图
    return cv2.cvtColor(rgb, cv2.COLOR_RGB2GRAY)


def median_image(source):
    """返回3x3中值去噪的新图像；不覆盖source，保留输入与显示的隔离。"""
    rgb = image.image2cv(source, ensure_bgr=False, copy=False)  # 仅读取源图
    filtered = cv2.medianBlur(rgb, 3)  # 每通道空间中值，针对随机脉冲点；不是时间平均
    return image.cv2image(filtered, bgr=False, copy=True)  # 拥有独立像素，避免数组生命周期问题


def prepare_yolo_image(crop_img, gray, previous_luma):
    """可选Gamma。输入原图、灰度图、上次EMA；返回(新图,原始亮度,新EMA,Gamma)。"""
    raw_luma = float(np.median(gray))  # 背景占多数，所以这个值不等同于球的亮度
    if previous_luma is None:
        smoothed_luma = raw_luma  # 首帧直接初始化亮度均值
    else:
        smoothed_luma = ((1.0 - LIGHT_EMA_ALPHA) * previous_luma +
                         LIGHT_EMA_ALPHA * raw_luma)
    if abs(smoothed_luma - LIGHT_TARGET_LUMA) <= LIGHT_DEADBAND:
        return crop_img, raw_luma, smoothed_luma, 1.0

    source_level = clamp(smoothed_luma, 5.0, 250.0) / 255.0  # 避免log(0)或接近log(1)
    gamma = float(clamp(TARGET_LOG_LUMA / np.log(source_level), LIGHT_GAMMA_MIN, LIGHT_GAMMA_MAX))
    rgb = image.image2cv(crop_img, ensure_bgr=False, copy=False)
    lut = np.clip(np.power(GAMMA_LEVELS, gamma) * 255.0 + 0.5, 0, 255).astype(np.uint8)
    # 每个RGB通道都使用同一LUT，仍可能改变色彩外观；须与训练输入一致。
    normalized_rgb = cv2.LUT(rgb, lut)
    normalized_img = image.cv2image(normalized_rgb, bgr=False, copy=True)
    return normalized_img, raw_luma, smoothed_luma, gamma


class KalmanX:
    """一维匀速卡尔曼滤波器，状态为绝对横坐标 x(px) 和速度 v(px/s)。"""

    def __init__(self, x, speed, t, variance):
        """用首次确认的位置、初速度、观测时间 t 和位置测量方差建立滤波器。"""
        # x/v 为状态，t 为最近一次预测或校正所对应的图像时间。
        self.x, self.v, self.t = x, speed, t
        # pxx=位置方差(px²)，pxv=位置/速度协方差(px²/s)，pvv=速度方差(px²/s²)。
        self.pxx, self.pxv, self.pvv = variance, 0.0, KF_START_SPEED_STD ** 2

    def predict(self, t):
        """把状态与协方差预测到观测时间 t；dt 使用实际时间差，单位秒。"""
        dt = t - self.t
        if dt <= 0:
            return
        self.x += self.v * dt
        q_dt = KF_ACCEL_PSD * dt  # 连续白噪声加速度模型对当前时间段的噪声量
        self.pxx += dt * (2 * self.pxv + dt * (self.pvv + q_dt / 3))
        self.pxv += dt * (self.pvv + q_dt / 2)
        self.pvv += q_dt
        self.t = t

    def correct(self, x, variance):
        """用检测位置 x 和测量方差 variance 校正预测，限制最大物理速度。"""
        innovation = x - self.x  # 创新：真实检测与预测位置之间的差值
        total_var = self.pxx + variance  # 创新方差，同时考虑预测和测量不确定性
        kx, kv = self.pxx / total_var, self.pxv / total_var  # 位置/速度各自的卡尔曼增益
        self.x += kx * innovation
        self.v = clamp(self.v + kv * innovation, -MAX_SPEED_PX_S, MAX_SPEED_PX_S)
        self.pvv = max(0.0, self.pvv - kv * self.pxv)
        self.pxx, self.pxv = variance * kx, variance * kv


def steel_center(rgb, obj):
    """在模型框中央的几行寻找被钢球遮住的绿色管面，返回窗口内球心。"""
    if not (MIN_BOX_SIZE_PX <= obj.w <= MAX_BOX_SIZE_PX and
            MIN_BOX_SIZE_PX <= obj.h <= MAX_BOX_SIZE_PX and 0.45 <= obj.w / obj.h <= 2.2):
        return None
    cx, cy = round(obj.x + obj.w / 2), round(obj.y + obj.h / 2)
    span = max(16, min(52, round(max(obj.w, obj.h) * 1.55)))
    dy = max(2, min(5, round(obj.h * .18)))
    x0, x1 = max(0, cx - span), min(rgb.shape[1], cx + span + 1)
    band = rgb[max(0, cy-dy):min(rgb.shape[0], cy+dy+1), x0:x1]
    if not band.size or min(cx - x0, x1 - cx - 1) < obj.w / 2 + 4:
        return None
    red, green, blue = band.astype(np.int16).transpose(2, 0, 1)
    # 顶灯反光会把绿管照成青色；实测反光处仍明显缺红，蓝通道可略高于绿。
    pipe = (green - red > np.maximum(22, green // 4)) & (blue - green <= green // 5)
    if min(pipe[:, :5].mean(), pipe[:, -5:].mean()) < .55:
        return None
    metal = np.count_nonzero(pipe, axis=0) * 2 < band.shape[0]
    metal[1:-1] |= metal[:-2] & metal[2:]
    edges = np.flatnonzero(np.diff(np.r_[False, metal, False]))
    choices = []
    for left, right in edges.reshape(-1, 2):
        width = right - left
        x = x0 + (left + right - 1) / 2
        if max(6, .45 * obj.w) <= width <= 1.8 * obj.w + 3 and abs(x - cx) <= .55 * obj.w:
            choices.append(float(x))
    return min(choices, key=lambda x: abs(x - cx)) if choices else None


# 从检测结果生成可用于跟踪的绝对坐标候选。
def candidates_from(objs, tile, origin):
    candidates, boxes = [], []
    if len(objs):
        rgb = image.image2cv(tile, ensure_bgr=False, copy=False)
        for obj in objs:
            center = steel_center(rgb, obj)
            if center is not None:
                candidates.append((origin + center, float(obj.score)))
            boxes.append((origin + obj.x, obj.y, obj.w, obj.h, center is not None))
    return candidates, boxes


def measurement_variance(score):
    # 低置信度框仍可补测，但在滤波里占的权重较小。
    """把框置信度 score 转成位置测量方差；低置信度框在校正中权重更低。"""
    return MEAS_VARIANCE / clamp(score, TRACK_CONF_THRESHOLD, 1.0)


class BallTracker:
    """管理连续帧锁定、卡尔曼选框、急转向重锁、短时预测和失效状态。"""
    def __init__(self):
        """建立未锁定状态；尚无真实位置时，默认横坐标为已标定的零点。"""
        self.kf = None  # 未锁定时为空；锁定后保存 KalmanX 实例
        self.pending = None  # 待确认轨迹：(首个X,首时刻,上个X,上时刻,连续次数)
        self.seen = False  # 本次运行是否曾成功锁定；决定首次/重捕获置信度门槛
        self.last_x = CENTER_X_PX  # 上次成功采用的原始测量横坐标，不是预测值
        self.last_measure_t = None  # 最近真实测量时间；预测不会刷新它
        self.search_index = 0

    # 依据待确认位置或已锁定位置计算下一次局部视野的起点。
    def tile_origin(self, t):
        if self.pending is not None and t - self.pending[3] <= CONFIRM_MAX_GAP_S:
            x = self.pending[2]
        elif self.kf is not None and t - self.last_measure_t <= CONFIRM_MAX_GAP_S:
            x = self.kf.x + self.kf.v * max(0.0, t - self.kf.t)
        else:
            origin = SEARCH_ORIGINS[self.search_index]
            self.search_index = (self.search_index + 1) % len(SEARCH_ORIGINS)
            return origin
        return int(clamp(round(x) - TILE_W // 2, 0, IMG_W - TILE_W))

    def confirm(self, candidates, t):
        """连续确认高置信度且位移合理的候选；成功建立滤波器时返回 True。"""
        threshold = REACQUIRE_GLOBAL_CONF_THRESHOLD if self.seen else ACQUIRE_CONF_THRESHOLD
        eligible = [(x, score) for x, score in candidates if score >= threshold]  # 可用于连续确认的框
        if self.pending is not None and t - self.pending[3] > CONFIRM_MAX_GAP_S:
            self.pending = None
        if not eligible:
            return False

        if self.pending is not None:
            first_x, first_t, prev_x, prev_t, count = self.pending
            dt = t - prev_t
            if 0 < dt <= CONFIRM_MAX_GAP_S:
                limit = CONFIRM_MARGIN_PX + MAX_SPEED_PX_S * dt  # 最高速度位移 + 容许误差
                nearest = min(eligible, key=lambda c: (abs(c[0] - prev_x), -c[1]))
                x, score = nearest
                if abs(x - prev_x) <= limit:
                    count += 1
                    if count >= CONFIRM_FRAMES:
                        v = clamp((x - first_x) / (t - first_t),
                                  -MAX_SPEED_PX_S, MAX_SPEED_PX_S)
                        self.kf = KalmanX(x, v, t, measurement_variance(score))
                        self.last_x, self.last_measure_t = x, t
                        self.pending, self.seen = None, True
                        return True
                    self.pending = first_x, first_t, x, t, count
                    return False

        x, _ = max(eligible, key=lambda c: c[1])
        self.pending = x, t, x, t, 1
        return False

    def observe(self, candidates, t):
        """融合检测图对应时刻 t 的候选，返回 (本次成功实测, 拒绝了异常候选)。"""
        if self.kf is not None and t - self.last_measure_t > TRACK_RESET_S:
            self.kf = None  # 未锁定时为空；锁定后保存 KalmanX 实例
        if self.kf is None:
            return self.confirm(candidates, t), False
        if t <= self.kf.t:
            return False, False

        kf = self.kf
        kf.predict(t)
        reachable = GATE_MIN_PX + MAX_SPEED_PX_S * (t - self.last_measure_t)  # 从上次实测出发的可达范围
        best = None  # 最优候选：(排序代价, X, 测量方差)
        alternate = []  # 可达但在预测门外的候选，用于急转向重锁
        for x, score in candidates:
            if abs(x - self.last_x) > reachable:
                continue
            variance = measurement_variance(score)
            sigma = sqrt(kf.pxx + variance)  # 预测与测量共同决定的不确定性
            gate = clamp(KF_GATE_SIGMA * sigma, GATE_MIN_PX, GATE_MAX_PX)
            residual = abs(x - kf.x)  # 候选与预测位置的距离，单位像素
            if residual > gate:
                alternate.append((x, score))
                continue
            # 先看和预测轨迹的距离，置信度用于小幅修正排序。
            cost = residual / gate + 0.25 * (1.0 - score)
            if best is None or cost < best[0]:
                best = cost, x, variance

        if best is None:
            # 急转向可能跑出预测门。物理上来得及到达的位置可重新连续确认。
            measured = self.confirm(alternate, t)
            return measured, bool(candidates) and not measured

        self.pending = None  # 待确认轨迹：(首个X,首时刻,上个X,上时刻,连续次数)
        _, x, variance = best
        kf.correct(x, variance)
        self.last_x, self.last_measure_t = x, t
        return True, False

    def output(self, t, measured, outlier):
        """在输出时间 t 外推位置，返回 (绝对 X, 像素速度, 实测年龄 ms, 状态位)。不改滤波内部时间。"""
        status = STATUS_OUTLIER if outlier else 0
        if self.last_measure_t is None:
            return self.last_x, 0.0, 65535, status
        age = max(0.0, t - self.last_measure_t)  # 距离真实测量过去多久，单位秒
        age_ms = min(65535, round(age * 1000.0))
        if self.kf is None or age > PREDICT_VALID_S:
            return self.last_x, 0.0, age_ms, status

        # 不改内部状态：下一次双缓冲测量仍会回到对应的采集时刻融合。
        x = self.kf.x + self.kf.v * max(0.0, t - self.kf.t)
        status |= STATUS_VALID | (STATUS_MEASURED if measured else STATUS_PREDICTED)
        return clamp(x, 0, IMG_W - 1), self.kf.v, age_ms, status


def crc8_table_entry(value):
    """计算一个字节的 CRC-8/ATM 查表项；多项式 0x07。"""
    for _ in range(8):
        value = ((value << 1) ^ (0x07 if value & 0x80 else 0)) & 0xFF
    return value


CRC8_TABLE = bytes(crc8_table_entry(value) for value in range(256))  # 启动时生成256字节查表


def crc8(data):
    """对 data 字节序列计算 CRC-8/ATM，初值为 0，用于串口校验。"""
    crc = 0
    for byte in data:
        crc = CRC8_TABLE[crc ^ byte]
    return crc


def send_v2(serial, sequence, status, x, v, age_ms):
    """把 x(px)、v(px/s) 换算为原 V2 的位置/速度单位，打包16字节后写入 UART。"""
    position = round((x - CENTER_X_PX) / PIXELS_PER_CM * 100.0)  # 有符号位移，0.01 cm/单位
    speed = round(v / PIXELS_PER_CM * 10.0)  # 有符号速度，0.1 cm/s/单位
    # 小端负载：版本、状态、序号、绝对X、相对位移、速度、测量年龄；不改控制器协议。
    payload = pack("<BBHhhhH", 2, status, sequence,
                   int(clamp(round(x), 0, IMG_W - 1)),
                   int(clamp(position, -32768, 32767)),
                   int(clamp(speed, -32768, 32767)), age_ms)
    serial.write(b'\xaa\x55' + payload + bytes((crc8(payload), 0x17)))


# 将视觉跟踪的更新节拍与 UART 的定期发送分开。
class BallUart:
    # 清空发送序号、定时基准和上次实测标记。
    def __init__(self):
        self.sequence = 0
        self.next_tx = None
        self.last_sent_measure = None

    # 到发送时刻才打包，区分新实测与沿旧状态外推的数据。
    def poll(self, serial, tracker, now, outlier):
        if self.next_tx is None:
            self.next_tx = now
        if now < self.next_tx:
            return None
        interval = 1.0 / TX_RATE_HZ
        self.next_tx += (int((now - self.next_tx) / interval) + 1) * interval
        stamp = tracker.last_measure_t
        fresh = stamp is not None and stamp != self.last_sent_measure and now - stamp <= PREDICT_VALID_S
        x, v, age, status = tracker.output(now, fresh, outlier and not fresh)
        send_v2(serial, self.sequence, status, x, v, age)
        self.sequence = (self.sequence + 1) & 0xffff
        if status & STATUS_MEASURED:
            self.last_sent_measure = stamp
        return status


def resolve_model_path():
    """优先加载MODEL_PATH；仅允许同名新模型在脚本旁回退，绝不静默加载旧模型。"""
    local_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), os.path.basename(MODEL_PATH))
    for path in (MODEL_PATH, local_path):
        if os.path.isfile(path):
            return os.path.abspath(path)
    raise FileNotFoundError("New model missing: {} or {}. Deploy MUD and its weights together.".format(
        MODEL_PATH, local_path))


def configure_camera(cam):
    """独立设置曝光和增益，并分别回读；一项失败不跳过另一项，也不伪称设置成功。"""
    for name, requested in (("exposure", MANUAL_EXPOSURE_US), ("gain", MANUAL_GAIN)):
        try:
            setter = getattr(cam, name)  # MaixPy相机参数接口，同一方法不传参数时为读取
        except AttributeError as error:
            print("Camera {} API MISSING: {}".format(name, error))
            continue
        try:
            setter(requested)
        except Exception as error:
            print("Camera {} SET FAILED: {}".format(name, error))
        try:
            print("Camera {} requested={}, actual={}".format(name, requested, setter()))
        except Exception as error:
            print("Camera {} READ FAILED: {}".format(name, error))
    try:
        cam.skip_frames(30)  # 等待参数稳定；帧数不是毫秒
    except Exception as error:
        print("Camera warmup failed:", error)


def show_tracking(dis, crop_img, x, v, age, status, boxes=(), diagnostic=None, input_mode=None):
    """在独立显示副本上画原始框、位置线与诊断；不改变模型输入或控制数据。"""
    global DISPLAY_DENOISE  # 显示滤波失败时关闭本次运行的开关，避免反复报错影响控制
    if DISPLAY_DENOISE:
        try:
            view = median_image(crop_img)
        except Exception as error:
            DISPLAY_DENOISE = False
            print("Display median disabled:", error)
            view = crop_img.copy()
    else:
        view = crop_img.copy()
    if SHOW_DETECTION_BOXES:
        for bx, by, bw, bh, accepted in boxes[:MAX_DEBUG_BOXES]:
            color = image.COLOR_GREEN if accepted else image.COLOR_RED
            left, top = int(clamp(bx, 0, IMG_W - 1)), int(clamp(by, 0, CROP_H - 1))
            width = int(clamp(bw, 1, IMG_W - left))
            height = int(clamp(bh, 1, CROP_H - top))
            view.draw_rect(left, top, width, height, color=color)

    zero = round(CENTER_X_PX)  # 蓝色机械零点线，使用裁切图横坐标
    view.draw_line(zero, 0, zero, CROP_H - 1, color=image.COLOR_BLUE, thickness=2)
    if status & STATUS_VALID:
        measured = bool(status & STATUS_MEASURED)
        label, color = ("YOLO", image.COLOR_GREEN) if measured else ("PRED", image.COLOR_YELLOW)
        xp = round(x)  # 当前输出的滤波/预测横坐标
        view.draw_line(xp, 0, xp, CROP_H - 1, color=color, thickness=2)
        position_cm, speed_cm_s = (x - CENTER_X_PX) / PIXELS_PER_CM, v / PIXELS_PER_CM
        text = f"{label} {position_cm:+.2f}cm {speed_cm_s:+.1f}cm/s"
    else:
        color = image.COLOR_YELLOW
        text = "SEARCH" if age == 65535 else f"LOST {age}ms"
    view.draw_string(5, 5, text, color=color, scale=1.0)
    if diagnostic is not None:
        raw_count, kept_count, max_score, luma, gamma, fps = diagnostic
        if input_mode is None:
            input_mode = ("MED3" if YOLO_DENOISE else "RAW") + ("+G" if ENABLE_YOLO_LIGHT_NORMALIZATION else "")
        # D 为原始框数，C 为通过管面和钢球轮廓复核的候选数。
        view.draw_string(5, 25, f"D:{raw_count} C:{kept_count} S:{max_score:.2f} L:{luma:.0f} F:{fps:.0f}",
                         color=image.COLOR_YELLOW, scale=1.0)
        view.draw_string(5, 45, f"G:{gamma:.2f} INPUT:{input_mode} "
                               f"DISP:{'MED3' if DISPLAY_DENOISE else 'RAW'}",
                         color=image.COLOR_YELLOW, scale=1.0)
    dis.show(view)


def main():
    """初始化新模型和设备，再按“采集→检测→跟踪→串口→显示”循环运行。"""
    if not (0 <= CROP_Y < IMG_H and 0 < CROP_H <= IMG_H - CROP_Y):
        raise ValueError("Crop must stay inside the camera frame")
    if not (0 <= CENTER_X_PX < IMG_W and PIXELS_PER_CM > 0):
        raise ValueError("Check zero X and pixels per cm")
    model_path = resolve_model_path()  # 启动时确认具体文件，避免工作目录改变导致旧模型被选中
    print("MODEL:", model_path)
    print("Frame: {}x{}, crop: (0, {}, {}, {}), zero X: {}, px/cm: {}".format(
        IMG_W, IMG_H, CROP_Y, IMG_W, CROP_H, CENTER_X_PX, PIXELS_PER_CM))
    print("Camera requested FPS:", CAMERA_FPS, "tile:", TILE_W, "Gamma:", ENABLE_YOLO_LIGHT_NORMALIZATION,
          "YOLO median:", YOLO_DENOISE, "Display median:", DISPLAY_DENOISE)

    # 每帧窗口原点可能改变，同步推理确保框、图像与时间戳一一对应。
    detector = nn.YOLOv5(model=model_path, dual_buff=False)
    cam = camera.Camera(IMG_W, IMG_H, fps=CAMERA_FPS, buff_num=CAMERA_BUFFER_NUM)
    configure_camera(cam)
    serial = uart.UART("/dev/ttyS4", UART_BAUD)  # 与控制器连接的UART4
    dis = display.Display()  # 板载屏幕
    tracker = BallTracker()  # 跟踪生命周期与卡尔曼状态
    normalize_light = ENABLE_YOLO_LIGHT_NORMALIZATION  # Gamma异常后仅本次运行关闭
    denoise_input = YOLO_DENOISE  # 输入滤波开关，不与显示滤波共用
    light_luma_ema = None  # 可选Gamma的亮度EMA初值
    publisher = BallUart()
    direct_frames = sent_direct = sent_packets = 0
    diagnostic_start, diagnostic_frames = time.monotonic(), 0  # 测量主循环实际吞吐率
    loop_fps = 0.0  # 包含采集、处理、推理、显示的循环Hz，不是传感器硬件帧率
    raw_luma, gamma = -1.0, 1.0  # 原始亮度0~255；Gamma=1表示不变

    while not app.need_exit():
        frame = cam.read()  # 始终保留整帧对象到本轮结束，避免裁切视图生命周期问题
        capture_t = time.monotonic()  # 时间为read完成时刻；不等于精确的曝光中点
        crop_img = crop_pipe(frame)  # 未改动的原始水管图，供原始亮度诊断和显示
        diagnostic_frames += 1
        diagnostic_due = capture_t - diagnostic_start >= DIAGNOSTIC_PERIOD_S
        if diagnostic_due:
            raw_luma = float(np.median(crop_to_gray(crop_img)))  # 始终在去噪/Gamma前测量

        origin = tracker.tile_origin(capture_t)
        tile = crop_img.crop(origin, 0, TILE_W, CROP_H)
        yolo_img = tile
        if denoise_input:
            try:
                yolo_img = median_image(tile)
            except Exception as error:
                denoise_input = False
                print("YOLO median disabled:", error)
        gamma = 1.0
        if normalize_light:
            try:
                gray = crop_to_gray(yolo_img)  # 需和送入Gamma的图像对应
                yolo_img, _, light_luma_ema, gamma = prepare_yolo_image(yolo_img, gray, light_luma_ema)
            except Exception as error:
                normalize_light = False
                print("YOLO light normalization disabled:", error)

        objs = detector.detect(yolo_img, conf_th=TRACK_CONF_THRESHOLD, iou_th=IOU_THRESHOLD) or []
        candidates, boxes = candidates_from(objs, tile, origin)
        measured, outlier = tracker.observe(candidates, capture_t)
        direct_frames += int(measured)

        now = time.monotonic()  # 发给控制器时的当前时刻
        x, v, age, status = tracker.output(now, measured, outlier)
        try:
            sent = publisher.poll(serial, tracker, now, outlier)
            if sent is not None:
                sent_packets += 1
                sent_direct += int(bool(sent & STATUS_MEASURED))
        except Exception as error:
            print("UART:", error)

        max_score = max((float(obj.score) for obj in objs), default=0.0)  # 检测阶段最高分
        if diagnostic_due:
            loop_fps = diagnostic_frames / max(now - diagnostic_start, 1e-6)
            print("DIAG F:{:.1f} D:{} C:{} S:{:.2f} X:{:.1f} age:{} status:0x{:02x} "
                  "meas:{}/{} tx_meas:{}/{}".format(loop_fps, len(objs), len(candidates),
                  max_score, x, age, status, direct_frames, diagnostic_frames, sent_direct, sent_packets))
            diagnostic_start, diagnostic_frames = now, 0
            direct_frames = sent_direct = sent_packets = 0
        diagnostic = len(objs), len(candidates), max_score, raw_luma, gamma, loop_fps
        input_mode = ("MED3" if denoise_input else "RAW") + ("+G" if normalize_light else "")
        show_tracking(dis, crop_img, x, v, age, status, boxes, diagnostic, input_mode)


def stop_on_terminate(signum, _frame):
    """收到SIGTERM时正常展开Python调用栈，让补光灯关闭逻辑有机会执行。"""
    raise SystemExit(128 + signum)


def run_with_fill_light():
    """运行时点亮B25，正常返回、Ctrl+C或异常退出时在finally中关闭。"""
    fill_light = None  # 保存GPIO对象，贯穿整个识别程序的运行周期
    previous_handler = None  # 临时替换SIGTERM处理函数，退出时恢复
    try:
        try:
            previous_handler = signal.signal(signal.SIGTERM, stop_on_terminate)
        except (AttributeError, OSError, RuntimeError, ValueError) as error:
            print("SIGTERM handler unavailable:", error)

        err.check_raise(pinmap.set_pin_function("B25", "GPIOB25"), "补光灯引脚设置失败")
        fill_light = gpio.GPIO("GPIOB25", gpio.Mode.OUT)
        fill_light.value(1)  # 高电平开启；相机预热与识别全程处于同一补光条件
        main()
    finally:
        # finally会在正常退出、模型/相机异常、KeyboardInterrupt、SystemExit时执行。
        # SIGKILL、解释器崩溃等不会执行Python清理，纯软件无法保证这些情况下熄灯。
        if fill_light is not None:
            try:
                fill_light.value(0)  # 先关灯，再恢复信号处理
            except Exception as error:
                print("Fill light OFF failed:", error)
        if previous_handler is not None:
            try:
                signal.signal(signal.SIGTERM, previous_handler)
            except Exception as error:
                print("Restore SIGTERM handler failed:", error)


if __name__ == "__main__":
    run_with_fill_light()
