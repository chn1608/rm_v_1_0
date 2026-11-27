# 资源分配

证据等级沿用 `docs/GPIO_MAP.md`：L0 板载固定、L1 已接线确认、L2 暂定、L3 新增/预留。

## 引脚

| 功能 | 引脚 | 外设/模式 | 等级 | 依据 | 备注 |
|---|---|---|---|---|---|
| LCD SCK | PB3 | SPI1_SCK AF5 | L0 | DM_TFT-V1.1 / 主板 J13 | 5 MHz，先分频 16 |
| LCD MOSI | PD7 | SPI1_MOSI AF5 | L0 | 同上 | 单向写 |
| LCD CS | PE15 | GPIO 输出 | L0 | 同上 | 低有效 |
| LCD DC | PD10 | GPIO 输出 | L0 | 同上 | 命令/数据 |
| LCD BLK | PB10 | GPIO 输出 | L0 | 同上 | 背光，高亮 |
| LCD RST | PB11 | GPIO 输出 | L0 | 同上 | 低复位 |
| 五向键 | PA5 | ADC1_INP19 | L0 | 模块图 + CubeMX | 梯形电阻 0/0.66/1.32/1.98/2.64 V |
| 用户键 | PA15 | GPIO 输入 | L0 | 主板 SW1 | 低有效，未在 HMI 使用 |
| 调试 TX/RX | PA9/PA10 | USART1 AF7 | L0 | 主板 J13 | 115200 8N1 |
| BLE TX/RX | PE8/PE7 | UART7 AF7 | L2 | 旧 GPIO 表 | 待模块手册确认波特率 |
| BLE LINK/STA | PD14/PD15 | GPIO 输入 | L2 | 旧 GPIO 表 | 上拉，未接模块时为"未连接" |
| 电机 A PWM | PA0 | TIM2_CH1 AF1 | L2 | 旧 GPIO 表 | 20 kHz |
| 电机 B PWM | PA2 | TIM2_CH3 AF1 | L2 | 旧 GPIO 表 | 20 kHz |
| AIN1/AIN2 | PE13/PE14 | GPIO 输出 | L2 | 旧 GPIO 表 | 方向，默认低 |
| BIN1/BIN2 | PD8/PD9 | GPIO 输出 | L3 | 本工程新增 | 原方案缺失，借用 USART3 脚 |
| STBY | PA1 | GPIO 输出 | L2 | 旧 GPIO 表 | 低=关断 |
| 舵机 PWM | PE9 | TIM1_CH1 AF1 | L2 | 旧 GPIO 表 | 1 µs 分辨率，20 ms 周期 |
| K1 继电器 | PB0 | GPIO 输出 | L2 | 旧 GPIO 表 | 高=吸合 |
| K2 继电器 | PA3 | GPIO 输出 | L2 | 旧 GPIO 表 | 高=吸合 |
| 电池电压 | PC4 | ADC1_INP4 | L2 | 主板分压 11:1 | 未标定 |
| 急停/触点电压 | PA6 | ADC1_INP3 | L2 | 旧 GPIO 表 | 分压未定，标记未标定 |
| 固定电源脚 | PC13/PC14/PC15 | GPIO 输出 | L0 | 旧方案 | 上电即拉低 |
| CAN1 RX | PD0 | FDCAN1_RX AF9 | L0 | 官方 DM-MC02 例程 | 板载收发器，接 PC 用 |
| CAN1 TX | PD1 | FDCAN1_TX AF9 | L0 | 同上 | 1 Mbit/s |
| CAN2 RX/TX | PB5/PB6 | FDCAN2 | L2 | 官方原理图 | 预留，未使用 |
| CAN3 RX/TX | PD12/PD13 | FDCAN3 | L2 | 官方原理图 | 预留，未使用 |

## 外设与中断

| 资源 | 用途 | 备注 |
|---|---|---|
| TIM2 CH1/CH3 | 双路电机 PWM | 240 MHz / 12 / 1000 = 20 kHz |
| TIM1 CH1 | 舵机 PWM | 1 MHz 计数，20 ms 周期 |
| TIM6 | HAL 时基 | 与 SysTick 分离 |
| SysTick | FreeRTOS 调度 | 1 kHz |
| ADC1 | 3 通道扫描（电池/触点/按键） | PLL2 96 MHz / 8 = 12 MHz |
| SPI1 | LCD | PLL1Q 80 MHz / 16 |
| USART1 | PC | RX 中断 + 环形缓冲 |
| UART7 | BLE | 同上 |
| DMA | 未启用 | 预留：LCD 批量、ADC 连续、UART 空闲中断 |
| IWDG | 未启用 | 见 HARDWARE_TODO |

中断优先级（`NVIC_PRIORITYGROUP_4`，数值越小越高）：

| 中断 | 抢占优先级 | 说明 |
|---|---:|---|
| SysTick | 15 | FreeRTOS 最低，符合 `configMAX_SYSCALL_INTERRUPT_PRIORITY` |
| TIM6 | 15 | HAL 时基 |
| USART1 / UART7 | 7 | 只搬字节，不做协议解析 |

## 余量

| 资源 | 已用 | 余量 | 目标 |
|---|---|---|---|
| Flash | 60.5 KB / 1 MB | 94% | ≤70% |
| DTCM (RW+ZI) | 18.2 KB / 128 KB | 86% | ≤70% |
| UART | 2 / 8 | USART2/3/6、UART4/5/8 可用 | ≥1 备用 |
| ADC | 3 / 20 | 17 通道 | 3–4 备用 |
| 定时器 | TIM1/2/6 | TIM3/4/5/8/15/16/17 可用 | 双编码器 |
| SPI | 1 / 4 | SPI2/3/4/5/6 | 可扩展 |
| I²C | 0 / 4 | I2C1..4 | ≥1 组 |
| FDCAN | 1 / 3 | FDCAN1 已用，FDCAN2/3 空闲 | 预留电机总线 |
| GPIO 空闲 | — | PA4/PA7/PA8/PA11..PA14、PB4..PB9、PE0..PE6 等 | 4–8 个 |
