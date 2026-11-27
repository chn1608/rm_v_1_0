"""Draw the chen_bot_v_1_0 wiring diagram (PNG + PDF).

The diagram is generated from the same constants that drive the firmware
(Firmware/Config/board_config.h), so a pin change is a one-line edit here too.
Values that are still unverified are marked 鏆傚畾/寰呮爣瀹?instead of being drawn as
if they were confirmed.
"""
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

OUT = Path(__file__).resolve().parents[1] / "docs"
W, H = 2300, 1560

FONT_CANDIDATES = [
    r"C:\Windows\Fonts\msyh.ttc",
    r"C:\Windows\Fonts\msyhbd.ttc",
    r"C:\Windows\Fonts\simhei.ttf",
]


def font(size, bold=False):
    for path in FONT_CANDIDATES:
        try:
            return ImageFont.truetype(path, size, index=0)
        except OSError:
            continue
    return ImageFont.load_default()


F_TITLE = font(46)
F_SUB = font(26)
F_BOX = font(28)
F_LINE = font(22)
F_SMALL = font(19)

INK = (28, 32, 38)
GRID = (150, 158, 168)
POWER = (176, 42, 42)
SIGNAL = (30, 78, 150)
GROUND = (24, 110, 70)
NOTE = (120, 92, 20)
BOX = (247, 249, 252)
BOX_EDGE = (90, 100, 112)
ACCENT = (232, 238, 246)


def box(draw, x, y, w, h, title, lines, edge=BOX_EDGE, fill=BOX):
    draw.rounded_rectangle([x, y, x + w, y + h], radius=14, fill=fill, outline=edge, width=3)
    draw.text((x + 16, y + 10), title, font=F_BOX, fill=INK)
    ty = y + 52
    for line in lines:
        draw.text((x + 16, ty), line, font=F_LINE, fill=INK)
        ty += 27
    return (x, y, w, h)


def dot(draw, x, y, color):
    draw.ellipse([x - 5, y - 5, x + 5, y + 5], fill=color)


def wire(draw, points, color, label=None, label_at=0.5):
    draw.line(points, fill=color, width=4, joint="curve")
    if label:
        (x1, y1), (x2, y2) = points[0], points[1]
        lx = int(x1 + (x2 - x1) * label_at)
        ly = int(y1 + (y2 - y1) * label_at)
        draw.text((lx + 6, ly - 26), label, font=F_SMALL, fill=color)


