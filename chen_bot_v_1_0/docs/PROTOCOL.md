# 通信协议 v1.0

三端共用：STM32（本工程）、PC 调试工具、Android 遥控。线格式与仓库中
`soft1/development/Protocol/vectors.txt` 的既有测试向量保持字节级一致。

## 帧格式

| 偏移 | 长度 | 字段 | 说明 |
|---:|---:|---|---|
| 0 | 2 | 帧头 | `0xA5 0x5A` |
| 2 | 1 | 版本 | 当前 `0x10` |
| 3 | 1 | 命令 | 请求 `0x01..0x7F`，应答置 bit7 |
| 4 | 1 | 载荷长度 | 0..64 |
| 5 | 2 | 序列号 | 小端，请求方自增 |
| 7 | n | 载荷 | 见下表 |
| 7+n | 2 | CRC | CRC-16/CCITT-FALSE，覆盖偏移 2..6+n |

- CRC 参数：poly `0x1021`，init `0xFFFF`，不反转，无最终异或。
- 字节序：全部小端；有符号量为二进制补码。
- 最大帧长 73 字节；帧间超时 100 ms（`CHEN_FRAME_TIMEOUT_MS`）。
- 解析器必须能处理分包、粘包、噪声、截断、非法长度；噪声丢弃不计错，
  头部合法但 CRC/版本错误的帧计入 `bad`。

## 命令

| ID | 名称 | 方向 | 载荷 |
|---|---|---|---|
| 0x01 | PING | 请求 | 空 |
| 0x02 | STATUS | 请求 | 空 |
| 0x03 | PARAM_GET | 请求 | `id:u8` |
| 0x04 | PARAM_SET | 请求 | `id:u8, value:i32` |
| 0x05 | STOP | 请求 | 空 |
| 0x06 | ARM | 请求 | 空 |
| 0x07 | DISARM | 请求 | 空 |
| 0x08 | MOTION | 请求 | `left:i16, right:i16`（‰） |
| 0x09 | SERVO | 请求 | `pulse_us:u16` |
| 0x0A | HEALTH | 请求 | 空 |
| 0x0B | VERSION | 请求 | 空 |
| 0x0C | FAULTS | 请求 | 空 |
| 0x82 | STATUS_REPLY | 应答 | 36 字节，见下 |
| 0x83 | PARAM_REPLY | 应答 | `id:u8, value:i32, min:i32, max:i32, revision:u32, class:u32` |
| 0x88 | MOTION_REPLY | 应答 | `left:i16, right:i16`（实际接受值） |
| 0x8A | HEALTH_REPLY | 应答 | 16 字节 |
| 0x8B | VERSION_REPLY | 应答 | `major:u8, minor:u8, patch:u8, profile:u8, protocol:u8, param_rev:u32` |
| 0x8C | FAULTS_REPLY | 应答 | `faults:u32, arm_count:u32` |
| 0x90 | ACK | 应答 | `code:u8=0, detail:u32` |
| 0x91 | NAK | 应答 | `code:u8, detail:u32` |

NAK 原因码：1 拒绝、2 超范围、3 未知命令/参数、4 长度错误、5 未使能、6 状态不允许。

## STATUS_REPLY 布局（36 字节）

| 偏移 | 类型 | 含义 |
|---:|---|---|
| 0 | u8 | 载荷布局版本 = 1 |
| 1 | u8 | 安全状态（0 boot … 6 estop） |
| 2 | u8 | 控制权（0 none, 1 local, 2 ble, 3 pc） |
| 3 | u8 | 标志位：bit0 运动允许、bit1 电压有效、bit2 电流有效、bit3 动力指令、bit4 BLE、bit5 PC、bit6/7 触点 |
| 4,6 | i16 | 左/右指令（‰） |
| 8,10 | i16 | 左/右实际输出（‰） |
| 12 | u16 | 舵机指令脉宽 us |
| 14 | i16 | 舵机指令角度（0.1°） |
| 16 | u16 | 电池电压 mV（无效时 0） |
| 18 | u16 | 电流 mA |
| 20 | u32 | 故障位 |
| 24 | u32 | 运行时间 s |
| 28 | u16 | 有效帧计数（饱和） |
| 30 | u16 | 错误帧计数（饱和） |
| 32 | u16 | 控制循环最大耗时 us（饱和） |
| 34 | u8 | 电压来源（`DataSource`） |
| 35 | u8 | 电流来源 |

## 权限

| 来源 | 读状态/参数 | 写参数 | ARM/MOTION/SERVO |
|---|---|---|---|
| PC | ✔ | 仅维护态 | ✘ |
| BLE | ✔ | ✘ | ✔（受安全机约束） |
| 本地 | ✔ | 仅维护态 | ✔ |

- `ARM` 是否接受本地确认由 `PARAM_ARM_REQUIRE_LOCAL` 决定，默认 1：远程无法自使能。
- `MOTION` 的序列号取自帧头 `sequence`，必须严格前进；重复/回绕按半程比较丢弃。
- 运动指令有效期 200 ms（`PARAM_MOTION_TIMEOUT_MS`），超时置 `FAULT_TIMEOUT` 并断开控制权。
- 心跳不得延长过期指令：只有 `MOTION`/`SERVO` 刷新 `last_motion_ms`。

## 兼容性

改动帧格式或 CRC 必须同步更新：本文档、`Protocol/vectors.txt`、PC 的 Python 解析、
Android 的 Kotlin 解析，以及 `Firmware/Test/test_core.c` 中的向量断言。

## CAN 承载（v1.1，替代 USART1 USB 转串口）

PC 状态链路改用达妙 USB2CANFD 模块接在 MC02 的 CAN1（PD0/PD1，板载收发器）。
应用层帧格式不变，只改为经 CAN 分段传输，因此 PC 与 Android 的解析代码可复用。

| 项目 | 值 |
|---|---|
| 总线 | CAN1，经典 CAN 2.0，1 Mbit/s |
| MCU→PC 标识符 | `0x601`（标准 11 位） |
| PC→MCU 标识符 | `0x600` |
| 每帧载荷 | `data[0]` = 段序号（从 0 起），`data[1..7]` = 最多 7 字节应用数据 |
| 重组 | 收到前 5 字节后由应用帧 byte4 得到总长 len+9，凑满即交付 |
| 丢段处理 | 序号不连续则丢弃本次重组并计数（board_can_errors()） |

一条状态帧共 45 字节（36 字节载荷 + 9 字节帧头尾），需要 7 个 CAN 帧；1 Mbit/s 下约 1.0 ms，占 50 ms 周期的 2%。

实现位置：

- MCU：`Firmware/BSP/can_link.c`（FDCAN1，滤波器精确匹配 0x600，RX FIFO0 中断只搬运）
- PC：`soft1/development/PC_Debug/can_link.py`（CanLink，含分段/重组与 FakeBus 测试替身）

模块固件模式（官方资料 `模块/通信模块/USB2CANFD`）：

| 模式 | PC 侧接口 | 说明 |
|---|---|---|
| SLCAN 固件 | python-can `interface="slcan"`，串口 COMx | 免驱动，Windows 最省事 |
| gs_usb 固件 | python-can `interface="gs_usb"`，channel 0 | 需安装达妙 gs_usb 驱动 |
| 出厂固件 | 需达妙上位机协议 | 本工程不使用 |

PC 端示例：

```
python monitor.py --can --can-interface slcan --can-channel COM3 --can-bitrate 1000000
```
