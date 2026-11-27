# 双模型协作记录

> 唯一共享上下文文件（COOPERATE.md 第 3 条）。只记录阶段、结论、待办与验证结果，
> 不记录讨论过程。代码历史以 Git 为准。

## 当前阶段

阶段 2（核心设备驱动）收尾 + 通信链路迁移（USART1 → USB2CANFD）。
主开发：DeepSeek；待审查：架构/安全/通信（节点 1–2 合并审查）。

## 已完成并验证

| 项 | 证据 |
|---|---|
| `chen_bot_v_1_0` 分层框架（Config/BSP/Device/Control/Service/Debug/APP/HMI/Test） | Keil 0 错误 0 警告，Code 50360 |
| 协议 v1.0（帧/CRC/分包/权限矩阵） | 主机测试 9 组通过 |
| 安全状态机、控制权、参数块 | 主机测试覆盖使能/超时/锁存/清除 |
| HMI 六页 + 五向键解码 | 主机仿真验证换行与局部刷新 |
| 台架 LCD_SMOKE 按键判定修复 | 已烧录，翻页正常；上下键曾失效，改最近档位后待确认 |
| 全部工程调试器统一 ST-Link V2 | 5 个 `.uvoptx` 均为 `nTsel=5` |
| Git 仓库 + GitHub（公开） | `chn1608/rm_v_1_0`，`main`/`development/v1.2` = `d761b11` |

## 本轮定位到的缺陷（均已修）

1. **ADC 扫描模式误用**：`ADC_SCAN_ENABLE` + `ADC_EOC_SEQ_CONV` + 三次轮询 →
   首轮读到末通道，后续轮询无 EOC 超时返回失败，按键与电池全废。
   改为单通道单次转换（`ADC_SCAN_DISABLE` + `ADC_EOC_SINGLE_CONV`）。
2. **ADC 跨任务并发**：控制任务（5 ms，优先级 5）与 HMI 任务（20 ms，优先级 1）
   同时对同一 ADC 做「配置通道→启动→轮询→停止」。已改为控制任务独占采样，
   经 `Snapshot.key_mv/key_valid` 发布，HMI 只读快照。
3. **`configMAX_PRIORITIES = 5` 小于控制任务优先级 5**：FreeRTOS 会把优先级钳位到
   4，导致控制与通信同级、时间片轮转，破坏"安全优先"的调度前提。改为 8。
4. **任务栈偏小**：HMI 2 KB（含 344 B 快照 + 渲染调用链），叠加
   `configCHECK_FOR_STACK_OVERFLOW=2` 与 `configASSERT→board_fatal()`，
   溢出即整机停机（屏幕定格）。已扩为 HMI 4 KB / 控制 2 KB / 通信 2 KB。
5. **任务创建未全部检查**：原先只检查控制任务，其余失败会带病启动；现已全部校验。

## 待解决

| 问题 | 现状 | 下一步 |
|---|---|---|
| 按键无响应 | 已消除上述 5 项缺陷并加入开机色块自检；等现场回报屏幕最终颜色定位停机阶段 | 颜色→阶段映射见 `README`/下方 |
| CAN 迁移 | `BSP/can_link.c` 已写（FDCAN1 经典帧 1 Mbit，0x601/0x600 分段），尚未接入 app | 接入 `app.pc`、更新 Keil/构建列表、PC 上位机改用 python-can |
| 五向键电压实测 | 分档按模块原理图，未实测 | 现场记录五档 mV 写入 `docs/DEBUG_GUIDE.md` |

## 开机色块自检（临时，`main.c`）

| 屏幕最终颜色 | 含义 |
|---|---|
| 蓝 `#001F` | LCD 已初始化，死在模拟前端之前 |
| 绿 `#07E0` | 模拟前端 OK，死在调度器之前 |
| 红 `#F800` | 模拟前端失败，死在调度器之前 |
| 黄 `#FFE0` | 已进入调度器，HMI 未成功绘制 |
| 正常界面 | 系统运行 |

## 待确认硬件

- CAN1：MC02 板上 `PD0(RX)/PD1(TX)`，板载收发器 + PESD1CAN；对外接口针序待核。
- USB2CANFD：出厂固件 / gs_usb 免驱 / slcan 三种模式，需确认当前模块烧的是哪一种。
- 五向键五档实测电压（0/0.66/1.32/1.98/2.64 V 为原理图值）。

## 编译与测试结果（本轮）

```
Keil µVision 5.24a  -r MDK-ARM/chen_bot_v_1_0.uvprojx
Code=50360  RO-data=10496  RW-data=88  ZI-data=22544
0 Error(s), 0 Warning(s)
```

## Git

本地与远端均 `development/v1.2`；本轮改动尚未提交（等按键问题定性后一并提交，
避免把调试代码混入历史）。
## 按键无响应：根因（2026-11-27，独立审查代理确认）

**根因**：ADC_ChannelConfTypeDef.Channel 传入的是裸十进制（4/3/19），而 HAL 把该字段
原样交给 LL，LL 用 ADC_CFGR_AWD1CH（bit26..30）反解通道号，三个值全部解出 0 →
PCSEL 只置 bit0、SQR1.SQ1=0 → 三路转换的都是 ADC1_INP0（PA0_C），PA0 模拟开关默认断开，
恒读约 0.2 V。旁证：现场 key 0.236 V 与 battery 2.6 V 正好差 11 倍分压比，说明是**同一次 raw**。

