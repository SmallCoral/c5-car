from maix import camera, display, image, nn, app, uart
from struct import pack
import cv2
import numpy as np
import os
import time


# ==========================
# 现场只需要调整这一段
# ==========================
IMG_W = 320
IMG_H = 240
# 60 FPS 对常见 MaixCAM 摄像头更稳定；过高的请求帧率可能产生噪点、
# 条纹或实际帧率不达标。若确认使用高速全局快门模组，再单独提高。
CAMERA_FPS = 60

# 屏幕初始化后锁定的几何中线，作为视觉零点默认值和旧协议中线。
SCREEN_CENTER_X_PX = IMG_W // 2

# 高速运动时短曝光比高相机帧率更重要。1500 us 时，80 cm/s 的球
# 在图像上理论拖影约 1.2 px。降低增益可明显减少颗粒噪点；若画面
# 太暗，应先增强直流 LED 照明，再小步增加增益。
CAMERA_BUFFER_NUM = 1
MANUAL_EXPOSURE_US = 1500
MANUAL_GAIN = 50

# 保持短曝光不变，只对送入 YOLO 的副本做亮度归一化，避免暗处自动曝光
# 拉长后重新产生拖影。LIGHT_TARGET_LUMA 应取模型训练图像的典型灰度。
ENABLE_YOLO_LIGHT_NORMALIZATION = True
LIGHT_TARGET_LUMA = 115.0
LIGHT_DEADBAND = 8.0
LIGHT_EMA_ALPHA = 0.1
LIGHT_GAMMA_MIN = 0.80
LIGHT_GAMMA_MAX = 1.75

# 视觉测量零点。当前按题设与屏幕中线重合；如安装存在固定偏差，
# 可单独修改该值。V2 绝对 X 会用该零点和相对位置重新恢复。
CENTER_X_PX = float(SCREEN_CENTER_X_PX)
PIXELS_PER_CM = 10.0

# 水管只出现在画面中间区域，减小 YOLO 推理量和误检率。
CROP_Y = IMG_H // 3 + 35
CROP_H = IMG_H // 5 

# 跟踪阶段允许低置信度模糊框；首次捕获和全管重捕获门限也整体降低。
TRACK_CONF_THRESHOLD = 0.25
ACQUIRE_CONF_THRESHOLD = 0.45
REACQUIRE_CONF_THRESHOLD = 0.25
REACQUIRE_GLOBAL_CONF_THRESHOLD = 0.40
IOU_THRESHOLD = 0.45
MIN_BOX_SIZE_PX = 4
MAX_BOX_SIZE_PX = 55

# OpenCV 只作为 YOLO 丢失时的备用测量。启动时必须保持空管，程序会
# 先等待再采集空管背景；背景固定不更新，避免静止小球被吸收到背景。
ENABLE_CV_FALLBACK = True
CV_BACKGROUND_WAIT_S = 2.0
CV_BACKGROUND_FRAMES = 30
CV_BALL_CONFIRM_FRAMES = 3
CV_DIFF_THRESHOLD = 20
CV_MIN_PEAK = 800.0
CV_PEAK_RATIO = 2.5
CV_CONTRAST_SCALE_MIN = 0.75
CV_CONTRAST_SCALE_MAX = 1.33
CV_CENTROID_HALF_WIDTH = 8
CV_PIPE_BAND_HALF_HEIGHT = 25
CV_SEARCH_GATE_MIN_PX = 30.0
CV_SEARCH_GATE_MAX_PX = 110.0

# alpha-beta 滤波器。先调 BETA（阻尼/速度），再调 ALPHA（跟随速度）。
FILTER_ALPHA = 0.40
FILTER_BETA = 0.06
ASSOCIATION_GATE_MIN_PX = 20.0
ASSOCIATION_GATE_MAX_PX = 110.0
ASSOCIATION_LOOKAHEAD_S = 0.020
PREDICT_VALID_TIME_S = 0.15
REACQUIRE_TIME_S = 0.18
RECOVERY_WINDOW_TIME_S = 1.20
RECOVERY_GATE_MIN_PX = 50.0
RECOVERY_GATE_MAX_PX = 260.0
RECOVERY_GATE_GROWTH_PX_S = 350.0
RECOVERY_PREDICT_TIME_S = 0.20
MAX_ABS_SPEED_CM_S = 80.0

