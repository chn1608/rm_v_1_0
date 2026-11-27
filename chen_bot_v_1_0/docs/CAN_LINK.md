# CAN 链路：接线、配置与联调

> 本文所有硬件与模块参数均出自官方资料：
> `gitee.com/kit-miao/damiao`
> - `开发板/控制板/DM-MC02`（子模块 `kit-miao/dm-mc02`）原理图 `drawings/2d/schematics/CtrBoard-H7_V1.0-240124.pdf`
> - `模块/通信模块/USB2CANFD`（子模块 `kit-miao/usb2canfd`）《达妙科技-USB转CANFD模块使用说明书V1.0》

## 1. 硬件接线

MC02 的 CAN1 与电源共用一个 4 芯端子 **P7（XT30PW-M 2+2）**：

| P7 引脚 | 信号 | 说明 |
|---|---|---|
| 1 | VCC_OUT1 | 板输出电源 |
| 2 | INGND | 电源地 |
| 3 | **CAN1_L** | 接模块 CAN_L |
| 4 | **CAN1_H** | 接模块 CAN_H |

MCU 侧：`CAN1_RX = PD0`、`CAN1_TX = PD1`（FDCAN1，AF9），板载收发器与 PESD1CAN 防护。

USB2CANFD 模块侧：

| 模块接口 | 类型 | 接线 |
|---|---|---|
| CAN | GH1.25 **2P** | 仅 CAN_H / CAN_L，**没有 GND 引脚** |
| 电源与通信 | Type-C | 接 PC，同时供电与通信 |
| 终端电阻 | 拨码开关 | 120 Ω，**出厂默认 ON** |

注意事项：

1. **必须共地**。模块 CAN 口不提供地，靠 Type-C 与 PC 共地；若主板由独立电池供电，
   需额外把 P7 的 INGND 与模块/PC 的地连在一起，否则通信不稳定或直接不通。
2. **终端电阻**：模块默认已接 120 Ω。短距离台架（<1 m）单端 120 Ω 可用；
   若总线较长或加挂第二个节点，请在总线另一端再并 120 Ω。
3. 支线长度控制在 0.3 m 内，CAN_H/CAN_L 使用双绞线。

## 2. 模块固件模式（必须先确认）

官方模块出厂固件使用达妙自有协议，**不能被 python-can 直接驱动**。本工程上位机使用
`python-can`，需要模块运行 SLCAN 或 gs_usb 固件（官方仓库 `模块/通信模块/USB2CANFD/固件/`
提供 `socketcan/slcan固件` 与 `socketcan/gsusb免驱固件` 及刷写说明 `FLASHING.md`）。

| 模式 | PC 侧接口 | 特点 |
|---|---|---|
| SLCAN | `--can-interface slcan --can-channel COM3` | 免驱，Windows 最省事 |
| gs_usb | `--can-interface gs_usb --can-channel 0` | 需装官方 gs_usb 驱动（`SDK/模块通信SDK/gs_usb_drives`） |
| 出厂固件 | 不支持 | 仅达妙上位机可用 |

## 3. 波特率

官方说明书给出的配置范围：仲裁域最大 1 Mbps（1M/500k/250k/200k/125k/100k），
数据域最大 5 Mbps（FDCAN）。本工程使用**经典 CAN，1 Mbps**，两端必须一致：

- MCU：`BOARD_CAN_BITRATE 1000000`，80 MHz 内核时钟 / 5 = 16 MHz，16 tq/位。
- 模块：在与模块配套的上位机里把仲裁域设为 1 Mbps（若使用 SLCAN 固件，则由
  `python-can` 的 `bitrate=1000000` 在打开总线时下发）。

## 4. 指示灯

| 指示 | 含义 |
|---|---|
| 蓝色慢闪 | 未连接上位机 |
| 绿色慢闪 | 已连接上位机 |
| 绿色快闪 | CAN 总线被动错误（多为波特率不一致/接线问题） |
| 红色快闪 | CAN 总线关闭（bus-off，检查终端电阻与共地） |

## 5. 帧映射

| 方向 | 11 位标识符 | 载荷 |
|---|---|---|
| MCU→PC | `0x601` | `data[0]` 段序号，`data[1..7]` 应用数据 |
| PC→MCU | `0x600` | 同上 |

应用层仍是 V1.0 协议帧（`A5 5A` 头 + CRC16），一条状态帧共 45 字节，拆成 7 个 CAN 帧（前 6 帧各 7 字节，末帧 3 字节）。
详见 `docs/PROTOCOL.md` 的「CAN 承载」章节。

## 6. 联调步骤

```powershell
# 1) 安装依赖
pip install python-can

# 2) 看模块枚举到的接口（SLCAN 会列出 COM 口）
python C:\Users\33515\Desktop\rm_v_1_0\soft1\development\PC_Debug\monitor.py --list-can

# 3) 读取 20 帧状态
python C:\Users\33515\Desktop\rm_v_1_0\soft1\development\PC_Debug\monitor.py `
    --can --can-interface slcan --can-channel COM3 --can-bitrate 1000000 --samples 20
```

判据：能打印 JSON 状态行（含 `uptime_s`、`state`、`bus_mv`、`good_frames`）即链路正常。

## 7. 故障排查

| 现象 | 先查什么 |
|---|---|
| 模块蓝灯慢闪 | 上位机没连上：换 Type-C 线/口，确认驱动 |
| 绿灯快闪 | 波特率不一致，或 CAN_H/L 接反 |
| 红灯快闪 | bus-off：终端电阻缺失/过多、未共地、线太长 |
| 灯正常但无数据 | MCU 未上电、`CHEN_FEATURE_PC_CAN=0`、滤波器 ID 不匹配（应为 0x600） |
| 偶发丢帧 | 支线过长、未双绞、地电位差；缩短支线并单点接地 |
