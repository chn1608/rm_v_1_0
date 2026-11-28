"""chen_bot_v_1_0 wiring diagram with a three-level colour code.

Green  = confirmed and usable (verified on the real board or in the official docs)
Yellow = pending / questionable / not yet fitted
Red    = serious problem that must be fixed before power-on

Every signal is named by its MCU pin (PAx/PBx/PCx/PDx/PEx); connector positions
are never written as "P7.4" style references.
"""
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

OUT = Path(__file__).resolve().parents[1] / "docs"
W, H = 2400, 1820

GREEN = (24, 120, 60)
YELLOW = (196, 140, 20)
RED = (198, 32, 32)
POWER = (176, 42, 42)
SIGNAL = (30, 78, 150)
EDGE = (70, 70, 70)
TEXT = (25, 25, 25)


def font(size, bold=False):
    return ImageFont.truetype(rf"C:\Windows\Fonts\{'msyhbd.ttc' if bold else 'msyh.ttc'}", size)


F_TITLE = font(34, True)
F_BOX = font(19, True)
F_LINE = font(16)
F_SMALL = font(15)
F_TABLE = font(16)


def box(d, x, y, w, h, title, lines, edge):
    d.rounded_rectangle([x, y, x + w, y + h], radius=10, outline=edge, width=3, fill="white")
    d.text((x + 14, y + 9), title, font=F_BOX, fill=TEXT)
    for i, line in enumerate(lines):
        d.text((x + 14, y + 36 + i * 22), line, font=F_LINE, fill=TEXT)


def arrow(d, x0, y0, x1, y1, color):
    d.line([(x0, y0), (x1, y1)], fill=color, width=3)
    d.ellipse([x1 - 5, y1 - 5, x1 + 5, y1 + 5], fill=color)