# dual_buff=True 会提高推理帧率，但返回上一帧的检测结果。
# 使用上一次循环间隔把检测坐标外推到当前时刻。
DETECTOR_DUAL_BUFFER = True
MAX_DETECTION_DELAY_S = 0.040

# 控制器使用固定频率的数据更稳定，50 Hz 足够完成本题。
UART_BAUD = 115200
TX_RATE_HZ = 50.0

# 1: 原 7 字节协议，仅用于兼容旧控制器；2: 推荐协议，含速度和有效标志。
UART_PROTOCOL = 2


# ==========================
# UART4 与协议
# ==========================
serial = uart.UART("/dev/ttyS4", UART_BAUD)

STATUS_VALID = 0x01
STATUS_MEASURED = 0x02
STATUS_PREDICTED = 0x04
STATUS_OUTLIER = 0x08
STATUS_CV_FALLBACK = 0x10


def clamp(value, low, high):
    return max(low, min(high, value))


def crc8(data):
    """CRC-8/ATM, polynomial=0x07, init=0x00."""
    crc = 0
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x80:
                crc = ((crc << 1) ^ 0x07) & 0xFF
            else:
                crc = (crc << 1) & 0xFF
    return crc


def send_legacy(valid, position_px):
    """
    兼容原协议：第一个 int16 始终为启动时锁定的屏幕中线，
    第二个 int16 为球相对中线的偏移。32767 表示偏移无效。
    """
    center_x = int(SCREEN_CENTER_X_PX)
    if valid:
        offset_px = int(round(position_px))
    else:
        offset_px = 32767

    frame = b'\xaa\x55'
    frame += pack("<h", center_x)
    frame += pack("<h", offset_px)
    frame += b'\x17'
    serial.write(frame)


def send_v2(sequence, status, position_px, speed_cm_s, age_ms):
    """
    V2 frame, little-endian, total 16 bytes:
      AA 55 | version:u8 | status:u8 | sequence:u16 |
      ball_x_px:i16 | position:i16 | speed:i16 | age_ms:u16 | crc8:u8 | 17

    ball_x_px: 小球滤波/预测后的实时绝对 X 像素坐标。
    position: 相对水管中心位置，单位 0.01 cm。
    speed:    沿 X 轴速度，单位 0.1 cm/s。
    crc8:     对 version 到 age_ms 的 12 字节计算 CRC-8/ATM。
    """
    ball_x_px = int(clamp(round(CENTER_X_PX + position_px),
                          0, IMG_W - 1))
    position_001_cm = int(clamp(
        round(position_px / PIXELS_PER_CM * 100.0), -32768, 32767))
    speed_01_cm_s = int(clamp(round(speed_cm_s * 10.0), -32768, 32767))
    age_ms = int(clamp(age_ms, 0, 65535))

    payload = pack("<BBHhhhH", 2, status, sequence, ball_x_px,
                   position_001_cm, speed_01_cm_s, age_ms)
    frame = b'\xaa\x55' + payload + pack("<B", crc8(payload)) + b'\x17'
    serial.write(frame)


# ==========================
# OpenCV 空管背景差分
# ==========================
CV_KERNEL = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (3, 3))


def crop_to_gray(crop_img):
    """零拷贝取得 RGB 数据，再转换成独立的灰度图。"""
    rgb = image.image2cv(crop_img, ensure_bgr=False, copy=False)
    return cv2.cvtColor(rgb, cv2.COLOR_RGB2GRAY)


