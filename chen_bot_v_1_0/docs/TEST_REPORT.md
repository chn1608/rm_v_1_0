# 构建与测试记录

日期：2026-11-27　工程：`chen_bot_v_1_0`

## 已完成

| 项目 | 命令 | 结果 |
|---|---|---|
| 主机单元测试 | `python Tools/build.py host` | 通过，`-Wall -Wextra -Werror -pedantic` 零告警 |
| ARM SIM | `python Tools/build.py arm --profile SIM` | 链接成功 |
| ARM BENCH | `python Tools/build.py arm --profile BENCH` | 链接成功 |
| ARM RUN | `python Tools/build.py arm --profile RUN` | 链接成功 |
| Keil µVision 5.24a | `UV4 -r MDK-ARM/chen_bot_v_1_0.uvprojx` | **0 错误 0 警告** |

Keil 工程尺寸（`CHEN_PROFILE=2`，`CHEN_FEATURE_ACTUATION=0`）：

```
Code=50040  RO-data=10496  RW-data=88  ZI-data=18112
```

占 Flash 1 MB 的 5.9%，DTCM 128 KB 的 14.2%。

## 调试器约定

两个工程的调试器统一为 **ST-Link V2**（`nTsel=5`，驱动
`STLink\ST-LINKIII-KEIL_SWO.dll`），并在 `TargetDriverDllRegistry` 中携带
`STM32H72x-73x_1024.FLM` 烧录算法（0x08000000，1 MB，算法 RAM 0x20000000）。
由 `Tools/set_stlink_debugger.py` 保证幂等，重新生成工程后可再执行一次。

## 主机测试覆盖

| 模块 | 用例 |
|---|---|
| 协议 | 编码字节序、CRC、逐字节重组、前置噪声、CRC 损坏拒收、连续两帧、截断超时 |
| 参数 | 默认值、越界拒绝、分类（运行期 vs 安全/标定需停机）、耦合限位、CRC 损坏检测、复位 |
| 安全 | 自检→待机、无本地确认拒绝使能、session 递增、重复/过期序号丢弃、越界拒绝、错 session 拒绝、链路丢失锁存、清除后必须重新使能、运动超时 |
| 底盘 | 直行斜坡、转向比例、限幅、换向必须先过零并保持、无许可强制归零 |
| 舵机 | 标定有效性、角度→脉宽、限幅、速率限制、非法标定拒绝 |
| PID | 比例跟踪、输出饱和与抗积分饱和、非法配置拒绝 |
| 按键 | 五档判定、±10% 增益偏差仍可分辨、消抖 3 次、释放事件 |
| 快照 | 36 字节状态帧字段位置与符号扩展 |
| 调试服务 | 状态/版本/参数读写权限、PC 不可运动、BLE 无本地确认不可使能、错控制权拒绝、停机能被所有者接受 |

## 未执行（必须如实标注）

| 项目 | 原因 |
|---|---|
| 实机烧录与运行 | 烧录会启动真实动力路径，需人工在场 |
| LCD 画面与五向键 | 需实物确认（按键电压已按模块原理图归类，实际读数待测） |
| BLE 实机链路 | 缺 ATK-BLE02 手册与实际模块 |
| 电机/舵机带载 | 未完成电气安全验收，`CHEN_FEATURE_ACTUATION=0` |
| ADC 标定 | 电池分压与触点分压未实测 |
| 栈/CPU 峰值 | 需在实物上用高水位与跟踪测量 |

## 复现步骤

```bash
cd chen_bot_v_1_0
python Tools/build.py host
python Tools/build.py arm --profile all
```

Keil：打开 `MDK-ARM/chen_bot_v_1_0.uvprojx`，目标 `CHEN_BOT`，F7。
烧录前确认 `Options → Debug → Settings → Flash Download` 中有
`STM32H72x-73x 1 MB`（工程已内置，地址 0x08000000）。