def main():
    img = Image.new("RGB", (W, H), (255, 255, 255))
    d = ImageDraw.Draw(img)

    d.text((60, 36), "chen_bot_v_1_0  接线图（DM-MC-Board02 / STM32H723VGT6）", font=F_TITLE, fill=INK)
    d.text((60, 96), "红线=动力/电源　蓝线=信号　绿线=地　黄色边框=暂定或未验证项", font=F_SUB, fill=(90, 96, 104))

    # ------------------------------------------------ power chain (top row)
    box(d, 60, 150, 240, 120, "4S 锂电池", ["3300 mAh 30C", "14.8 V 标称"], edge=POWER)
    box(d, 330, 150, 210, 120, "总开关", ["+ 保险", "规格待定"], edge=POWER)
    box(d, 570, 150, 210, 120, "分电板", ["集中分电", "强弱电分离"], edge=POWER)
    box(d, 860, 140, 250, 140, "LM2596S", ["5 V  继电器/ULN2003", "暂定，未实测"], edge=NOTE)
    box(d, 860, 300, 250, 140, "DFR0831", ["5 V  TB6612 VM", "暂定，未实测"], edge=NOTE)
    box(d, 860, 460, 250, 140, "UBEC-8A", ["7.4 V  舵机", "暂定，未实测"], edge=NOTE)

    wire(d, [(300, 210), (330, 210)], POWER)
    wire(d, [(540, 210), (570, 210)], POWER)
    wire(d, [(780, 210), (820, 210), (820, 210), (860, 210)], POWER)
    wire(d, [(820, 210), (820, 370), (860, 370)], POWER)
    wire(d, [(820, 210), (820, 530), (860, 530)], POWER)

    # ------------------------------------------------ control row
    box(d, 60, 590, 350, 200, "达妙 USB2CAN", ["Type-C → PC（供电+通信）", "CAN 座: H→第4脚  L→第3脚", "UART 座: GND→第2脚 (TX/RX 不用)", "120Ω 默认 ON"], edge=SIGNAL)
    box(d, 60, 800, 300, 130, "ST-Link/V2", ["SWDIO↔PA13  SWCLK↔PA14", "3V3 仅参考  GND"], edge=SIGNAL)
    box(d, 60, 960, 300, 140, "ATK-BLE02", ["PE7←TXD   PE8→RXD", "LINK→PD14  STA→PD15", "5 V / GND  暂定"], edge=NOTE)

    mcu_lines = [
        "STM32H723VGT6  LQFP100",
        "5 V 供电（USB-C 或 VIN）",
        "SWD: PA13 / PA14",
        "USART1: PA9(TX)  PA10(RX)",
        "UART7: PE7(RX)  PE8(TX)",
        "SPI1: PB3(SCK)  PD7(MOSI)",
        "ADC1: PC4 电池  PA5 按键  PA6 触点",
        "PWM: PA0 / PA2 电机  PE9 舵机",
        "GPIO: PE13 PE14 PD8 PD9 PB0 PA3 PA1",
        "CAN1: PD0(RX) PD1(TX)  FDCAN1 AF9",
    ]
    box(d, 880, 600, 520, 380, "DM-MC-Board02 主控", mcu_lines, fill=ACCENT)

    box(d, 1700, 620, 520, 280, "DM_TFT-V1.1  240x280", [
        "1 5V      2 GND      3 CS←PE15",
        "4 SCK←PB3    5 MOSI←PD7",
        "6 BLK←PB10   7 RST←PB11",
        "8 DC←PD10    9 MISO 不用",
        "10 KEY→PA5（五向键梯形电阻）",
    ], edge=SIGNAL)

    wire(d, [(410, 668), (880, 668)], SIGNAL, "CAN1 H/L")
    dot(d, 410, 668, SIGNAL)
    dot(d, 880, 668, SIGNAL)
    # ADAPTER_GND: the CAN connector has no ground pin; the adapter's UART 3P
    # carries GND and must be tied to the board ground (terminal pin 2).
    wire(d, [(410, 742), (880, 742)], GROUND, "GND")
    dot(d, 410, 742, GROUND)
    dot(d, 880, 742, GROUND)
    wire(d, [(360, 865), (880, 865)], SIGNAL, "SWD")
    dot(d, 360, 865, SIGNAL)
    dot(d, 880, 865, SIGNAL)
    wire(d, [(360, 1010), (820, 1010), (820, 940), (880, 940)], SIGNAL, "UART7")
    dot(d, 360, 1010, SIGNAL)
    dot(d, 880, 940, SIGNAL)
    wire(d, [(1400, 720), (1700, 720)], SIGNAL, "SPI1/KEY")
    dot(d, 1400, 720, SIGNAL)
    dot(d, 1700, 720, SIGNAL)

    # ------------------------------------------------ actuation row
    box(d, 60, 1210, 280, 110, "左电机 1:48", ["AO1 / AO2", "编码器：未安装"], edge=POWER)
    box(d, 60, 1340, 280, 110, "右电机 1:48", ["BO1 / BO2", "编码器：未安装"], edge=POWER)

    box(d, 520, 1180, 450, 260, "TB6612FNG", [
        "PWMA←PA0    PWMB←PA2",
        "AIN1←PE13   AIN2←PE14",
        "BIN1←PD8    BIN2←PD9",
        "STBY←PA1    VCC=3V3",
        "VM=5V(暂定)",
    ], edge=POWER)

    box(d, 1030, 1180, 380, 200, "LD-220MG 舵机", [
        "PWM←PE9 (TIM1_CH1)",
        "电源：UBEC 7.4 V（暂定）",
        "中位 1500 us，限位待标定",
    ], edge=POWER)

    box(d, 1470, 1160, 750, 290, "动力控制 K1 / K2", [
        "K1 线圈 ←PB0 → ULN2003A → 5 V",
        "K2 线圈 ←PA3 → ULN2003A → 5 V",
        "触点串在动力正极（K1 = 总动力）",
        "触点反馈未接线（FEATURE_RELAY_FEEDBACK=0）",
    ], edge=POWER)

    wire(d, [(880, 980), (880, 1120), (740, 1120), (740, 1180)], SIGNAL, "PWM / 方向")
    dot(d, 880, 980, SIGNAL)
    dot(d, 740, 1180, SIGNAL)
    wire(d, [(520, 1290), (340, 1290)], POWER, "AO1/AO2")
    wire(d, [(520, 1395), (340, 1395)], POWER, "BO1/BO2")
    wire(d, [(1200, 980), (1200, 1180)], SIGNAL, "PWM")
    dot(d, 1200, 980, SIGNAL)
    wire(d, [(1400, 940), (1440, 940), (1440, 1240), (1470, 1240)], SIGNAL, "PB0/PA3")
    dot(d, 1400, 940, SIGNAL)

    # ------------------------------------------------ notes
    d.rounded_rectangle([60, 1480, 2220, 1536], radius=8, fill=(255, 250, 235), outline=NOTE, width=2)
    d.text((78, 1490),
           "注意：三路降压模块的输入电压与电流能力尚未实测，上电前先断开电机与舵机单独核对 5 V / 7.4 V 轨；"
           "全部地必须共地，动力地与信号地单点汇合；CAN 总线两端 120Ω 终端，支线不超过 0.3 m。",
           font=F_SMALL, fill=NOTE)
    d.text((78, 1514),
           "CAN：适配器 H→端子第4脚(CAN1_H) L→第3脚(CAN1_L) GND→第2脚(INGND)；CANH/CANL 由 PD1/PD0 经板载 U1 驱动，主板需 5V 供电。",
           font=F_SMALL, fill=NOTE)

    OUT.mkdir(parents=True, exist_ok=True)
    png = OUT / "wiring_chen_bot_v1_0.png"
    pdf = OUT / "wiring_chen_bot_v1_0.pdf"
    def save(image, path, fmt=None):
        """Save, but survive a viewer holding the previous file open."""
        try:
            if fmt:
                image.save(path, fmt, resolution=150.0)
            else:
                image.save(path)
            return path
        except PermissionError:
            alt = path.with_name(path.stem + "_new" + path.suffix)
            if fmt:
                image.save(alt, fmt, resolution=150.0)
            else:
                image.save(alt)
            print("locked, wrote", alt)
            return alt

    print(save(img, png))
    print(save(img, pdf, "PDF"))


if __name__ == "__main__":
    main()