def prepare_yolo_image(crop_img, previous_luma):
    """对 YOLO 输入副本做限幅 Gamma 归一化，返回原始中值亮度和 Gamma。"""
    gray = crop_to_gray(crop_img)
    raw_luma = float(np.median(gray))
    if previous_luma is None:
        smoothed_luma = raw_luma
    else:
        smoothed_luma = ((1.0 - LIGHT_EMA_ALPHA) * previous_luma +
                         LIGHT_EMA_ALPHA * raw_luma)

    if abs(smoothed_luma - LIGHT_TARGET_LUMA) <= LIGHT_DEADBAND:
        return crop_img, raw_luma, smoothed_luma, 1.0

    source_level = clamp(smoothed_luma, 5.0, 250.0) / 255.0
    target_level = clamp(LIGHT_TARGET_LUMA, 5.0, 250.0) / 255.0
    gamma = np.log(target_level) / np.log(source_level)
    gamma = float(clamp(gamma, LIGHT_GAMMA_MIN, LIGHT_GAMMA_MAX))

    rgb = image.image2cv(crop_img, ensure_bgr=False, copy=True)
    levels = np.arange(256, dtype=np.float32) / 255.0
    lut = np.clip(np.power(levels, gamma) * 255.0 + 0.5,
                  0, 255).astype(np.uint8)
    normalized_rgb = cv2.LUT(rgb, lut)
    normalized_img = image.cv2image(
        normalized_rgb, bgr=False, copy=True)
    return normalized_img, raw_luma, smoothed_luma, gamma


def capture_empty_pipe_background(cam, dis):
    """启动时采集空管平均背景。采集阶段管内不能放置钢球。"""
    wait_end = time.time() + CV_BACKGROUND_WAIT_S
    while time.time() < wait_end and not app.need_exit():
        frame = cam.read()
        crop_img = frame.crop(0, CROP_Y, IMG_W, CROP_H)
        view = crop_img.copy()
        remaining = max(0.0, wait_end - time.time())
        view.draw_string(5, 5, "BG WAIT {:.1f}s".format(remaining),
                         color=image.COLOR_YELLOW, scale=1.0)
        dis.show(view)

    accumulator = np.zeros((CROP_H, IMG_W), dtype=np.float32)
    captured = 0
    for index in range(CV_BACKGROUND_FRAMES):
        if app.need_exit():
            break
        frame = cam.read()
        crop_img = frame.crop(0, CROP_Y, IMG_W, CROP_H)
        gray = crop_to_gray(crop_img)
        cv2.accumulate(gray, accumulator)
        captured += 1

        view = crop_img.copy()
        view.draw_string(5, 5, "BG CAL {}/{}".format(
                             index + 1, CV_BACKGROUND_FRAMES),
                         color=image.COLOR_YELLOW, scale=1.0)
        dis.show(view)

    if captured == 0:
        raise RuntimeError("OpenCV background capture aborted")

    background = np.clip(accumulator / float(captured), 0, 255).astype(np.uint8)
    background = cv2.GaussianBlur(background, (3, 3), 0)
    return background, float(background.mean())


