# 架构

## 分层与依赖方向

```
APP (tasks)  ──►  Control / Service / Debug  ──►  Device (abstraction)  ──►  BSP  ──►  HAL
   │                     │                            ▲
   └──► HMI ─────────────┘                            └── Device/Mock（SIM）
```

规则：

1. 只有 `BSP/` 和 `Device/Real/` 允许出现 `HAL_*`、寄存器与引脚号。
2. `Control/`、`Service/`、`Debug/`、`Config/params.c`、`Device/keys.c` 不依赖 HAL，
   因此可以在主机上编译并测试（见 `Tools/build.py` 的 `PURE` 列表）。
3. `APP/` 是唯一持有 `Devices` 表、`Safety`、`Params`、`Snapshot` 实例的地方。
4. `HMI/` 只消费 `HmiSnapshot`，不直接操作任何设备。

## 模块职责

| 模块 | 职责 | 不做什么 |
|---|---|---|
| `Config/profile.h` | 配置选择、周期、栈、优先级 | 不放业务逻辑 |
| `Config/features.h` | 硬件功能开关、`CHEN_REAL_OUTPUTS` | 不代表安全验收通过 |
| `Config/board_config.h` | 引脚、分压系数、限值、舵机标定 | 不放运行期可变参数 |
| `Config/params.c` | 参数表、范围检查、CRC、分类 | 不做 NVM 擦写（接口预留） |
| `Device/devices.h` | 电机/舵机/电源/显示/按键/电压/电流/转速/位置/触点 | 不做限幅与状态机 |
| `Device/Real` | 把设备表映射到 BSP | 不做策略 |
| `Device/Mock` | 同接口仿真，数据标 `DATA_SIMULATED` | 不冒充真实测量 |
| `Control/safety.c` | 状态机、控制权、超时、故障锁存 | 不直接写引脚 |
| `Control/chassis.c` | 左右差速混合、限幅、斜坡、换向保护 | 不做通信 |
| `Control/servo_ctrl.c` | 角度↔脉宽、限位、速率限制 | 不假设有位置反馈 |
| `Control/pid.c` | 整数 PID，编码器闭环预留 | 未接编码器前不参与输出 |
| `Service/protocol.c` | 帧编解码、CRC16、分包粘包、超时 | 不做权限判断 |
| `Service/faults.c` | 故障位名称与描述 | 不决定何时置位 |
| `Service/snapshot.c` | 统一状态结构与线上编码 | 不做显示 |
| `Debug/debug_service.c` | 命令分发、权限矩阵、参数读写 | 不绕过安全状态机 |
| `HMI/` | 页面、字体、局部刷新 | 不访问设备 |
| `APP/app.c` | 任务、装配、快照发布 | 不做底层时序 |

## 数据流

1. **上行**：`control_task` 采集电压/按键/链路状态 → 更新 `Snapshot`（临界区一次拷贝）
   → HMI、遥测、调试服务读取同一份快照。
2. **下行**：BLE/PC 帧 → `protocol_feed` → `debug_handle` → `safety_motion/servo/arm`
   → `control_task` 整形 → `Devices.motor/servo` → BSP。
3. **安全**：任何故障位 → `safety_trip` → 清 owner、清指令、关动力；`control_task`
   每个周期都会把"未授权"翻译成 0 输出，而不是依赖故障任务去关。

## 权限矩阵

| 来源 | 状态/诊断 | 参数读 | 参数写 | 使能/运动 | 舵机 |
|---|---|---|---|---|---|
| PC (`OWNER_PC`) | ✔ | ✔ | 仅维护态 | ✘ | ✘ |
| BLE (`OWNER_BLE`) | ✔ | ✔ | ✘ | ✔（受安全机约束） | ✔ |
| 本地 (`OWNER_LOCAL`) | ✔ | ✔ | 仅维护态 | ✔（可满足本地确认） | ✔ |

## 关键设计决策

- **协议字节级不变**：沿用 `Protocol/vectors.txt` 的帧格式与 CRC，PC/Android 无需改动即可互操作。
- **参数分类**：`PARAM_RUNTIME` 可在线改；`PARAM_SAFETY`/`PARAM_CAL` 必须在停机态改。
- **编译期去能**：`CHEN_REAL_OUTPUTS=0` 时 `safety_arm()` 直接返回 false，避免"靠运行时判断"漏掉。
- **不伪造反馈**：未安装的编码器、电流、触点一律 `DATA_ABSENT`/`DEV_UNAVAILABLE`。
- **单写者**：`Snapshot` 只由 `control_task` 写，其他任务读，避免跨任务撕裂。

## 资源分配

见 `docs/RESOURCE_MAP.md`（引脚、外设、DMA、中断、余量）。
