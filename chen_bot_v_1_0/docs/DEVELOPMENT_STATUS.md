# 开发状态

日期：2026-11-27　基线：`chen_bot_v_1_0`（新建，独立于 `Firmware/` 旧版本）

## 已完成

| 范围 | 状态 | 证据 |
|---|---|---|
| 分层目录 | 完成 | `Firmware/{Config,BSP,Device/Real,Device/Mock,Control,Service,Debug,APP,HMI,Test}` |
| 统一设备抽象 | 完成 | `Device/devices.h`，10 类设备回调 + `Transport` |
| 安全状态机与控制权 | 完成 | `Control/safety.c`，主机用例覆盖使能/超时/锁存/清除 |
| 参数集中管理 | 完成 | `Config/params.c`，13 个参数、三分类、CRC、耦合校验 |
| 通信协议 v1.0 | 完成 | 帧格式与既有向量兼容，命令集扩展到 12 条 |
| 统一状态快照 | 完成 | `Service/snapshot.c`，36 字节线上编码 |
| 调试服务与权限矩阵 | 完成 | `Debug/debug_service.c` |
| FreeRTOS 任务 | 完成 | 5 个任务，静态分配，周期见 `FREERTOS_TASKS.md` |
| LCD + 五向键 HMI | 完成（软件） | 六页界面、局部刷新、最近档位判键 |
| SIM/BENCH/RUN | 完成 | 三配置均链接成功 |
| 主机自动化测试 | 完成 | 9 组用例全部通过 |
| Keil 工程 | 完成 | `MDK-ARM/chen_bot_v_1_0.uvprojx`，0 错误 0 警告 |

## 进行中 / 下一步

1. **实物点亮**：烧录 RUN/SIM 镜像，确认 LCD 六页、按键方向与档位。
2. **按键实测**：记录五档实际 mV，写入 `docs/DEBUG_GUIDE.md` 标定表
   （当前按模块原理图归类，已留 ±15% 容差）。
3. **BLE 链路**：取得 ATK-BLE02 手册后在 `BSP/uart_link.c` 上对接，
   并用 `MSG_STATUS` 验证端到端时延。
4. **动力启用**：电气验收后置 `CHEN_FEATURE_ACTUATION=1`，先空载单轮，
   再按 `ACCEPTANCE.md` 逐项测试。
5. **参数持久化**：`CHEN_FEATURE_PARAM_NVM` 目前为 0，接口已在
   `Config/params.c` 留出 CRC 与版本字段。

## 阻塞项

| 阻塞 | 影响模块 | 需要 |
|---|---|---|
| 电池/触点分压未标定 | `BSP/board.c` 电压换算 | 万用表实测两点 |
| 编码器未安装 | `Control/pid.c` 闭环 | 硬件到位 |
| 电流检测未安装 | 快照/故障 | 硬件到位 |
| 继电器触点反馈未接线 | 故障页 | 硬件到位 |
| IWDG 未启用 | 复位保护 | 需先确认停机调试策略 |

## 与旧版本的关系

- `Firmware/`（LCD_SMOKE 台架）保持不变，仍可独立构建与烧录。
- 新框架按 V1.2 分层与命名重写，复用其已验证的协议 CRC、HMI 渲染与按键判据。
- 两套工程互不引用，避免重构影响在用的台架固件。