def detect_ball_with_cv(crop_img, background, background_mean,
                        predicted_px, association_gate_px):
    """
    在预测窗口内用空管背景差分和一维列能量寻找钢球。
    返回 (相对中线位置px, 质量比, 峰值)；无可靠候选时位置为 None。
    """
    gray = crop_to_gray(crop_img)
    gray = cv2.GaussianBlur(gray, (3, 3), 0)

    # 同时对齐当前图和背景的平均亮度、对比度。球只占少量像素，对整幅
    # 统计量影响很小；缩放限幅可防止暗处噪声被过度放大。
    current_mean = float(gray.mean())
    current_std = max(float(gray.std()), 1.0)
    background_std = max(float(background.std()), 1.0)
    contrast_scale = clamp(background_std / current_std,
                           CV_CONTRAST_SCALE_MIN,
                           CV_CONTRAST_SCALE_MAX)
    adjusted = np.clip(
        (gray.astype(np.float32) - current_mean) * contrast_scale +
        background_mean, 0, 255).astype(np.uint8)

    diff = cv2.absdiff(adjusted, background)
    diff = cv2.medianBlur(diff, 3)
    _, mask = cv2.threshold(diff, CV_DIFF_THRESHOLD, 255, cv2.THRESH_BINARY)
    mask = cv2.morphologyEx(mask, cv2.MORPH_OPEN, CV_KERNEL)
    mask = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, CV_KERNEL)

    band_center = CROP_H // 2
    band_top = max(0, band_center - CV_PIPE_BAND_HALF_HEIGHT)
    band_bottom = min(CROP_H, band_center + CV_PIPE_BAND_HALF_HEIGHT + 1)
    if band_top > 0:
        mask[:band_top, :] = 0
    if band_bottom < CROP_H:
        mask[band_bottom:, :] = 0

    column_score = mask.sum(axis=0).astype(np.float32)
    column_score = cv2.GaussianBlur(column_score.reshape(1, -1),
                                    (9, 1), 0).reshape(-1)

    if predicted_px is None:
        search_left = 0
        search_right = IMG_W
    else:
        gate = clamp(max(CV_SEARCH_GATE_MIN_PX, association_gate_px),
                     CV_SEARCH_GATE_MIN_PX, CV_SEARCH_GATE_MAX_PX)
        predicted_abs = CENTER_X_PX + predicted_px
        search_left = int(max(0, predicted_abs - gate))
        search_right = int(min(IMG_W, predicted_abs + gate + 1))

    if search_right - search_left < 3:
        return None, 0.0, 0.0

    local_score = column_score[search_left:search_right]
    local_peak_index = int(np.argmax(local_score))
    peak = float(local_score[local_peak_index])
    noise = float(np.median(local_score))
    quality = (peak + 1.0) / (noise + 1.0)

    if peak < CV_MIN_PEAK or quality < CV_PEAK_RATIO:
        return None, quality, peak

    peak_x = search_left + local_peak_index
    centroid_left = max(search_left, peak_x - CV_CENTROID_HALF_WIDTH)
    centroid_right = min(search_right, peak_x + CV_CENTROID_HALF_WIDTH + 1)
    weights = column_score[centroid_left:centroid_right]
    weight_sum = float(weights.sum())
    if weight_sum <= 0.0:
        return None, quality, peak

    coordinates = np.arange(centroid_left, centroid_right, dtype=np.float32)
    absolute_x = float((coordinates * weights).sum() / weight_sum)
    return absolute_x - CENTER_X_PX, quality, peak


def wait_for_ball_after_background(cam, dis, background, background_mean):
    """背景采集完成后等待放球，连续确认后才进入闭环数据发送。"""
    confirmed = 0
    while not app.need_exit():
        frame = cam.read()
        crop_img = frame.crop(0, CROP_Y, IMG_W, CROP_H)
        position_px, quality, _ = detect_ball_with_cv(
            crop_img, background, background_mean, None,
            CV_SEARCH_GATE_MAX_PX)

        view = crop_img.copy()
        if position_px is None:
            confirmed = 0
            view.draw_string(5, 5, "PLACE BALL",
                             color=image.COLOR_YELLOW, scale=1.0)
        else:
            confirmed += 1
            ball_x = int(clamp(round(CENTER_X_PX + position_px),
                               0, IMG_W - 1))
            view.draw_line(ball_x, 0, ball_x, CROP_H,
                           color=image.COLOR_GREEN, thickness=2)
            view.draw_string(5, 5, "BALL {}/{} Q:{:.1f}".format(
                                 confirmed, CV_BALL_CONFIRM_FRAMES, quality),
                             color=image.COLOR_GREEN, scale=1.0)
        dis.show(view)

        if confirmed >= CV_BALL_CONFIRM_FRAMES:
            return