def main():
    img = Image.new("RGB", (W, H), "white")
    d = ImageDraw.Draw(img)

    d.text((40, 22), "chen_bot_v_1_0 接线图（DM-MC-Board02 / STM32H723VGT6）",
           font=F_TITLE, fill=TEXT)
    d.text((44, 68), "全部引脚以 MCU 引脚名标注　|　线色：红=电源 蓝=信号 黑=地",
           font=F_SMALL, fill=(90, 90, 90))

    # 图例
    lx = 44
    for label, color in (("绿框：已确认可用", GREEN), ("黄框：待定/有问题", YELLOW), ("红框：严重问题", RED)):
        d.rounded_rectangle([lx, 96, lx + 26, 122], radius=5, outline=color, width=3)
        d.text((lx + 34, 99), label, font=F_SMALL, fill=color)
        lx += 230

    # ------------------------------------------------------------ 电源链
    px = 40
    box(d, px, 140, 330, 84, "4S 锂电池（已确认）",
        ["格氏 ACE 3300mAh 30C", "14.8V，满电 16.8V，XT60"], GREEN)
    box(d, px, 264, 330, 74, "VIFLY Shortsaver2（已确认）", ["XT30/XT60 防短路保护"], GREEN)
    box(d, px, 378, 330, 96, "分电 XT60 → 2×XT30（待定）",
        ["清单里没有分电板", "需自制 XT60 一拖二线束"], YELLOW)
    box(d, px, 514, 330, 104, "UBEC-8A（待定）",
        ["输出 5V/6V/7.4V 跳线", "切到 5V 才能接主板", "舵机电压需同档"], YELLOW)
    box(d, px, 658, 330, 104, "12V 降压（严重问题）",
        ["TB6612 VM 绝对最大 15V", "4S 满电 16.8V 会超压", "必须补一个 12V 降压"], RED)

    arrow(d, 205, 224, 205, 264, POWER)
    arrow(d, 205, 338, 205, 378, POWER)
    arrow(d, 205, 474, 205, 514, POWER)
    arrow(d, 205, 618, 205, 658, POWER)

    # ------------------------------------------------------------ 主控
    box(d, 720, 330, 600, 470, "主控 DM-MC-Board02（已确认）",
        ["供电：USB-C 5V（当前实测工作方式）",
         "SWD：PA13 / PA14",
         "CAN1：PD0(RX) / PD1(TX)",
         "ADC：PA5 五向键 / PC4 电池 / PA6 急停",
         "PWM：PA0(TIM2_CH1) / PA2(TIM2_CH3)",
         "方向：PE13 PE14 PD8 PD9　STBY→PA1",
         "舵机：PE9（TIM1_CH1）",
         "SPI1：PB3 PD7 PE15 PD10 PB10 PB11",
         "UART7：PE7 PE8 / PD14 PD15",
         "预留编码器：PB4 PB5 / PB6 PB7"], GREEN)

    arrow(d, 370, 566, 700, 566, POWER)
    d.text((450, 540), "5V（USB-C 或 UBEC 5V）", font=F_SMALL, fill=POWER)
    arrow(d, 370, 710, 660, 710, POWER)
    d.line([(660, 710), (660, 916), (1180, 916), (1180, 940)], fill=POWER, width=3)
    d.text((560, 690), "12V（降压后）", font=F_SMALL, fill=POWER)

    # ------------------------------------------------------------ 右侧
    right = [
        ("DM_TFT-V1.1 240×280（已确认）",
         ["CS←PE15　DC←PD10", "SCK←PB3　MOSI←PD7",
          "BLK←PB10　RST←PB11", "KEY→PA5　5V/GND", "实物已验证：显示与按键可用"], 140, 160, GREEN),
        ("达妙 USB2CAN（待定）",
         ["CAN_H — PD1　CAN_L — PD0", "UART 座 GND → 主板 GND",
          "适配器需 1Mbps/CAN2.0", "当前总线无应答，链路未通"], 370, 140, YELLOW),
        ("ST-Link V2（已确认）",
         ["SWDIO — PA13　SWCLK — PA14", "GND 必须接"], 570, 96, GREEN),
        ("ATK-BLE02（待定，未装）",
         ["TXD→PE7　RXD→PE8", "LINK→PD14　STA→PD15", "需 AT 指令与固件确认"], 726, 120, YELLOW),
    ]
    for title, lines, y, h, color in right:
        box(d, 1520, y, 520, h, title, lines, color)
        d.line([(1320, y + h / 2), (1520, y + h / 2)], fill=color, width=3)
        d.ellipse([1315, y + h / 2 - 5, 1325, y + h / 2 + 5], fill=color)

    # ------------------------------------------------------------ 功率侧
    box(d, 720, 940, 600, 180, "TB6612FNG 双通道驱动（待定）",
        ["VM ← 12V　VCC ← 3V3　STBY ← PA1",
         "PWMA←PA0　AIN1←PE13　AIN2←PE14",
         "PWMB←PA2　BIN1←PD8　BIN2←PD9",
         "A01/A02 左电机　B01/B02 右电机",
         "带载/堵转/温升未验证"], YELLOW)

    box(d, 40, 940, 420, 86, "左电机 TT + AB 编码器（待定）", ["A01 / A02 → 电机两端"], YELLOW)
    box(d, 40, 1046, 420, 86, "右电机 TT + AB 编码器（待定）", ["B01 / B02 → 电机两端"], YELLOW)
    arrow(d, 460, 983, 720, 983, POWER)
    d.line([(460, 1089), (600, 1089), (600, 1035), (720, 1035)], fill=POWER, width=3)

    box(d, 1440, 940, 600, 120, "LD-220MG 数字舵机（待定）",
        ["信号 ← PE9（TIM1_CH1，50Hz）", "电源 ← UBEC（5V 档）　GND 共地", "行程/限位未标定"], YELLOW)
    arrow(d, 390, 604, 390, 866, POWER)
    d.line([(390, 866), (1700, 866), (1700, 1000), (1440, 1000)], fill=POWER, width=3)
    d.text((420, 840), "UBEC 5V → 舵机（走主控下方，不穿框）", font=F_SMALL, fill=POWER)

    box(d, 1440, 1100, 600, 130, "编码器接线（待定，固件未启用）",
        ["左：A→PB4(TIM3_CH1)　B→PB5(TIM3_CH2)",
         "右：A→PB6(TIM4_CH1)　B→PB7(TIM4_CH2)",
         "CHEN_FEATURE_ENCODER=0，引脚仅预留"], YELLOW)

    box(d, 720, 1150, 600, 110, "急停与独立断能（待定）",
        ["硬件急停 NC 回路尚未落地", "人工再使能未实现（HARDWARE_TODO）",
         "软件侧 PA6 触点检测：CHEN_FEATURE_ESTOP_INPUT=0"], YELLOW)

    # ------------------------------------------------------------ 连接表
    top = 1300
    d.rounded_rectangle([40, top, W - 40, H - 24], radius=10, outline=EDGE, width=2, fill=(252, 252, 252))
    d.text((60, top + 8), "接线表（绿=已确认 / 黄=待定 / 红=严重问题）", font=F_BOX, fill=TEXT)

    rows = [
        (GREEN, "电源", "USB-C 5V → 主板（当前实测工作方式）；电池经 Shortsaver2 保护"),
        (YELLOW, "电源", "分电需自制 XT60 一拖二；UBEC-8A 必须切 5V 档才能接主板"),
        (RED, "电源", "TB6612 VM 只能接 12V（降压后）：4S 满电 16.8V 超过其 15V 绝对最大值"),
        (GREEN, "地", "电池负、UBEC 负、TB6612 GND、主板 GND、舵机 GND、适配器 GND 单点汇合"),
        (GREEN, "电机", "PWMA←PA0　AIN1←PE13　AIN2←PE14　PWMB←PA2　BIN1←PD8　BIN2←PD9　STBY←PA1　VCC←3V3"),
        (YELLOW, "电机", "左电机 A01/A02、右电机 B01/B02 接 TB6612；带载与堵转电流未验证"),
        (YELLOW, "舵机", "信号←PE9；电源←UBEC 5V；GND 共地；行程限位未标定"),
        (GREEN, "液晶", "CS←PE15　DC←PD10　SCK←PB3　MOSI←PD7　BLK←PB10　RST←PB11　KEY→PA5　5V/GND"),
        (YELLOW, "CAN", "CAN_H—PD1　CAN_L—PD0；适配器 UART GND→主板 GND；总线当前无 ACK"),
        (GREEN, "调试", "SWDIO—PA13　SWCLK—PA14　GND→主板 GND"),
        (YELLOW, "蓝牙", "TXD→PE7　RXD→PE8　LINK→PD14　STA→PD15（模块未装、AT 未确认）"),
        (YELLOW, "编码器", "左 A→PB4　左 B→PB5　右 A→PB6　右 B→PB7（固件未启用）"),
        (YELLOW, "急停", "硬件 NC 回路与人工再使能未落地；PA6 检测默认关闭"),
        (GREEN, "充电", "格氏 iMars S60（2S–4S）接电池 XT60，充电时断开整车"),
    ]
    y = top + 40
    for color, tag, line in rows:
        d.rounded_rectangle([58, y + 1, 74, y + 17], radius=4, outline=color, width=2)
        d.text((86, y), tag, font=F_TABLE, fill=(120, 60, 30))
        d.text((168, y), line, font=F_TABLE, fill=TEXT)
        y += 28

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
