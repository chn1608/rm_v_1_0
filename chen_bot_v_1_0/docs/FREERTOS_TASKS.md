# FreeRTOS 任务与调度

## 任务表

| 任务 | 周期 | 优先级 | 栈（字） | 触发 | 职责 |
|---|---|---:|---:|---|---|
| `control` | 5 ms | 5 | 384 | 周期 | 安全步进、底盘/舵机整形、执行器输出、快照发布 |
| `comm` | 5 ms | 4 | 320 | 周期（轮询） | PC/BLE 收字节、协议解析、调试服务分发、应答 |
| `telemetry` | 50 ms（`PARAM_TELEMETRY_MS`） | 2 | 256 | 周期 | 向 PC 发送状态帧 |
| `hmi` | 20 ms | 1 | 512 | 周期 | 五向键采样消抖、每周期绘制一个条带 |
| `diag` | 1000 ms | 1 | 256 | 周期 | 运行时间、循环耗时、健康计数 |
| `IDLE` | — | 0 | `configMINIMAL_STACK_SIZE` | 调度器 | 静态分配，钩子指向 `board_fatal()` |

设计取舍：

- 安全与运动控制合并为一个 5 ms 任务，避免两个任务对同一执行器写操作。
- 通信采用轮询而非事件通知：UART 由中断收字节入环形缓冲，任务只搬运，ISR 不做
  协议解析，满足"中断里不做耗时业务"。
- 遥测与 HMI 优先级最低，链路拥塞或屏幕刷新不会推迟控制周期。
- 不额外创建"按键任务""LCD 任务"，职责相近且同周期，合并可减少上下文切换。

## 通信关系

```
USART1 ISR ─┐                        ┌─► PC 链路（遥测 + 参数 + 监测）
            ├─► 环形缓冲 ─► comm ────┤
UART7  ISR ─┘                        └─► BLE 链路（遥控 + 状态）
                                   │
                                   └─► debug_handle ─► safety_* / params_*

control ──► Snapshot（临界区写） ──► hmi / telemetry / debug 读
```

## 共享资源与锁

| 资源 | 写者 | 读者 | 保护方式 |
|---|---|---|---|
| `Snapshot` | `control` | `comm`、`hmi`、`telemetry`、`diag` | `taskENTER_CRITICAL` 整结构拷贝 |
| `Safety` | `comm`（授权/指令）、`control`（步进/故障） | `debug` | 短临界区；只在 5 ms 任务里步进 |
| `Params` | `comm`（维护态） | 所有任务 | 32 位对齐写入；`revision` 递增 |
| UART 环形缓冲 | ISR | `comm` | 关中断取数，`lost` 标志报告溢出 |
| 执行器 | `control` | — | 单一所有者，其他任务不得直接调用 |

锁顺序：不存在嵌套加锁；临界区只做结构拷贝或指针交换，不做阻塞调用。

## 时基

- HAL 时基由 `TIM6` 提供（`BSP/timebase.c`），与 FreeRTOS 的 SysTick 分离，
  避免 `HAL_Delay` 与调度器互相影响。
- 任务周期统一用 `vTaskDelayUntil`，以绝对节拍推进，累计误差不随时间漂移。
- 控制任务记录单次执行耗时上界（`loop_max_us`），`diag` 任务汇总到快照，
  超过 `CHEN_TASK_DEADLINE_MS` 时 HMI 任务健康位显示异常。

## 看门狗

IWDG 尚未启动。原因：复位后从 `main()` 到调度器的窗口、以及停机调试时的喂狗策略
需要先在实物上确认；未经确认就开启会把调试过程变成反复复位。接口位置在
`diag_task`，实现前必须先完成 `docs/HARDWARE_TODO.md` 中的复位行为验证。

## 栈与余量

栈按"最深调用链 + 32 字节余量"估算：HMI 需要 `Hmi` 结构（约 5.8 KB 在 `.bss`，
不在栈上）与条带绘制，栈 512 字；控制任务含浮点无关的整数运算，384 字。
实测峰值需在实物上用 `uxTaskGetStackHighWaterMark` 采集，当前为设计值，不是测量值。