# ==========================
# alpha-beta 跟踪器
# ==========================
class BallTracker:
    def __init__(self):
        self.initialized = False
        self.ever_measured = False
        self.x_px = 0.0
        self.v_px_s = 0.0
        self.last_time = 0.0
        self.last_measure_time = 0.0
        self.recovery_active = False
        self.recovery_anchor_time = 0.0
        self.recovery_x_px = 0.0
        self.recovery_v_px_s = 0.0

    def reference(self, now):
        if not self.initialized:
            return None
        if now - self.last_measure_time > REACQUIRE_TIME_S:
            return None
        dt = clamp(now - self.last_time, 0.0, 0.10)
        return self.x_px + self.v_px_s * dt

    def association_gate(self, now):
        if not self.initialized:
            return ASSOCIATION_GATE_MAX_PX
        missing_time = clamp(now - self.last_measure_time, 0.0, 0.12)
        travel_px = abs(self.v_px_s) * (
            missing_time + ASSOCIATION_LOOKAHEAD_S)
        return clamp(ASSOCIATION_GATE_MIN_PX + travel_px,
                     ASSOCIATION_GATE_MIN_PX,
                     ASSOCIATION_GATE_MAX_PX)

    def recovery_window(self, now):
        """返回丢失后的预测中心和逐步扩大的重捕获门限。"""
        if self.initialized:
            age_s = now - self.last_measure_time
            if age_s <= REACQUIRE_TIME_S:
                return None, 0.0
            elapsed = age_s - REACQUIRE_TIME_S
            if elapsed > RECOVERY_WINDOW_TIME_S:
                return None, 0.0
            dt = clamp(now - self.last_time, 0.0,
                       RECOVERY_PREDICT_TIME_S)
            reference_px = self.x_px + self.v_px_s * dt
        elif self.recovery_active:
            elapsed = now - self.recovery_anchor_time
            if elapsed > RECOVERY_WINDOW_TIME_S:
                return None, 0.0
            dt = clamp(elapsed, 0.0, RECOVERY_PREDICT_TIME_S)
            reference_px = (self.recovery_x_px +
                            self.recovery_v_px_s * dt)
        else:
            return None, 0.0

        reference_px = clamp(reference_px, -CENTER_X_PX,
                             IMG_W - 1 - CENTER_X_PX)
        gate_px = clamp(
            RECOVERY_GATE_MIN_PX + RECOVERY_GATE_GROWTH_PX_S * elapsed,
            RECOVERY_GATE_MIN_PX, RECOVERY_GATE_MAX_PX)
        return reference_px, gate_px

    def update(self, measurement_px, now):
        measured = False

        if not self.initialized:
            if measurement_px is None:
                return False, False, 65535
            self.initialized = True
            self.ever_measured = True
            self.recovery_active = False
            self.x_px = measurement_px
            self.v_px_s = 0.0
            self.last_time = now
            self.last_measure_time = now
            return True, True, 0

        dt = clamp(now - self.last_time, 1.0 / 240.0, 0.10)
        self.x_px += self.v_px_s * dt
        self.last_time = now

        if measurement_px is not None:
            residual = measurement_px - self.x_px
            self.x_px += FILTER_ALPHA * residual
            self.v_px_s += FILTER_BETA * residual / dt
            max_speed_px_s = MAX_ABS_SPEED_CM_S * PIXELS_PER_CM
            self.v_px_s = clamp(self.v_px_s, -max_speed_px_s, max_speed_px_s)
            self.last_measure_time = now
            self.ever_measured = True
            self.recovery_active = False
            measured = True

        age_s = now - self.last_measure_time
        valid = age_s <= PREDICT_VALID_TIME_S
        age_ms = int(clamp(round(age_s * 1000.0), 0, 65535))

        # 长时间丢球后保存短时运动趋势，并进入逐步扩大的恢复搜索。
        if age_s > REACQUIRE_TIME_S:
            if not self.recovery_active:
                self.recovery_active = True
                self.recovery_anchor_time = now
                self.recovery_x_px = self.x_px
                self.recovery_v_px_s = self.v_px_s
            self.initialized = False
            self.v_px_s = 0.0

        return valid, measured, age_ms


# ==========================
# YOLO、摄像头与显示
# ==========================
model_path = "model_314653.mud"
if not os.path.exists(model_path):
    model_path = "/root/models/wuhaha/feball/model_314653.mud"

detector = nn.YOLOv5(model=model_path, dual_buff=DETECTOR_DUAL_BUFFER)
cam = camera.Camera(IMG_W, IMG_H, fps=CAMERA_FPS,
                    buff_num=CAMERA_BUFFER_NUM)

try:
    cam.exposure(MANUAL_EXPOSURE_US)
    cam.gain(MANUAL_GAIN)
    cam.skip_frames(30)
    print("Camera exposure:", cam.exposure(), "us, gain:", cam.gain())
except Exception as error:
    # 某些摄像头模组可能不支持手动参数，仍允许程序运行并明确告警。
    print("Camera manual exposure unavailable:", error)

dis = display.Display()
tracker = BallTracker()

