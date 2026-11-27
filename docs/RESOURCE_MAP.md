# 外设资源与余量

| 功能 | 分配 | 影响与预留 |
|---|---|---|
| HAL/RTOS | TIM6 / SysTick，各 1 ms；HAL 优先级 15 | 不占 TIM1/2。IRQ TIM6_DAC 仅 HAL，SysTick/PendSV/SVC 为内核 |
| 电机/舵机/蜂鸣器/加热 | TIM2 CH1/3、TIM1 CH1、TIM12 CH2、TIM3 CH4 | 轴、分频、波形待实测。20 kHz/50 Hz/4 kHz 为旧表设计目标 |
| UART | USART1 PC，UART7 BLE，UART5 SBUS；备用 UART9 PD14/15 P16.11/.9 | USART10/UART8 在现有候选中让位；RS485 USART2/3 独立 |
| SPI | SPI1 LCD，SPI2 BMI088，SPI6 停；SPI3 在当前 GPIO 方案中不成立 | SPI1 的 TFT 线束无独立额外 CS，新增 SPI 待另审 |
| I2C | I2C2 PB10/11 被官方 TFT GPIO 使用 | I2C1 PB8/9（P16.22/.24 或 DCMI.20/.22，按图与实物复核）可做扩展，需核已有上拉及 3.3 V |
| ADC | ADC1 PC4 CH4/PA5 CH19；PA4 CH18、PA6 CH3 支路反馈候选 | 额外模拟资源未找到满足足够引出与当前复用的 3–4 路。优先在备用 I2C 上评审 ADC/电流设备 |
| 双编码器 | 每个编码器需要同一计数器 CH1/CH2 | 外置双路计数或带速度回传电机优先；PD14/15 只是 TIM4 CH3/4，不能承诺硬件编码。GPIO 候选依赖外设替换后再冻结 |
| 其他可用脚 | PD14/15、PB8/9 按通信预留，DCMI PC6/7/8/9 预留 | 占 DCMI，待核接口布线及高频输入；同一脚不得重复承诺 |
| CAN | FDCAN1/2/3 板载三路 | 供电、端末阻抗和调试条件确认后启用 |
| DMA | 当前不用；例程 ADC1 DMA1.S0、SPI2 DMA1.S1/S2 | 预留 UART7 RX DMA1.S3、USART1 RX DMA1.S4、LCD SPI1 TX DMA1.S5 作为候选。明确 DMAMUX1 request 及环形 RX |
| IRQ | 候选 DMA/UART/EXTI 5–7 | 优先级数值小于 5 的 ISR 不调用 RTOS FromISR；紧急断能硬件独立 |
| SWD/NVM | PA13/14 保留；板载 W25Q64 | 外置 Flash 参数分区未冻结；先 RAM 配置 |

STM32H7 构建基线在 DTCM 放 RW/ZI/任务栈。引入 DMA 时，须独立指定 DMA1 可达 SRAM 段，按缓存行对齐并管理 D-Cache 归属与周期。当前无 DMA、无实物性能数据。镜像区域、静态内存量及运行测量项目见测试报告。