第二后果：采样时间按裸号码写入 SMPR1.SMP0 而非 SMPR2.SMP19，通道 19 实际仅 1.5 周期。

**修复**：三处通道赋值改为 __LL_ADC_DECIMAL_NB_TO_CHANNEL(nb)；撤掉期间误加的 PA5 内部上拉
（会改变 100K 梯形分压）。台架版 panel.c 用的是 ADC_CHANNEL_19 编码字，故当时可用。

**过程记录（供后续避免重复）**：先后误判为「扫描/EOC 模式错误」「ADC 跨任务并发」
「HMI 栈溢出」「PA5 悬空」，均被现场数据否定；最终由逐行比对 HAL/LL 源码定位。
教训：外设通道/句柄字段的单位与编码，必须对照 HAL 头文件确认，而不是按数值直传。

## 审查来源说明

本环境没有可用的 CODEX / GPT CLI，按 COOPERATE.md 第 6 条采用**独立子代理**做交叉审查，
结论与证据（文件:行号）已并入本节；未虚构外部模型调用。

## 按键逻辑整改（以 gitee 官方资料为准）

依据：`gitee.com/kit-miao/damiao` → 子模块 `kit-miao/dm-mc02` →
`例程/CtrBoard-H7_KEY/Core/Src/adc.c`、`例程/CtrBoard-H7_KEY/User/adc_modlue.c`。

官方做法与本次对齐：

| 项 | 官方 | 本工程 |
|---|---|---|
| ADC 通道 | `ADC_CHANNEL_4` / `ADC_CHANNEL_19`（编码字） | 已改用 `__LL_ADC_DECIMAL_NB_TO_CHANNEL()` |
| 转换方式 | 扫描 + `ADC_EOC_SINGLE_CONV` + DMA 循环连续 | 扫描 + 单次轮询（3 rank，含急停），保留轮询 |
| 采样时间 | `ADC_SAMPLETIME_32CYCLES_5` | 已对齐 |
| 判键 | RAW 窗口：0–200 中、700–1000 右、1500–1800 左、2200–2500 上、2800–3500 下，其余松开 | 已照搬 |
| 左右方向 | **右=0.66 V 档，左=1.32 V 档**（与模块原理图标注相反） | 以官方为准 |
| 消抖 | 10 ms `hold_filter` | 40 ms 稳定窗 + 40 ms 事件屏蔽（应对 100K/100nF 释放斜坡） |

**新发现的方向性矛盾**：模块原理图（DM_TFT-V1.1.pdf 表格）标 0.66 V=左、1.32 V=右，
而官方例程 `adc_modlue.c` 判 700–1000=右、1500–1800=左。本工程采用官方例程。
若现场手感与此相反，只需交换 `BOARD_KEY_WIN_LEFT_*` 与 `BOARD_KEY_WIN_RIGHT_*` 两组窗口。

**操作逻辑整改**：

1. 按键解码从 HMI 任务（20 ms，受渲染抖动影响）移到控制任务（5 ms），消抖结果通过 4 深事件环
   交给 HMI，渲染再慢也不会改变判键时序。
2. 增加 40 ms 事件屏蔽窗：松开按键时 100K+100nF 的电压斜坡会扫过相邻档位，
   旧实现会把斜坡判成"多按了一次"（现象为：中键返回后自动下移一格、上键上移后自动回位）。
3. 判键改为官方 RAW 窗口，窗口之间是"松开"区，斜坡经过间隙时不再产生伪按键。

## 诊断代码清理

已移除全部临时诊断：`main.c` 的开机色块（蓝/绿/红/黄）、控制与 HMI 任务入口色块、
`CHEN_DIAG_KEYS` 覆盖层及其编译宏、工程 Define 中的 `CHEN_DIAG_KEYS=1`。
当前工程不再有任何只为调试存在的显示或分支。

## CAN 施工进度

| 项 | 状态 | 位置 |
|---|---|---|
| MCU 侧 FDCAN1 传输层 | 已实现，已编译 | `Firmware/BSP/can_link.c`（0x601/0x600，7 字节分段） |
| 应用接入 | 已完成 | `app.c` 的 `app.pc` 改走 `board_can_transport()` |
| PC 侧 CAN 传输 | 已实现，单测通过 | `soft1/development/PC_Debug/can_link.py` + `test_can_link.py` |
| PC 上位机 | 已改为 CAN | `monitor.py --can --can-interface slcan|gs_usb` |
| 协议文档 | 已补 CAN 承载章节 | `docs/PROTOCOL.md` |
| 引脚与资源 | 已补 CAN1/CAN2/CAN3 | `docs/RESOURCE_MAP.md` |
| 接线图 | 已把 USB-TTL 换成 USB2CANFD | `docs/wiring_chen_bot_v1_0.pdf` |

## 本轮编译与测试

```
Keil µVision 5.24a  -r MDK-ARM/chen_bot_v_1_0.uvprojx
Code=54956  RO-data=10576  RW-data=132  ZI-data=22948
0 Error(s), 0 Warning(s)

python Tools/build.py host  ->  9 组用例全部通过
烧录：Erase Done / Programming Done / Verify OK  18:29:46
```