cv_background = None
cv_background_mean = 0.0
if ENABLE_CV_FALLBACK:
    try:
        cv_background, cv_background_mean = capture_empty_pipe_background(
            cam, dis)
        print("OpenCV background ready:", cv_background.shape,
              "mean:", cv_background_mean)
        wait_for_ball_after_background(
            cam, dis, cv_background, cv_background_mean)
    except Exception as error:
        cv_background = None
        print("OpenCV fallback disabled:", error)

last_loop_time = time.time()
last_tx_time = 0.0
fps = 0.0
sequence = 0
light_luma_ema = None
light_raw_luma = -1.0
light_gamma = 1.0
light_normalization_available = ENABLE_YOLO_LIGHT_NORMALIZATION


while not app.need_exit():
    loop_start = time.time()
    loop_dt = loop_start - last_loop_time
    last_loop_time = loop_start
    if loop_dt > 0:
        instant_fps = 1.0 / loop_dt
        fps = instant_fps if fps == 0 else 0.90 * fps + 0.10 * instant_fps

    img = cam.read()
    crop_img = img.crop(0, CROP_Y, IMG_W, CROP_H)
    view = crop_img.copy()
    crop_cy = CROP_H // 2

    view.draw_line(int(CENTER_X_PX), 0, int(CENTER_X_PX), CROP_H,
                   color=image.COLOR_BLUE, thickness=2)

    yolo_img = crop_img
    if light_normalization_available:
        try:
            yolo_img, light_raw_luma, light_luma_ema, light_gamma = (
                prepare_yolo_image(crop_img, light_luma_ema))
        except Exception as error:
            light_normalization_available = False
            light_raw_luma = -1.0
            light_gamma = 1.0
            print("YOLO light normalization disabled:", error)

    objs = detector.detect(yolo_img,
                           conf_th=TRACK_CONF_THRESHOLD,
                           iou_th=IOU_THRESHOLD) or []

    candidates = []
    for obj in objs:
        if (MIN_BOX_SIZE_PX <= obj.w <= MAX_BOX_SIZE_PX and
                MIN_BOX_SIZE_PX <= obj.h <= MAX_BOX_SIZE_PX):
            center_x = obj.x + obj.w * 0.5
            measurement_px = center_x - CENTER_X_PX
            candidates.append((obj, measurement_px))

    chosen = None
    reacquired_by_yolo = False
    rejected_as_outlier = False
    reference_px = tracker.reference(loop_start)
    association_gate_px = tracker.association_gate(loop_start)
    recovery_reference_px, recovery_gate_px = tracker.recovery_window(
        loop_start)

    if candidates:
        if reference_px is None:
            if recovery_reference_px is not None:
                recovery_candidates = [
                    item for item in candidates
                    if (item[0].score >= REACQUIRE_CONF_THRESHOLD and
                        abs(item[1] - recovery_reference_px) <=
                        recovery_gate_px)
                ]
                if recovery_candidates:
                    chosen = max(
                        recovery_candidates,
                        key=lambda item: (
                            item[0].score - 0.002 * abs(
                                item[1] - recovery_reference_px),
                            item[0].score))
                    reacquired_by_yolo = True

            acquire_threshold = ACQUIRE_CONF_THRESHOLD
            if tracker.ever_measured:
                acquire_threshold = REACQUIRE_GLOBAL_CONF_THRESHOLD
            acquisition_candidates = [
                item for item in candidates
                if item[0].score >= acquire_threshold
            ]
            if chosen is None and acquisition_candidates:
                chosen = max(acquisition_candidates,
                             key=lambda item: (item[0].score,
                                               item[0].w * item[0].h))
                reacquired_by_yolo = tracker.ever_measured
        else:
            nearest = min(candidates,
                          key=lambda item: abs(item[1] - reference_px))
            if abs(nearest[1] - reference_px) <= association_gate_px:
                chosen = nearest
            else:
                rejected_as_outlier = True

    measurement_px = None
    measurement_source = "NONE"
    cv_measurement_px = None
    cv_quality = 0.0
    cv_peak = 0.0

    if chosen is not None:
        measurement_px = chosen[1]
        measurement_source = (
            "RE-YOLO" if reacquired_by_yolo else "YOLO")
        if DETECTOR_DUAL_BUFFER and tracker.initialized:
            detection_delay_s = clamp(loop_dt, 0.0, MAX_DETECTION_DELAY_S)
            measurement_px += tracker.v_px_s * detection_delay_s
    elif cv_background is not None:
        try:
            cv_reference_px = reference_px
            cv_gate_px = association_gate_px
            if (cv_reference_px is None and
                    recovery_reference_px is not None and
                    recovery_gate_px < CV_SEARCH_GATE_MAX_PX):
                cv_reference_px = recovery_reference_px
                cv_gate_px = recovery_gate_px
            cv_measurement_px, cv_quality, cv_peak = detect_ball_with_cv(
                crop_img, cv_background, cv_background_mean,
                cv_reference_px, cv_gate_px)
            if cv_measurement_px is not None:
                measurement_px = cv_measurement_px
                measurement_source = "CV"
        except Exception as error:
            cv_background = None
            print("OpenCV runtime fallback disabled:", error)

    valid, measured, age_ms = tracker.update(measurement_px, loop_start)

    status = 0
    if valid:
        status |= STATUS_VALID
    if measured:
        status |= STATUS_MEASURED
    elif valid:
        status |= STATUS_PREDICTED
    if rejected_as_outlier:
        status |= STATUS_OUTLIER
    if measured and measurement_source == "CV":
        status |= STATUS_CV_FALLBACK

    speed_cm_s = tracker.v_px_s / PIXELS_PER_CM

    if loop_start - last_tx_time >= 1.0 / TX_RATE_HZ:
        try:
            if UART_PROTOCOL == 1:
                send_legacy(valid, tracker.x_px)
            else:
                send_v2(sequence, status, tracker.x_px, speed_cm_s, age_ms)
            sequence = (sequence + 1) & 0xFFFF
        except Exception as error:
            print("UART:", error)
        last_tx_time = loop_start

    if chosen is not None:
        obj = chosen[0]
        view.draw_rect(obj.x, obj.y, obj.w, obj.h, color=image.COLOR_RED)
        view.draw_circle(int(obj.x + obj.w * 0.5),
                         int(obj.y + obj.h * 0.5), 4,
                         color=image.COLOR_GREEN)
    elif measured and measurement_source == "CV":
        cv_x = int(clamp(round(CENTER_X_PX + cv_measurement_px),
                         0, IMG_W - 1))
        view.draw_line(cv_x, 0, cv_x, CROP_H,
                       color=image.COLOR_YELLOW, thickness=1)
        view.draw_circle(cv_x, crop_cy, 7, color=image.COLOR_YELLOW)

    if tracker.initialized or valid:
        filtered_x = int(round(CENTER_X_PX + tracker.x_px))
        filtered_x = int(clamp(filtered_x, 0, IMG_W - 1))
        view.draw_line(filtered_x, 0, filtered_x, CROP_H,
                       color=image.COLOR_GREEN, thickness=2)
        view.draw_circle(filtered_x, crop_cy, 5, color=image.COLOR_GREEN)

    if measured:
        if measurement_source == "CV":
            state_text = "CV"
            state_color = image.COLOR_YELLOW
        else:
            state_text = measurement_source
            state_color = image.COLOR_GREEN
    elif valid:
        state_text = "PREDICT"
        state_color = image.COLOR_YELLOW
    else:
        state_text = "INVALID"
        state_color = image.COLOR_RED

    position_cm = tracker.x_px / PIXELS_PER_CM
    view.draw_string(5, 5,
                     "{} x:{:+.2f}cm v:{:+.1f}cm/s".format(
                         state_text, position_cm, speed_cm_s),
                     color=state_color, scale=1.0)
    view.draw_string(5, 25,
                     "F:{:.0f} L:{:.0f} g:{:.2f} A:{} G:{:.0f}".format(
                         fps, light_raw_luma, light_gamma,
                         age_ms,
                         recovery_gate_px if recovery_reference_px is not None
                         else association_gate_px),
                     color=image.COLOR_YELLOW, scale=1.0)
    if measurement_source == "CV":
        view.draw_string(5, 45,
                         "CV Q:{:.1f} peak:{:.0f}".format(
                             cv_quality, cv_peak),
                         color=image.COLOR_YELLOW, scale=1.0)

    dis.show(view)
