# chen_bot_v_1_0

STM32H723VGT6 机器人电控程序框架，按《V_1.2 工程开发总任务》分层实现。
本目录是独立可编译、可测试、可继续开发的正式工程，不依赖 `Firmware/` 下的旧版本。

## 目录

```
chen_bot_v_1_0/
├── Firmware/
│   ├── main.c              启动顺序：安全电平 → 时钟 → 总线 → 设备表 → 应用
│   ├── Config/             profile（SIM/BENCH/RUN）、功能开关、板级常量、参数块
│   ├── BSP/                时钟、PWM、ADC、LCD、双 UART —— 唯一接触寄存器的一层
│   ├── Device/             统一设备抽象；Real/ 真实适配，Mock/ 仿真
│   ├── Control/            安全状态机、底盘整形、舵机整形、PID（预留）
│   ├── Service/            通信协议、故障位、统一状态快照
│   ├── Debug/              统一调试服务（权限矩阵、参数读写、诊断）
│   ├── APP/                应用装配与 FreeRTOS 任务
│   └── HMI/                LCD 六页界面、字体与五向键渲染
├── Tests/                  主机单元测试（独立于固件，不参与编译）
├── MDK-ARM/                Keil 工程（目标名 CHEN_BOT）
├── Tools/                  build.py、keil_project.py、wiring_diagram.py、set_stlink_debugger.py
└── docs/                   架构、任务、协议、资源、测试、验收
```

## 构建

```bash
# 主机单元测试（GCC，-Werror -pedantic）
python Tools/build.py host

# ARM 三配置（Arm Compiler 5，默认 D:/keil5/ARM/ARMCC/bin）
python Tools/build.py arm --profile all
```

产物：`build/chen_bot_v_1_0/arm/<PROFILE>/chen_bot_v_1_0.axf`。

Keil：打开 `MDK-ARM/chen_bot_v_1_0.uvprojx`，目标 `CHEN_BOT`，F7 构建、F8 下载。
工程内已包含 ST-Link 烧录算法（`STM32H72x-73x_1024.FLM`，0x08000000，1 MB）。

## 三种配置

| 配置 | 用途 | 设备 | 真实动力 |
|---|---|---|---|
| SIM | 主机/无动力仿真 | Mock | 编译期禁止 |
| BENCH | 单模块台架 | Real | 默认禁止，需 `CHEN_FEATURE_ACTUATION=1` 且现场授权 |
| RUN | 整车 | Real | 同上，另需互锁与电压窗口满足 |

`CHEN_REAL_OUTPUTS` 由 `CHEN_FEATURE_ACTUATION` 派生；为 0 时 `safety_arm()` 在编译期就不存在成功路径。

## 安全基线

- 上电先拉安全电平（电机方向脚低、STBY 低、继电器断开），再初始化 HAL。
- 单控制权：`safety_arm()` 发放 session，过期/重复序号丢弃，超时 200 ms 自动断开。
- 故障锁存：自检、互锁、链路、超时、器件、急停、电源、参数、通信、看门狗。
- 清除故障只回到 STANDBY，必须重新授权，绝不恢复历史运动指令。
- 未安装的传感器一律 `DATA_ABSENT`，不参与"自检通过"。

## 当前证据等级

- 已编译：主机测试 + ARM 三配置，Keil 0 错误 0 警告。
- 已通过主机测试：协议、参数、安全、底盘、舵机、PID、按键、快照、调试服务。
- 未执行：实机烧录、LCD 画面、BLE 实机链路、电机/舵机带载、ADC 标定。
  详见 `docs/TEST_REPORT.md` 与 `docs/ACCEPTANCE.md`。

## 来源与范围

本工程的部分模块继承自上一轮开发会话，部分文件派生于达妙官方工程，逐项说明见
[`docs/PROVENANCE.md`](docs/PROVENANCE.md)。要点：

- `Firmware/HMI/*`、`Firmware/BSP/lcd.c`：继承自上一轮会话；
- `Firmware/BSP/timebase.c`、`Firmware/Config/FreeRTOSConfig.h`、`BSP/layout.sct`、
  `MDK-ARM/config/stm32h7xx_hal_conf.h`：厂商派生，保留原版权头；
- 厂商 HAL/CMSIS/FreeRTOS 源码与 RM2021_Hero 参考工程**不在本仓库**，仅本地引用。

仓库已公开。第三方资料（厂商 HAL/CMSIS/FreeRTOS 源码、RM2021_Hero 参考工程）不在仓库内，
克隆后需另行取得；其余为本项目自身的开发产物。

## 当前状态（2026-11-28）

| 项 | 状态 |
|---|---|
| 编译 | Keil 0 错误 0 警告（CAN 配置，Code 55516） |
| LCD + 五向键 | **实物已验证可用**（判键按官方 RAW 窗口，左右方向以官方例程为准） |
| 上位机串口 | 已完全删除（`CHEN_FEATURE_PC_DEBUG` 移除，USART1 不再初始化） |
| 上位机 CAN | 固件侧就绪（FDCAN1、1 Mbit、0x600/0x601、消息 RAM 已配、bus-off 自动恢复、内部回环自检） |
| PC 上位机 | `soft1/development/PC_Debug/gui.py`（tkinter，单实例）与 `monitor.py`（命令行） |
| CAN 实机联调 | 适配器回 0x02（总线无 ACK），尚未通 |
| 动力 | `CHEN_FEATURE_ACTUATION = 0`，未启用 |

### 已知问题

- **严重**：TB6612FNG 的 VM 绝对最大 15 V，4S 满电 16.8 V 超压，必须加 12 V 降压。
- **待定**：UBEC-8A 输出档位（必须 5 V 才能接主板）、分电需自制线束。
- **待定**：CAN 链路（适配器需 CAN2.0 + 1 Mbps）、BLE 模块、编码器、急停与独立断能。

接线图与状态分级见 `docs/wiring_chen_bot_v1_0.pdf`（绿=已确认 / 黄=待定 / 红=严重）。
