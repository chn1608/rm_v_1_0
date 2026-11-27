# FreeRTOS 任务与时基

本地参考工程 FreeRTOS V10.3.1（MIT），使用原有 RVDS ARM_CM4F 兼容端口及硬件 FPU 上下文，链接配置是 Cortex-M7.fp.dp。SysTick 对应 RTOS 1 ms，HAL TIM6 独立 1 ms，TIM6_DAC 优先级 15。USART1 IRQ 优先级 7，不调用 FreeRTOS API。采用固定优先级且全静态分配。

| 当前执行者 | 优先级 | 周期 | 所有权 |
|---|---:|---|---|
| safety-diag | 3 | 5 ms | 状态机、参数、PC 解析器、状态组装，先安全检查后服务串口，每轮最多 64 字节 |
| UART1 IRQ | NVIC 7 | 异步 | RX 入 256 字节环和 HAL TX，遇错误清流，缓冲拥塞可观测 |
| lcd-ui | 1 | 每轮结束后至少 20 ms | 台架和 BENCH/RUN 的独占 SPI1/ADC1 显示任务。每轮最多传输一个渲染带，启动首帧完成后开背光。任务栈 512 words。 |
| Idle | 0 | 调度空闲 | 128 words |
| TIM6 ISR | NVIC 15 | 1 ms | HAL tick，独立于 FreeRTOS SysTick |

`safety-diag` 静态栈 384 words，实物高水位尚未测。硬故障及栈溢出进入板级保持关断的异常路径。后续高负荷 LCD、ADC、通信接入时移出该任务，按 10 ms 按键、100–200 ms LCD 刷新、20–50 ms 普通遥测为初始建议。控制输入使用带本地到达时间的单槽，安全状态独占，低优先级只读快照通过固定容量消息交换。锁范围内禁止阻塞 I/O。IWDG 只有完成关键任务健康和复位证据后接入，复位期间的真实关断依赖硬件互锁。
