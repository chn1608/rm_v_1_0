"""chen_bot_v_1_0 上位机（图形界面）。

tkinter 写成，Anaconda / 官方 Python 自带，无需额外安装。
数据来自达妙 USB2CAN 适配器（出厂 CDC 固件，官方串口协议），
没接硬件时可用「演示模式」查看界面。

启动：
    python gui.py
    python gui.py --demo
"""
from __future__ import annotations

import argparse
import ctypes
import math
import queue
import sys
import threading
import time
import tkinter as tk
from ctypes import wintypes
from tkinter import messagebox, ttk

from protocol import Decoder, Frame, STATUS, decode_status

import ctypes
from ctypes import wintypes


MUTEX_NAME = "Global\\chen_bot_v1_0_gui"
ERROR_ALREADY_EXISTS = 183


def claim_single_instance():
    """只允许一个状态窗口；已有实例时把它提到前面并退出本进程。"""
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.CreateMutexW.restype = wintypes.HANDLE
    handle = kernel32.CreateMutexW(None, False, MUTEX_NAME)
    if not handle:
        return None
    if ctypes.get_last_error() == ERROR_ALREADY_EXISTS:
        user32 = ctypes.WinDLL("user32", use_last_error=True)
        hwnd = user32.FindWindowW(None, "chen_bot_v_1_0 上位机")
        if hwnd:
            user32.ShowWindow(hwnd, 9)      # SW_RESTORE
            user32.SetForegroundWindow(hwnd)
        return None
    return handle


FONT = ("Microsoft YaHei UI", 10)
FONT_BIG = ("Microsoft YaHei UI", 20, "bold")
FONT_MID = ("Microsoft YaHei UI", 11, "bold")
FONT_SMALL = ("Microsoft YaHei UI", 9)

REFRESH_MS = 100
POLL_INTERVAL = 0.1
HISTORY = 120


class DataWorker(threading.Thread):
    """在后台线程里轮询主板，把结果放进队列。"""

    def __init__(self, link, out: queue.Queue, demo: bool = False) -> None:
        super().__init__(daemon=True)
        self._link = link
        self._out = out
        self._demo = demo
        self._stop = threading.Event()
        self._sequence = 0
        self._t0 = time.monotonic()

    def stop(self) -> None:
        self._stop.set()

    def run(self) -> None:
        decoder = Decoder()
        while not self._stop.is_set():
            try:
                if self._demo:
                    self._out.put(("row", self._demo_row()))
                    time.sleep(POLL_INTERVAL)
                    continue

                sequence = self._sequence & 0xFFFF
                self._sequence += 1
                fails_before = getattr(self._link, "tx_fail", 0)
                self._link.write(Frame(STATUS, sequence).encode())

                got = False
                deadline = time.monotonic() + 0.5
                while time.monotonic() < deadline and not self._stop.is_set():
                    chunk = self._link.read(73)
                    if not chunk:
                        continue
                    for frame in decoder.feed(chunk, 0):
                        if frame.kind == 0x82:
                            self._out.put(("row", decode_status(frame)))
                            got = True
                            break
                    if got:
                        break
                if not got:
                    fails_now = getattr(self._link, "tx_fail", 0)
                    if fails_now > fails_before:
                        self._out.put(("noack", ""))
                    else:
                        self._out.put(("warn", "本轮没有收到状态帧"))
            except Exception as exc:                       # noqa: BLE001
                self._out.put(("error", str(exc)))
                return
            time.sleep(max(0.0, POLL_INTERVAL))

    def _demo_row(self) -> dict:
        t = time.monotonic() - self._t0
        return dict(uptime_s=int(t), state="remote", owner="pc", motion_permitted=True,
                    power_enabled=True, ble_link=True, pc_link=True,
                    left_cmd=120, right_cmd=-80, left_out=100, right_out=-60,
                    servo_us=int(1500 + 60 * math.sin(t)),
                    servo_mdeg=int(500 * math.sin(t)),
                    bus_mv=int(14800 + 220 * math.sin(t / 3)),
                    bus_source="measured", current_ma=1200, current_source="measured",
                    faults=0, good_frames=int(t * 10), bad_frames=0,
                    loop_max_us=1800)


