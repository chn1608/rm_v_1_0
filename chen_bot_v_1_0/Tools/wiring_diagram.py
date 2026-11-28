"""chen_bot_v_1_0 wiring diagram (finalized hardware).

Colour code:
  green  = connection confirmed / already working
  yellow = pending or questionable
  red    = serious problem, must be fixed before power-on

Every signal is named by its MCU pin (PAx/PBx/PCx/PDx/PEx); no connector
position notation such as "P7.4".
"""
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

OUT = Path(__file__).resolve().parents[1] / "docs"
W, H = 2600, 1960

GREEN = (24, 120, 60)
YELLOW = (198, 140, 20)
RED = (200, 32, 32)
POWER = (176, 42, 42)
SIGNAL = (30, 78, 150)
GND = (20, 110, 70)
TEXT = (25, 25, 25)


def font(size, bold=False):
    return ImageFont.truetype(rf"C:\Windows\Fonts\{'msyhbd.ttc' if bold else 'msyh.ttc'}", size)


F_T = font(34, True)
F_B = font(19, True)
F_L = font(16)
F_S = font(15)
F_TB = font(16)


def box(d, x, y, w, h, title, lines, edge):
    d.rounded_rectangle([x, y, x + w, y + h], radius=10, outline=edge, width=3, fill="white")
    d.text((x + 12, y + 8), title, font=F_B, fill=TEXT)
    for i, line in enumerate(lines):
        d.text((x + 12, y + 34 + i * 21), line, font=F_L, fill=TEXT)


def line(d, pts, color, width=3):
    d.line(pts, fill=color, width=width, joint="curve")


def arrow(d, x0, y0, x1, y1, color):
    d.line([(x0, y0), (x1, y1)], fill=color, width=3)
    d.ellipse([x1 - 5, y1 - 5, x1 + 5, y1 + 5], fill=color)


def label(d, x, y, text, color):
    d.text((x, y), text, font=F_S, fill=color)


