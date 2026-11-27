# V1.2 GPIO 分配（评审候选）

**L0** 固定 PCB · **L1** 人工签认线束 · **L2** 暂定接口 · **L3** 扩展。当前资料未提供 L1 签认清单。I/O 方向以 MC02 为参照。主板 3.3 V 逻辑与接头上的 5 V 电源分开。表内初态为设计目标或现行软件配置，复位至应用初始化期间以硬件电路和实测为准。S1–S4 为 P11 信号排由 PA0 到 PE13 的逻辑顺序，按丝印和通断识别。

| 功能 | MCU 引脚/外设 | 座/触点 | 模式、方向、电平 | 初态及上下拉 | 等级、依据及冲突 |
|---|---|---|---|---|---|
| PC TX/RX | PA9/PA10 USART1 AF7 | P10.1 TX / .2 RX / .3 地 | AF PP，出/入，3.3 V | UART 空闲高；不用外供 VCC | L2，主板图2/板视图，手册重排，实板待核 |
| BLE RX/TX | PE7/PE8 UART7 AF7 | P13.3 RX / .4 TX / .2 地 / .1 板端5V | AF PP，入/出，3.3 V MCU | UART 空闲高；初版未启用 | L2，主板图2；待核 BLE 对端，P13.1 暂空 |
| BLE LINK/STA | 暂无 GPIO | PD14/PD15 留给 P16 UART9 | 未配置，待确认模块引脚、方向和逻辑 | 暂空 | L2；外设冲突 UART9 |
| LCD 电源/地 | VCC_5VS/GND | U13 主板图 .1/.10，TFT U2 .1/.10 | 5 V 出/地 | 用官方线束；测极性；.11/.12 图为固定焊脚 | L0，手册§14 针序有异 |
| LCD CS/SCK | PE15/PB3 SPI1 | U13 图 .2/.3 | GPIO PP CS / AF5，出，3.3 V | CS 不选中高，SCK 空闲按例程；初版未启用 | L0，ALL 例程；PB3 与 SWO 互斥 |
| LCD MISO/MOSI | PB4/PD7 SPI1 | U13 图 .4/.5 | MISO 保留，MOSI AF5 出 3.3 V | TFT 用 TX-only，初版未启用 | L0，TFT MISO 无屏端 MISO 驱动 |
| LCD BLK/RESET | PB10/PB11 | U13 图 .6/.7 | 官方 TFT GPIO 出，3.3 V | 初版未启用；背光初始关 | L0，ALL；通用 I2C2 AF4 与模块互斥 |
| LCD D/C | PD10 | U13 图 .8 | GPIO PP 出，3.3 V | 初版未启用 | L0，ALL |
| 五向键 KEY | PA5 ADC1_INP19 | U13 图 .9，TFT U2 .9 | 模拟入，板载 TFT 网络 0–3.3 V 名义 | MCU 无拉，TFT 有上拉；待实测 | L0，PCB/DS13313/ALL；SPI6 禁用 |
| WS2812 | PA7 SPI6_MOSI | 板载 | 固定信号出 | 当前禁用 SPI6 | L0，PA5 冲突；不得作电流采集 |
| BMI088 SPI2 | PB13/PC1/PC2_C；PC0/PC3 | 板载 | AF5 SCK/MOSI/MISO；双 CS 输出，3.3 V | 待驱动接入，CS 高空闲，PC2_C 按模拟开关配置 | L0，图纸＋例程 |
| BMI088 加热/中断 | PB1 TIM3_CH4 AF2；PE10/PE12 | 板载 | PWM 出；EXTI10/12 入 | 加热初始禁用；中断极性按实测 | L0，图纸＋例程 |
| 蜂鸣器 | PB15 TIM12_CH2 AF2 | 板载 | PWM 出 | 关闭 | L0，图纸 |
| 用户键/复位 | PA15/NRST | SW1/SW2 | GPIO 入/硬复位 | 板载上拉/低有效 | L0，独立于五向按键 |
| SWD | PA14 SWCLK/PA13 SWDIO | P1.3/.4，.1目标3.3V，.2地 | AF0，3.3 V 参考 | 保留调试 | L0，图纸/手册 |
| USB FS | PA11/PA12 OTG_HS FS | USB-C D−/D+ | AF10，USB PHY | 程序无 USB 初始化；USB 仍可能供系统 | L0 |
| 固定受控电源 | PC13 OUT2、PC14 OUT1、PC15 5V_EN | 两组受控 OUT、P11/P13/CAN 分路 | GPIO PP 出 3.3 V | 应用显式低；复位/端口电压待测 | L0，勿启 LSE，勿走动力旁路 |
| W25Q64 | PE11/PD11/PB0/PA3/PA1/PB2 | 板载 | OCTOSPI2；CS/D0/D1/D2/D3/CLK | 独占；初版未启用 | L0，PA1/PB0/PA3 禁作外接用途 |
| PWMA/PWMB | PA0/PA2 TIM2_CH1/3 AF1 | P11 S1/S2 → TB H1.8/.2 | AF PP 出 3.3 V | 初版未配置；后续 CCR0 且 STBY 低 | L2，图2/旧表/模块说明 |
| AIN1/AIN2 | PE13/PE14 | P11 S4 / P16.13 → TB H1.6/.7 | GPIO PP 出 3.3 V | 初版未配置；候选低，驱动端核实际下拉 | L2，PE13 与 TIM1_CH3 互斥 |
| BIN1/BIN2 | PC12/PC11 | P16.19/.21 → TB H1.4/.3 | GPIO PP 出 3.3 V | 初版未配置；候选低，核下拉 | L2，占 SPI3 MOSI/MISO |
| STBY | PC10 | P16.23 → TB H1.5 | GPIO PP 出 3.3 V | 初版未配置；驱动端硬下拉，失效低 | L2，占 SPI3 SCK |
| 独立舵机台架 PWM | PE9 TIM1_CH1 AF1 | P11 S3 | AF PP 出 3.3 V | 初版未配置；独立外供，失能无波形待实测 | L2，和参考 TIM1 时基解绑 |
| K1/K2 | PE1/PE0 | P16.17/.15 → ULN IN1/IN2 候选 | GPIO PP 出 3.3 V | 初版未配置；外部下拉，复位释放 | L2，UART8 互斥；急停独立 |
| 急停/再使能 | PE2/PE3 | P15.3/.4（按原理图核验） | 隔离后 GPIO 入 3.3 V | 高、断线或无效为不安全；PE3 需边沿 | L2/L3，与 USART10 互斥；初版未配置 |
| 母线板侧 | PC4 ADC1_INP4 | 板载 R86 100k/R87 10k | 模拟入；名义比例 11:1 | 模拟无拉；待两点标定 | L0，图2；板侧与电池端分别量 |
| K1 后电压 | PA4 ADC1_INP18 | DCMI.16 | 模拟入，分压/保护适配 | 无拉；未安装保护前保持断开 | L2，图6；占 DCMI |
| K2 后触点电压 | PA6 ADC1_INP3 | DCMI.8 | 模拟入，独立分压/保护 | 同上；独立记录 UBEC 输出 | L2，图6；占 DCMI |
| CAN1/2/3 | PD0/PD1、PB5/PB6、PD12/PD13 | CAN 端子；第三路亦经 P16.10/.12 | 板载收发器，CANH/L 双线 | 禁用；布线及端接待定 | L0/L3，图1/4/6 |
| RS485 #1/#2 | PD8/PD9/PB14、PD5/PD6/PD4 | P5/P4，及 P16 差分 | USART3/2 经板载差分 | 禁用；DE 初态由硬件和后续 BSP 控制 | L0/L3，图5/6 |
| SBUS | PD2 UART5_RX | P6.1 信号 / .2 5V / .3 地 | 板载反相 RX，100 k 8E2 候选 | 禁用，外供时核隔离 | L0/L3，图5；手册 PC02 有误 |

扩展资源与物理位置见 [RESOURCE_MAP](RESOURCE_MAP.md)。线束级图纸见 [HARDWARE_INTERFACE](HARDWARE_INTERFACE.md)。