class App(ttk.Frame):
    FIELDS = [
        ("uptime_s", "运行时间", " s"),
        ("servo_us", "舵机脉冲", " us"),
        ("servo_mdeg", "舵机角度", " m°"),
        ("left_out", "左轮输出", "‰"),
        ("right_out", "右轮输出", "‰"),
        ("loop_max_us", "最大循环", " us"),
        ("good_frames", "有效帧", ""),
        ("bad_frames", "错误帧", ""),
        ("current_ma", "电流", " mA"),
    ]

    def __init__(self, master: tk.Tk, demo: bool = False) -> None:
        super().__init__(master, padding=10)
        self.master = master
        self.demo = demo
        self.link = None
        self.worker = None
        self._connecting = False
        self._noack_logged = 0.0
        self.queue: queue.Queue = queue.Queue()
        self.history: list[float] = []
        self.vars: dict[str, tk.StringVar] = {}
        self._build()
        self.after(REFRESH_MS, self._drain)
        if demo:
            self._start_demo()

    # ------------------------------------------------------------------ UI
    def _build(self) -> None:
        self.master.title("chen_bot_v_1_0 上位机")
        self.master.geometry("1120x760")
        self.master.minsize(1040, 700)
        self.pack(fill="both", expand=True)
        self.columnconfigure(0, weight=1)
        self.rowconfigure(3, weight=1)

        bar = ttk.Frame(self)
        bar.grid(row=0, column=0, sticky="ew")
        ttk.Label(bar, text="串口", font=FONT).pack(side="left")
        self.port_var = tk.StringVar()
        self.port_box = ttk.Combobox(bar, textvariable=self.port_var, width=10,
                                     font=FONT, state="readonly")
        self.port_box.pack(side="left", padx=6)
        ttk.Button(bar, text="刷新", command=self._refresh_ports).pack(side="left")
        self.connect_btn = ttk.Button(bar, text="连接", command=self._toggle)
        self.connect_btn.pack(side="left", padx=8)
        self.status_var = tk.StringVar(value="未连接")
        self.status_label = ttk.Label(bar, textvariable=self.status_var, font=FONT_MID,
                                      foreground="#b03030")
        self.status_label.pack(side="left", padx=10)
        ttk.Label(bar, text="请求周期 100 ms", font=FONT).pack(side="right")

        big = ttk.Frame(self)
        big.grid(row=1, column=0, sticky="ew", pady=(10, 4))
        for i in range(3):
            big.columnconfigure(i, weight=1, uniform="big")
        self.voltage_var = tk.StringVar(value="--")
        self.state_var = tk.StringVar(value="--")
        self.owner_var = tk.StringVar(value="--")
        for i, (title, var, color) in enumerate((("电池电压", self.voltage_var, "#1f6f3f"),
                                                 ("系统状态", self.state_var, "#1f4e8c"),
                                                 ("控制权", self.owner_var, "#7a4b00"))):
            cell = ttk.LabelFrame(big, text=title, padding=6)
            cell.grid(row=0, column=i, sticky="nsew", padx=3)
            ttk.Label(cell, textvariable=var, font=FONT_BIG, foreground=color,
                      anchor="center").pack(fill="x")

        mid = ttk.Frame(self)
        mid.grid(row=2, column=0, sticky="ew")
        for i in range(3):
            mid.columnconfigure(i, weight=1, uniform="mid")
        for index, (key, label, unit) in enumerate(self.FIELDS):
            box = ttk.LabelFrame(mid, text=label, padding=4)
            box.grid(row=index // 3, column=index % 3, sticky="nsew", padx=3, pady=3)
            var = tk.StringVar(value="--")
            self.vars[key] = var
            ttk.Label(box, textvariable=var, font=FONT_MID, anchor="center").pack(fill="x")

        low = ttk.Frame(self)
        low.grid(row=3, column=0, sticky="nsew", pady=(8, 0))
        low.columnconfigure(0, weight=3)
        low.columnconfigure(1, weight=2)
        low.rowconfigure(0, weight=1)
        chart = ttk.LabelFrame(low, text="电池电压趋势（最近 120 点）", padding=6)
        chart.grid(row=0, column=0, sticky="nsew")
        self.canvas = tk.Canvas(chart, background="#fbfbfb", highlightthickness=0, height=200)
        self.canvas.pack(fill="both", expand=True)

        right = ttk.LabelFrame(low, text="事件", padding=6)
        right.grid(row=0, column=1, sticky="nsew", padx=(8, 0))
        self.log = tk.Text(right, height=10, font=FONT, state="disabled",
                           background="#ffffff", relief="flat", wrap="word")
        self.log.pack(fill="both", expand=True)

        foot = ttk.Frame(self)
        foot.grid(row=4, column=0, sticky="ew", pady=(8, 0))
        ttk.Button(foot, text="清空事件", command=self._clear_log).pack(side="left")
        ttk.Label(foot, text="数据来源：达妙 USB2CAN（官方 CDC 协议）   MCU：STM32H723VGT6",
                  font=FONT_SMALL, foreground="#666666").pack(side="right")

    # ------------------------------------------------------------- actions
    def _refresh_ports(self) -> None:
        try:
            from serial.tools import list_ports
            ports = [p.device for p in sorted(list_ports.comports())]
        except Exception:                                  # noqa: BLE001
            ports = []
        self.port_box["values"] = ports
        if ports and not self.port_var.get():
            self.port_var.set(ports[0])
        self._log("找到串口：" + (", ".join(ports) if ports else "无"))

    def _toggle(self) -> None:
        if self.worker is not None:
            self._disconnect()
            return
        if self.demo:
            self._start_demo()
            return
        port = self.port_var.get().strip()
        if not port:
            messagebox.showwarning("提示", "请先选择串口（适配器以 USB 串行设备出现）")
            return
        if self._connecting:
            return
        self._connecting = True
        self.connect_btn.configure(text="连接中…", state="disabled")
        self.status_var.set("正在打开 %s …" % port)
        self.status_label.configure(foreground="#7a4b00")

        def open_port() -> None:
            try:
                from usb2can_link import Usb2CanLink
                link = Usb2CanLink(port)           # handshake takes ~1.2 s
            except Exception as exc:               # noqa: BLE001
                self.queue.put(("open_failed", str(exc)))
                return
            self.queue.put(("opened", (port, link)))

        threading.Thread(target=open_port, daemon=True).start()

    def _on_opened(self, port: str, link) -> None:
        self._connecting = False
        self.link = link
        self.connect_btn.configure(text="断开", state="normal")
        self._start_worker()
        self.status_var.set("已连接 %s" % port)
        self.status_label.configure(foreground="#1f6f3f")
        self._log("已连接 %s" % port)

    def _start_demo(self) -> None:
        self._start_worker()
        self.connect_btn.configure(text="断开")
        self.status_var.set("演示模式")
        self.status_label.configure(foreground="#7a4b00")
        self._log("演示模式：数据为本地生成")

    def _start_worker(self) -> None:
        self.worker = DataWorker(self.link, self.queue, demo=self.demo)
        self.worker.start()

    def _disconnect(self) -> None:
        self._connecting = False
        if self.worker is not None:
            self.worker.stop()
            self.worker.join(timeout=0.4)
            self.worker = None
        if self.link is not None:
            try:
                self.link.close()
            except Exception:                              # noqa: BLE001
                pass
            self.link = None
        self.connect_btn.configure(text="连接", state="normal")
        self.status_var.set("未连接")
        self.status_label.configure(foreground="#b03030")
        self._log("已断开")

    # -------------------------------------------------------------- update
    def _drain(self) -> None:
        try:
            while True:
                kind, payload = self.queue.get_nowait()
                if kind == "row":
                    self._apply(payload)
                elif kind == "warn":
                    self._log(payload)
                elif kind == "noack":
                    now = time.monotonic()
                    if now - self._noack_logged > 2.0:
                        self._noack_logged = now
                        self._log("总线无应答（0x02）：检查 CAN_H/CAN_L、GND 共地与 5V 供电")
                elif kind == "opened":
                    self._on_opened(*payload)
                elif kind == "open_failed":
                    self._connecting = False
                    self.connect_btn.configure(text="连接", state="normal")
                    self.status_var.set("未连接")
                    self.status_label.configure(foreground="#b03030")
                    self._log("打开串口失败：" + str(payload))
                else:
                    self._log("错误：" + str(payload))
                    self._disconnect()
        except queue.Empty:
            pass
        self.after(REFRESH_MS, self._drain)

    def _apply(self, row: dict) -> None:
        mv = row.get("bus_mv")
        self.voltage_var.set("--" if mv is None else "%.2f V" % (mv / 1000.0))
        self.state_var.set(str(row.get("state", "--")))
        self.owner_var.set(str(row.get("owner", "--")))
        for key, _label, unit in self.FIELDS:
            value = row.get(key)
            self.vars[key].set("--" if value is None else "%s%s" % (value, unit))
        if mv:
            self.history.append(mv / 1000.0)
            del self.history[:-HISTORY]
            self._draw()
        if row.get("faults"):
            self._log("故障码 0x%08X" % row["faults"])

    def _draw(self) -> None:
        c = self.canvas
        c.delete("all")
        w, h = c.winfo_width(), c.winfo_height()
        if w < 40 or h < 40 or len(self.history) < 2:
            return
        lo, hi = min(self.history), max(self.history)
        if hi - lo < 0.4:
            lo, hi = lo - 0.2, hi + 0.2
        pad = 26
        step = (w - 2 * pad) / max(1, HISTORY - 1)
        points = []
        for i, v in enumerate(self.history):
            points.extend([pad + i * step, h - pad - (v - lo) / (hi - lo) * (h - 2 * pad)])
        c.create_line(*points, fill="#1f6f3f", width=2)
        c.create_text(pad, 12, anchor="w", text="%.2f V" % hi, font=FONT_SMALL, fill="#666666")
        c.create_text(pad, h - 12, anchor="w", text="%.2f V" % lo, font=FONT_SMALL, fill="#666666")

    def _log(self, message: str) -> None:
        stamp = time.strftime("%H:%M:%S")
        self.log.configure(state="normal")
        self.log.insert("end", "[%s] %s\n" % (stamp, message))
        self.log.see("end")
        self.log.configure(state="disabled")

    def _clear_log(self) -> None:
        self.log.configure(state="normal")
        self.log.delete("1.0", "end")
        self.log.configure(state="disabled")


def main() -> int:
    parser = argparse.ArgumentParser(description="chen_bot_v_1_0 图形界面上位机")
    parser.add_argument("--demo", action="store_true", help="用本地生成的数据查看界面")
    args = parser.parse_args()

    guard = claim_single_instance()
    if guard is None:
        print("chen_bot_v_1_0 上位机已在运行，已切到该窗口。")
        return 0

    root = tk.Tk()
    app = App(root, demo=args.demo)
    app._refresh_ports()
    root.mainloop()
    return 0


if __name__ == "__main__":
    sys.exit(main())