def main():
    img = Image.new("RGB", (W, H), "white")
    d = ImageDraw.Draw(img)

    d.text((40, 20), "chen_bot_v_1_0 接线图（定稿硬件 · DM-MC-Board02 / STM32H723VGT6）",
           font=F_T, fill=TEXT)
    d.text((44, 64), "红=电源　蓝=信号　绿=地　|　引脚一律用 MCU 引脚名", font=F_S, fill=(90, 90, 90))
    lx = 44
    for text, color in (("绿框：已确认可用", GREEN), ("黄框：待定/有问题", YELLOW),
                        ("红框：严重问题", RED)):
        d.rounded_rectangle([lx, 96, lx + 26, 122], radius=5, outline=color, width=3)
        d.text((lx + 34, 99), text, font=F_S, fill=color)
        lx += 240

    # ----------------------------------------------------------- 电源链
    box(d, 40, 150, 390, 96, "4S 锂电池 格氏 ACE 3300mAh", ["14.8V，满电 16.8V，XT60 输出"], GREEN)
    box(d, 40, 286, 390, 78, "VIFLY Shortsaver2 防短路", ["XT60 输入 / 输出"], GREEN)
    box(d, 40, 404, 390, 96, "分电 XT60 → 2×XT30（待定）", ["清单无分电板，需自制线束", "18AWG 红正黑负"], YELLOW)
    box(d, 40, 540, 390, 104, "UBEC-8A 独立供电（待定）",
        ["输出 5V / 6V / 7.4V 跳线", "本项目用 5V 档", "6V/7.4V 严禁接主板"], YELLOW)
    box(d, 40, 684, 390, 104, "12V 降压（严重问题）",
        ["TB6612 VM 绝对最大 15V", "4S 满电 16.8V 超压", "必须补一个 12V 降压"], RED)

    arrow(d, 235, 246, 235, 286, POWER)
    arrow(d, 235, 364, 235, 404, POWER)
    arrow(d, 235, 500, 235, 540, POWER)
    arrow(d, 235, 644, 235, 684, POWER)

    # ----------------------------------------------------------- 主控
    box(d, 780, 180, 620, 480, "主控 DM-MC-Board02（已确认）",
        ["供电：USB-C 5V（当前实测方式）",
         "SWD：PA13 / PA14",
         "CAN1：PD0(RX) / PD1(TX)",
         "ADC：PA5 五向键 / PC4 电池 / PA6 急停",
         "PWM：PA0(TIM2_CH1) / PA2(TIM2_CH3)",
         "方向：PE13 PE14 PD8 PD9　STBY→PA1",
         "舵机：PE9（TIM1_CH1）",
         "SPI1：PB3 PD7 PE15 PD10 PB10 PB11",
         "UART7：PE7 PE8 / PD14 PD15",
         "预留编码器：PB4 PB5 / PB6 PB7"], GREEN)

    # 5V：UBEC → 主控
    line(d, [(430, 566), (600, 566), (600, 240), (780, 240)], POWER)
    label(d, 452, 214, "5V → 主板", POWER)

    # ----------------------------------------------------------- 右侧
    right = [
        ("DM_TFT-V1.1 240×280（已确认）",
         ["CS←PE15　DC←PD10　SCK←PB3　MOSI←PD7",
          "BLK←PB10　RST←PB11　KEY→PA5", "实物已验证：显示与五向键可用"], 150, 110, GREEN),
        ("达妙 USB2CAN（待定）",
         ["CAN_H — PD1　CAN_L — PD0", "UART 座 GND → 主板 GND",
          "需官方上位机设 CAN2.0 + 1 Mbps"], 300, 110, YELLOW),
        ("ST-Link V2（已确认）",
         ["SWDIO — PA13　SWCLK — PA14", "GND → 主板 GND"], 450, 96, GREEN),
        ("ATK-BLE02（待定，未装）",
         ["TXD→PE7　RXD→PE8", "LINK→PD14　STA→PD15", "5V / GND"], 586, 110, YELLOW),
    ]
    for title, lines, y, h, color in right:
        box(d, 1620, y, 560, h, title, lines, color)
        ymid = y + h / 2
        line(d, [(1400, ymid), (1620, ymid)], color)
        d.ellipse([1395, ymid - 5, 1405, ymid + 5], fill=color)

    # ----------------------------------------------------------- 功率级
    box(d, 880, 900, 620, 210, "TB6612FNG 双通道驱动（待定）",
        ["VM ← 12V（降压后）　VCC ← 3V3",
         "PWMA←PA0　AIN1←PE13　AIN2←PE14",
         "PWMB←PA2　BIN1←PD8　BIN2←PD9",
         "STBY←PA1（高电平使能）",
         "A01/A02 左电机　B01/B02 右电机",
         "GND 与主板单点共地"], YELLOW)

    box(d, 60, 900, 480, 96, "左电机 TT + AB 编码器（待定）", ["A01 / A02 → 电机两端"], YELLOW)
    box(d, 60, 1040, 480, 96, "右电机 TT + AB 编码器（待定）", ["B01 / B02 → 电机两端"], YELLOW)
    line(d, [(540, 948), (880, 948)], POWER)
    line(d, [(540, 1088), (700, 1088), (700, 1060), (880, 1060)], POWER)

    # 12V：分压 → TB6612 VM（走 x=620 通道）
    line(d, [(430, 736), (620, 736), (620, 1040), (880, 1040)], POWER)
    label(d, 640, 1010, "12V → VM", POWER)

    # 舵机
    box(d, 1620, 900, 560, 120, "LD-220MG 数字舵机（待定）",
        ["信号 ← PE9（TIM1_CH1，50Hz）", "电源 ← UBEC 5V　GND 共地", "行程/限位未标定"], YELLOW)
    line(d, [(1400, 700), (1520, 700), (1520, 930), (1620, 930)], SIGNAL)
    label(d, 1528, 900, "PE9", SIGNAL)

    # 5V → 舵机（走 x=580 通道，绕开电机框）
    line(d, [(430, 600), (580, 600), (580, 1290), (1560, 1290), (1560, 990), (1620, 990)], POWER)
    label(d, 604, 1262, "UBEC 5V → 舵机", POWER)

    # 编码器
    box(d, 1620, 1060, 560, 130, "编码器（待定，固件未启用）",
        ["左：A→PB4(TIM3_CH1)　B→PB5(TIM3_CH2)",
         "右：A→PB6(TIM4_CH1)　B→PB7(TIM4_CH2)",
         "CHEN_FEATURE_ENCODER=0，仅预留"], YELLOW)

    # 急停
    box(d, 880, 1160, 620, 110, "急停与独立断能（待定）",
        ["硬件 NC 回路未落地，人工再使能未实现",
         "软件侧 PA6 检测默认关闭（FEATURE_ESTOP_INPUT=0）"], YELLOW)

    # GND 汇流
    box(d, 60, 1240, 480, 90, "GND 单点汇流（星形）",
        ["电池负 / UBEC 负 / TB6612 / 主板 / 舵机 / 适配器"], GND)
    line(d, [(300, 1330), (300, 1380), (2480, 1380)], GND)
    for x in (880, 1180, 1620, 1900):
        d.ellipse([x - 6, 1374, x + 6, 1386], fill=GND)
    label(d, 340, 1388, "所有地线汇到这一点后再连电池负极（避免回流串扰）", GND)

    # ----------------------------------------------------------- 表
    top = 1430
    d.rounded_rectangle([40, top, W - 40, H - 24], radius=10, outline=(70, 70, 70), width=2,
                        fill=(252, 252, 252))
    d.text((60, top + 8), "接线表（色块含义：绿=已确认 / 黄=待定 / 红=严重问题）",
           font=F_B, fill=TEXT)

    rows = [
        (GREEN, "电源", "电池 + → Shortsaver2 → 分电 → UBEC-8A；UBEC 输出取 5V 档"),
        (YELLOW, "电源", "分电需自制 XT60 一拖二线束（18AWG，红正黑负）"),
        (RED, "电源", "TB6612 的 VM 经 12V 降压后接入；4S 满电 16.8V 超过其 15V 绝对最大值"),
        (GREEN, "地", "电池负、UBEC 负、12V 降压负、TB6612 GND、主板 GND、舵机 GND、适配器 GND 单点汇合"),
        (GREEN, "电机", "PWMA←PA0　AIN1←PE13　AIN2←PE14　PWMB←PA2　BIN1←PD8　BIN2←PD9　STBY←PA1　VCC←3V3"),
        (YELLOW, "电机", "左电机 A01/A02、右电机 B01/B02 接 TB6612；带载/堵转电流未验证"),
        (YELLOW, "舵机", "信号←PE9；电源←UBEC 5V；GND 共地；行程限位未标定"),
        (GREEN, "液晶", "CS←PE15　DC←PD10　SCK←PB3　MOSI←PD7　BLK←PB10　RST←PB11　KEY→PA5　5V/GND"),
        (YELLOW, "CAN", "CAN_H—PD1　CAN_L—PD0；适配器 UART 座 GND→主板 GND；需 CAN2.0 + 1 Mbps"),
        (GREEN, "调试", "SWDIO—PA13　SWCLK—PA14　GND→主板 GND"),
        (YELLOW, "蓝牙", "TXD→PE7　RXD→PE8　LINK→PD14　STA→PD15（模块未装）"),
        (YELLOW, "编码器", "左 A→PB4　左 B→PB5　右 A→PB6　右 B→PB7（固件未启用）"),
        (YELLOW, "急停", "硬件 NC 回路与人工再使能未落地；软件 PA6 检测默认关闭"),
        (GREEN, "充电", "格氏 iMars S60（2S–4S）接电池 XT60，充电时断开整车"),
    ]
    y = top + 42
    for color, tag, text in rows:
        d.rounded_rectangle([60, y + 1, 76, y + 17], radius=4, outline=color, width=2)
        d.text((88, y), tag, font=F_TB, fill=(120, 60, 30))
        d.text((170, y), text, font=F_TB, fill=TEXT)
        y += 29

    OUT.mkdir(parents=True, exist_ok=True)
    png = OUT / "wiring_chen_bot_v1_0.png"
    pdf = OUT / "wiring_chen_bot_v1_0.pdf"
    img.save(png)
    try:
        img.save(pdf, "PDF", resolution=150.0)
    except PermissionError:
        pdf = OUT / "wiring_chen_bot_v1_0_new.pdf"
        img.save(pdf, "PDF", resolution=150.0)
        print("locked, wrote", pdf)
    print(png)
    print(pdf)


if __name__ == "__main__":
    main()
