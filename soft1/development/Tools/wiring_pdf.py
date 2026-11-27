"""Rebuild the four-sheet V1.2 wiring review PDF (requires reportlab)."""
from pathlib import Path
import argparse
import os

from reportlab.lib.colors import HexColor
from reportlab.lib.pagesizes import A4, landscape
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.pdfgen.canvas import Canvas


W, H = landscape(A4)
INK = "#142539"
NAVY = "#123451"
BLUE = "#12639A"
GREEN = "#167262"
RED = "#A92A35"
AMBER = "#9B520C"
MUTED = "#4F6170"
EDGE = "#D3DFE8"
PALE = "#EFF5F9"
FONT = "CJK"
LEFT, RIGHT = 42, W - 42


class Drawing:
    def __init__(self, out):
        self.pdf = Canvas(str(out), pagesize=(W, H), pageCompression=1)
        self.pdf.setTitle("机甲杯电控 V1.2 接线图")
        self.pdf.setSubject("资料核对版／等待实物验收")
        self.page = 0

    def rect(self, x, y, w, h, fill, stroke=None, radius=0):
        p = self.pdf
        p.setFillColor(HexColor(fill))
        p.setStrokeColor(HexColor(stroke or fill))
        if radius:
            p.roundRect(x, H-y-h, w, h, radius, fill=1, stroke=1)
        else:
            p.rect(x, H-y-h, w, h, fill=1, stroke=1)

    def text(self, x, y, value, size=12, color=INK, align="left"):
        if align == "right":
            x -= pdfmetrics.stringWidth(value, FONT, size)
        if align == "center":
            x -= pdfmetrics.stringWidth(value, FONT, size) / 2
        self.pdf.setFillColor(HexColor(color))
        self.pdf.setFont(FONT, size)
        self.pdf.drawString(x, H-y, value)

    def line(self, x1, y1, x2, y2, color, dashed=False):
        p = self.pdf
        p.setStrokeColor(HexColor(color))
        p.setLineWidth(1.35)
        p.setDash(5, 3) if dashed else p.setDash()
        p.line(x1, H-y1, x2, H-y2)
        p.setDash()

    def arrow(self, x1, y, x2, color, dashed=False):
        self.line(x1, y, x2, y, color, dashed)
        d = 1 if x2 > x1 else -1
        self.line(x2 - 5*d, y-3, x2, y, color)
        self.line(x2 - 5*d, y+3, x2, y, color)

    def header(self, title, subtitle):
        self.page += 1
        self.rect(0, 0, W, 109, NAVY)
        self.text(LEFT, 26, "机甲杯  /  电控工程 V1.2", 11, "#C4DBEB")
        self.text(LEFT, 60, title, 27, "#FFFFFF")
        self.text(LEFT, 87, subtitle, 12, "#E1EEF7")
        self.text(RIGHT, 27, "评审候选  ·  未经实物验收", 11, "#FFDB9C", "right")

    def footer(self, source):
        self.line(LEFT, 552, RIGHT, 552, EDGE)
        self.text(LEFT, 572, source, 9, MUTED)
        self.text(RIGHT, 572, f"{self.page} / 4  ·  2026-11-27", 9, MUTED, "right")
        self.pdf.showPage()

    def note(self, y, title, lines):
        height = 26 + 18*len(lines)
        self.rect(LEFT, y, RIGHT-LEFT, height, "#FFF3E4", "#F3DBBD", 6)
        self.text(LEFT+12, y+17, title, 11, AMBER)
        for i, line in enumerate(lines):
            self.text(LEFT+12, y+34+18*i, line, 11)

    def cable(self, top, title, endpoint, rows, spacing=20):
        left_x, right_x = LEFT, 552
        outer = RIGHT - LEFT
        box = 250
        height = 34 + spacing*len(rows)
        self.rect(left_x, top, box, height, PALE, EDGE, 6)
        self.rect(right_x, top, RIGHT-right_x, height, PALE, EDGE, 6)
        self.text(left_x+12, top+19, title, 12, NAVY)
        self.text(RIGHT-12, top+19, endpoint, 12, NAVY, "right")
        for index, (label_a, label_b, kind, direction) in enumerate(rows):
            y = top+34+index*spacing
            color = {"signal": BLUE, "ground": GREEN, "power": RED, "hold": AMBER, "ref": BLUE}[kind]
            self.text(left_x+box-11, y+4, label_a, 11, INK, "right")
            self.text(right_x+10, y+4, label_b, 11)
            a, b = left_x+box, right_x
            if direction == "out":
                self.arrow(a, y, b, color, kind == "hold")
            elif direction == "in":
                self.arrow(b, y, a, color, kind == "hold")
            elif direction == "open":
                self.line(a, y, a+35, y, color, True)
                self.line(b-35, y, b, y, color, True)
                self.text((a+b)/2, y+4, "暂空", 10, AMBER, "center")
            else:
                self.line(a, y, b, y, color, kind == "hold")
        assert outer > box

    def rows(self, top, widths, headings, rows, height=25):
        x_end = LEFT + sum(widths)
        self.rect(LEFT, top, sum(widths), height, NAVY)
        for i, row in enumerate([headings, *rows]):
            y = top+i*height
            if i:
                self.rect(LEFT, y, sum(widths), height, PALE if i % 2 == 1 else "#FFFFFF")
            x = LEFT
            for width, value in zip(widths, row):
                assert pdfmetrics.stringWidth(value, FONT, 11) <= width-16, value
                self.text(x+8, y+16, value, 11, "#FFFFFF" if i == 0 else INK)
                x += width
            assert x == x_end

    def node(self, x, y, width, first, second):
        self.rect(x, y-19, width, 38, PALE, EDGE, 5)
        self.text(x+width/2, y-2, first, 11, NAVY, "center")
        self.text(x+width/2, y+12, second, 9, MUTED, "center")

    def overview(self):
        self.header("整车配电与独立急停", "动力仅覆盖底盘与独立舵机台架。虚线是待审的供电与安全路径。")
        self.text(LEFT, 134, "正电源分支", 13, NAVY)
        n = self.node
        n(LEFT, 251, 120, "4S 电池", "满充名义 16.8 V")
        n(196, 251, 129, "总保护与分配", "规格、线径待定")
        self.arrow(162, 251, 196, RED, True)
        self.line(325, 251, 352, 251, RED, True)
        self.line(352, 161, 352, 341, RED, True)
        for y in (161, 221, 281, 341):
            self.arrow(352, y, 381, RED, True)
        n(381, 161, 152, "控制支路保护", "校核 VIN 下限与跌落")
        n(381, 221, 152, "电机支路保护", "DFR0831 → 5 V 候选")
        n(381, 281, 152, "舵机台架支路保护", "K2 常开触点 → UBEC")
        n(381, 341, 152, "线圈支路保护", "降压与线圈规格待核")
        for y in (161, 221, 281, 341):
            self.arrow(533, y, 572, RED, True)
        n(572, 161, 227, "MC02 独立 XT30 VIN 正极", "与两组 XT30 2+2 受控 OUT 区分")
        n(572, 221, 227, "K1 常开触点 → TB6612 VM", "输出 5 V 候选，VCC 逻辑另供")
        n(572, 281, 227, "UBEC 输出 → 独立舵机台架", "7.4 V 候选，单独校验失电")
        n(572, 341, 227, "硬急停 NC ＋ 硬件人工再使能", "两线圈正端；负端经核定 ULN 灌电流")
        self.text(LEFT, 393, "电源回路", 12, NAVY)
        self.text(LEFT, 415, "电池负极到各模块电源负极走配电回流。MC02 与非隔离信号保留参考地。电机与舵机回流走各自电源支路。", 11)
        self.text(LEFT, 435, "VCC_5V 是板端可控对外 5 V。VCC_5VS 为系统 5 V，可受 USB 供电。两种网及外部 5 V 分开。", 11)
        self.note(465, "接电前确认", [
            "先核端子、绝缘、保险、续流、触点直流分断及独立断能保持。用假负载验证 MCU 卡高、触点异常和急停释放。",
            "软件急停反馈用于诊断。线圈电压、ULN 扩展板端子、独立串联切断器件和具体线束等待审核。"
        ])
        self.footer("依据  MC02 手册电源树、主板原理图 2/3/5 页、项目硬件架构与复核清单")

    def communications(self):
        self.header("PC 调试、BLE 与 SWD", "由主板原理图针号标识。每组先在实物上认 Pin 1，再核信号、地及电平。")
        self.cable(127, "MC02 P10  ·  USART1", "PC USB 转 3.3 V TTL", [
            ("P10.1  PA9 / TX", "RXD", "signal", "out"),
            ("P10.2  PA10 / RX", "TXD", "signal", "in"),
            ("P10.3  GND", "GND", "ground", "tie"),
        ])
        self.text(LEFT, 236, "P10 手册文字表为 1 GND / 2 TX / 3 RX。图 2 为 1 TX / 2 RX / 3 GND。转换器 VCC 悬空。", 11, AMBER)
        self.cable(246, "MC02 P13  ·  UART7", "ATK-BLE02  ·  对端版次待核", [
            ("P13.4  PE8 / TX", "RX（按官方资料确认）", "hold", "out"),
            ("P13.3  PE7 / RX", "TX（按官方资料确认）", "hold", "in"),
            ("P13.2  GND", "GND（按实物确认）", "ground", "tie"),
            ("P13.1  板端可控 5 V", "模块 VCC（规格待确认）", "hold", "open"),
        ])
        self.text(LEFT, 373, "LINK／STA 暂空，PD14／PD15 用于 UART9 预留。不可依据主板 VCC 脚推断 BLE 模块供电规格。", 11, AMBER)
        self.cable(382, "MC02 P1  ·  SWD", "ST-Link  ·  核探头端定义", [
            ("P1.1  目标 VDD 3.3 V", "VTref（确认仅为目标参考）", "ref", "out"),
            ("P1.2  GND", "GND", "ground", "tie"),
            ("P1.3  PA14 / SWCLK", "SWCLK", "signal", "in"),
            ("P1.4  PA13 / SWDIO", "SWDIO（双向）", "signal", "tie"),
        ])
        self.text(LEFT, 516, "P1 不引出 NRST。目标板用经核准的独立供电。先确认调试器口的 VTref 性质与供电路径。", 11)
        self.footer("依据  MC02 手册第 8/9 节、主板原理图 2 页及板视图、正点原子 BLE02 官方产品页")

    def lcd(self):
        self.header("官方 LCD 与五向按键", "U13 主板图与 TFT 子板图互相吻合。说明书第十四节文字表的针位不同，须在实物核对。")
        self.rows(128, [92, 175, 175, 145, 171],
                  ["主板 U13", "MC02 网名", "TFT U2 功能", "手册文字编号", "连接状态"], [
            ("1", "VCC_5VS", "1  电源输入 5 V", "1", "先量电源和极性"),
            ("2", "PE15 / SPI1_CS", "2  CS", "3", "主板 → TFT"),
            ("3", "PB3 / SPI1_SCK", "3  SCLK", "4", "主板 → TFT"),
            ("4", "PB4 / SPI1_MISO", "4  无屏端回传驱动", "5", "TX-only，检查无驱动"),
            ("5", "PD7 / SPI1_MOSI", "5  SDA／数据", "6", "主板 → TFT"),
            ("6", "PB10 / I2C2_SCL", "6  BLK／背光控制", "7", "官方屏使用 GPIO"),
            ("7", "PB11 / I2C2_SDA", "7  RES／复位", "8", "官方屏使用 GPIO"),
            ("8", "PD10 / GPIO", "8  D/C", "9", "主板 → TFT"),
            ("9", "PA5 / ADC1_INP19", "9  KEY／模拟电压", "10", "TFT → 主板"),
            ("10", "GND", "10  GND", "2", "共同参考地"),
        ], 23)
        self.text(LEFT, 402, "KEY 单线模拟电压  /  名义值", 13, NAVY)
        for i, (label, value) in enumerate((("中心", "0 V"), ("左", "0.66 V"), ("右", "1.32 V"),
                                            ("上", "1.98 V"), ("下", "2.64 V"), ("释放", "3.30 V"))):
            x = LEFT+127*i
            self.rect(x, 414, 116, 40, PALE, EDGE, 5)
            self.text(x+58, 430, label, 11, NAVY, "center")
            self.text(x+58, 446, value, 11, GREEN, "center")
        self.note(467, "硬件核对后才能插接", [
            "用导通档识别主板 U13 和模块 U2 的第 1 针、地和 KEY。11/12 为图中接地固定焊脚。排线两端视角可能镜像。",
            "板载 RGB 占 PA7。官方屏配置停用 PA5 上的 SPI6 SCK。PA5 无内部上下拉，按已确认模块电压标定。"
        ])
        self.footer("依据  MC02 手册第 14 节、主板原理图 5 页、DM_TFT-V1.1、ALL 例程与 ST DS13313")

    def actuators(self):
        self.header("双轮驱动、舵机台架与反馈", "以下端点用于工程核线。动力、急停、保护与反馈签核前，候选线束和对应驱动保持隔离。")
        self.cable(123, "MC02  ·  双轮控制候选", "TB6612  ·  H1 信号排", [
            ("P11 S1  PA0 / TIM2_CH1", "H1.8  PWMA", "hold", "out"),
            ("P11 S2  PA2 / TIM2_CH3", "H1.2  PWMB", "hold", "out"),
            ("P11 S4  PE13", "H1.6  AIN1", "hold", "out"),
            ("P16.13  PE14", "H1.7  AIN2", "hold", "out"),
            ("P16.19  PC12", "H1.4  BIN1", "hold", "out"),
            ("P16.21  PC11", "H1.3  BIN2", "hold", "out"),
            ("P16.23  PC10", "H1.5  STBY（驱动端硬下拉）", "hold", "out"),
            ("MC02 信号参考地", "H1.1  GND", "ground", "tie"),
        ], 18)
        self.rows(311, [141, 263, 354], ["线束／节点", "一端", "另一端及核对项"], [
            ("TB 供电", "K1 后 5 V／核准的 3.3 V／配电负极", "H2.1 VM／H2.2 VCC／H2.3、H2.8 GND"),
            ("双轮电机", "H2.4 AO1、.5 AO2／H2.7 BO1、.6 BO2", "A、B 两电机端子；方向通过授权单轮验证"),
            ("舵机台架", "P11 S3 PE9；独立 UBEC 正负极", "控制信号；电源正负极；负端作信号参考"),
            ("线圈命令", "P16.17 PE1／P16.15 PE0", "ULN IN1/IN2 候选；驱动端下拉；待测"),
            ("急停反馈", "P15.3 PE2／P15.4 PE3", "隔离的辅助 NC 健康／人工再使能候选"),
            ("ADC 反馈", "PC4 板载；DCMI.16 PA4／.8 PA6", "板侧 VIN；独立保护后读 K1/K2 下游"),
        ], 22)
        self.note(473, "保留断开项", [
            "P11 三列为信号、可控 5 V、地。信号定位 S1 至 S4 对应 PA0、PA2、PE9、PE13。5 V 列在本页均留空。",
            "K1/K2 ADC 接口在分压、保护及回供校核前断开。P15.1 的 5 V 不参与急停。电流和编码器接口待选。"
        ])
        self.footer("依据  MC02 主板原理图 2/6 页、TB6612 模块图、项目 GPIO 清单和硬件复核表")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--font", type=Path,
                        default=Path(os.environ.get("WINDIR", "C:/Windows")) / "Fonts/simhei.ttf")
    args = parser.parse_args()
    pdfmetrics.registerFont(TTFont(FONT, str(args.font)))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    drawing = Drawing(args.output)
    drawing.overview()
    drawing.communications()
    drawing.lcd()
    drawing.actuators()
    drawing.pdf.save()


if __name__ == "__main__":
    main()
