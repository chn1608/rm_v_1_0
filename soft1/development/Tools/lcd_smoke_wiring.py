"""Build the isolated MC02 LCD and SWD bench wiring diagram."""
import argparse
import os
from pathlib import Path

from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont

from wiring_pdf import BLUE, EDGE, FONT, LEFT, MUTED, NAVY, RED, RIGHT, Drawing


class BenchDrawing(Drawing):
    def footer(self, source):
        self.line(LEFT, 550, RIGHT, 550, EDGE)
        self.text(LEFT, 572, source, 9, MUTED)
        self.text(RIGHT, 572, f"{self.page} / 3  ·  2026-11-27", 9, MUTED, "right")
        self.pdf.showPage()

    def swd(self):
        self.header("MC02 与 ST-LINK/V2 接线", "仅接核心板、配套 LCD 和调试器。主板经 USB-C 取电，SWD 接线独立。")
        self.text(LEFT, 133, "两条 USB 路径", 13, NAVY)
        self.node(LEFT, 173, 174, "电脑 USB 口 A", "标准 5 V USB 供电")
        self.node(316, 173, 220, "MC02 USB-C", "VBUS → D14 → 系统 5 V")
        self.node(630, 173, 170, "U13 → 配套 LCD", "系统 5 V 与信号")
        self.arrow(216, 173, 316, RED)
        self.arrow(536, 173, 630, RED)
        self.node(LEFT, 222, 174, "电脑 USB 口 B", "调试器独立供电")
        self.node(316, 222, 220, "ST-LINK/V2 USB", "连接 Keil uVision")
        self.arrow(216, 222, 316, BLUE)
        self.text(552, 224, "供电经 USB-C，SWD 的 VAPP 取目标参考。", 10, MUTED)
        self.cable(257, "MC02 P1  ·  SWD", "原装 ST-LINK/V2  ·  CN3 20 针", [
            ("P1.1  目标 3.3 V", "CN3.1/.2  VAPP 参考", "ref", "out"),
            ("P1.2  GND", "CN3.4  GND（至少一根地）", "ground", "tie"),
            ("P1.3  PA14 / SWCLK", "CN3.9  SWCLK", "signal", "in"),
            ("P1.4  PA13 / SWDIO", "CN3.7  SWDIO（双向）", "signal", "tie"),
        ], 21)
        self.text(LEFT, 392, "原装盒装版用 CN3 的 SWD 信号。CN3.19 VDD 输出悬空。P1 无 NRST，调试选 SW，SWO 留空。", 11)
        self.text(LEFT, 412, "小棒版按已核实的 GND、SWCLK、SWDIO 丝印接。3.3 V／5 V 输出悬空。只有明确标为参考输入的端子才接 P1.1。", 11)
        self.note(442, "首次通电", [
            "USB 全部拔掉后压接线束。用导通档核 P1.2 地，确认 Pin 1 和 LCD 排线方向。先接主板 USB-C，再接探头 USB。",
            "XT30 VIN、两组 OUT、PWM、外接 5 V 均留空。若板端 3.3 V 不稳或点亮后反复复位，先断电检查供电和排线。"
        ])
        self.footer("依据  达妙 MC02 V1.0 主板图 1–3 页及板视图，ST UM1075 Rev 10 表 4")

    def panel(self):
        self.header("U13 与官方 LCD 逐针核对", "主板 V1.0 原理图 U13 对 DM_TFT-V1.1 子板 U2。配套连接方向以实物 Pin 1 和通断为准。")
        self.rows(128, [100, 158, 170, 152, 177], ["U13 → U2", "主板网络／引脚", "TFT 端", "手册文字编号", "本验证用法"], [
            ("1 → 1", "VCC_5VS", "电源输入", "1", "主板系统 5 V 网"),
            ("2 → 2", "PE15", "CS", "3", "片选"),
            ("3 → 3", "PB3 / SPI1 SCK", "SCLK", "4", "时钟，SWO 留空"),
            ("4 → 4", "PB4 / SPI1 MISO", "预留", "5", "固件不驱动"),
            ("5 → 5", "PD7 / SPI1 MOSI", "SDA", "6", "显示数据"),
            ("6 → 6", "PB10", "BLK", "7", "背光"),
            ("7 → 7", "PB11", "RES", "8", "复位"),
            ("8 → 8", "PD10", "D/C", "9", "数据／命令"),
            ("9 → 9", "PA5 / ADC1_INP19", "KEY", "10", "五向键模拟输出"),
            ("10 → 10", "GND", "地", "2", "共同参考地"),
        ], 23)
        self.text(LEFT, 400, "板上定位", 13, NAVY)
        self.text(LEFT, 422, "参考板视图中，USB-C 朝上，LCD 插座在右边，P1 在底边用户键旁。接口朝向以板上丝印和导通结果为准。", 11)
        self.text(LEFT, 442, "排线从两端看可能镜像。断电核通 U13.10 到主板地及 U2.10 到子板地，再核第 1 针和第 9 针。", 11)
        self.note(465, "手册文字表与原理图有冲突", [
            "手册列第 2 针为 GND、第 10 针为 KEY。主板图与 TFT 子板图均列第 10 针为 GND、第 9 针为 KEY。",
            "11/12 是图中接地固定焊脚。使用匹配模块和配套连接。针序有出入时，保持断电并复核。"
        ])
        self.footer("依据  达妙 MC02 手册第 14 节、主板原理图 5 页、管脚标注图、DM_TFT-V1.1")


    def serial(self):
        self.header("USB 转串口诊断接线", "可选连接。USART1 用 3.3 V TTL、115200 bit/s、8N1、无流控。")
        self.node(LEFT, 161, 195, "电脑 USB 口 C", "独立连接串口模块")
        self.node(318, 161, 217, "USB 转串口模块", "确认 TXD 为 3.3 V TTL")
        self.arrow(237, 161, 318, BLUE)
        self.text(555, 164, "MC02 继续经自己的 USB-C 供电。", 11, MUTED)
        self.cable(210, "MC02 P10  ·  USART1", "USB 转串口模块  ·  TTL 端", [
            ("P10.1  PA9 / TX", "RXD", "signal", "out"),
            ("P10.2  PA10 / RX", "TXD", "signal", "in"),
            ("P10.3  GND", "GND", "ground", "tie"),
        ], 26)
        self.text(LEFT, 348, "USB 转串口模块的 3.3 V、5 V、VCC 供电脚全部留空。P10 只有 TX、RX、GND 三个触点。", 11)
        self.text(LEFT, 370, "只验证接收时可以先接 P10.1 → RXD 和 P10.3 → GND。确认适配器 TXD 电平后再接 P10.2。", 11)
        self.note(407, "针序和电平", [
            "主板图为 1 TX、2 RX、3 GND。手册文字表为 1 GND、2 TX、3 RX。先断电核对 Pin 1、地和信号。",
            "5 V TTL、RS-232、RS-485 的信号不可直接接 P10。选择模块上对应 3.3 V 的 TTL 信号制式。",
            "先给主板接 USB，再接转串口的 USB。断电时先拔转串口 USB，避免 TXD 在主板掉电时仍驱动。",
            "串口助手选择该模块的 COM 口。观察启动和心跳，发送 ASCII ? 查询状态。ST-LINK/V2 仍用 SWD 调试。"
        ])
        self.footer("依据  达妙 MC02 手册第 8 节，主板原理图 2 页及管脚标注图")


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("output", type=Path)
    p.add_argument("--font", type=Path,
                   default=Path(os.environ.get("WINDIR", "C:/Windows")) / "Fonts/simhei.ttf")
    args = p.parse_args()
    pdfmetrics.registerFont(TTFont(FONT, str(args.font)))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    diagram = BenchDrawing(args.output)
    diagram.pdf.setTitle("MC02 LCD 与 ST-LINK/V2 技术验证接线图")
    diagram.pdf.setSubject("MC02 独立 USB 供电、SWD、TFT 与可选 USB 串口")
    diagram.swd()
    diagram.panel()
    diagram.serial()
    diagram.pdf.save()


if __name__ == "__main__":
    main()
