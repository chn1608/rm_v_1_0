# MC02 LCD 与 SWD 技术验证

适用 DM-MC-Board02、达妙配套 1.69 英寸 240×280 LCD／五向键和 ST-LINK/V2。用电脑 USB-C 线路给 MC02 和 LCD 供电，另用 USB 线路连接探头。该组合可验证进入主程序、SWD 识别与断点、HAL 时基、FreeRTOS 调度、SPI 显示和 ADC 按键。所有电机、舵机、继电器和外部电源均不接入。

## 硬件连接

使用 [独立台架接线图](lcd-smoke-wiring.pdf)。主板图中 USB VBUS 经 D14 接到系统电源 VCC_5VS，为 MCU 的 3.3 V 稳压级和 TFT 提供上游电源。该电源路径与板端受控 VCC_5V 分开。仅在对应板版、配套连接和供电稳定的情况下开展验证。

1. 拔掉主板和探头的 USB，核对主板与屏幕板版、配套连接的防反方向、P1 Pin 1 和地。主板图和 TFT 子板图的接地触点为 U13.10 和 U2.10，KEY 为 .9。手册第十四节文字表与图纸编号有出入，断电按实物通断核对后再插接。
2. 原装盒装版 ST-LINK/V2 的 CN3 接线为 P1.1 → VAPP（CN3.1/.2）、P1.2 → GND（例如 CN3.4）、P1.3 → SWCLK（CN3.9）、P1.4 ↔ SWDIO（CN3.7）。使用短线。原装 CN3.19 的 VDD 输出悬空。若是小棒版，只接已核实的 GND、SWCLK、SWDIO，3.3 V 和 5 V 输出悬空。只有确认是电压参考输入的针脚才接 P1.1。
3. 用电脑标准 5 V USB 接 MC02 USB-C，再给探头接 USB。XT30 VIN、受控 OUT、PWM、外接 5 V 均留空。若具备万用表，检查 P1.1 对 P1.2 约 3.3 V 且背光打开后保持稳定。遇异常先断电复查供电与排线。
4. P1 没有 NRST。先用普通 SWD 连接和软件复位。板上 RESET 键可人工按下，SWO 留空以保留 PB3 的 LCD SPI1 时钟。

## 可选 USB 转串口

USB 转串口只用于收发和信号参考。MC02 和 LCD 始终经 MC02 的 USB-C 路径独立供电。USB 转串口模块的 **VCC、3.3 V、5 V 供电脚全部悬空**。适配器在自身 USB 插入时由电脑供电。先给主板上电，再给探头和转串口插 USB。断电时先拔探头和转串口的 USB，再拔主板 USB。

确认模块信号为 3.3 V TTL 后，按主板原理图接 P10.1（PA9/TX）→ 模块 RXD、P10.2（PA10/RX）← 模块 TXD、P10.3 ↔ 模块 GND。手册文字表的针序与原理图不同，先断电核地和 Pin 1。只观测日志时可先接 TX → RXD 和 GND，TXD 留空。若模块仅支持 5 V TTL 或 RS-232／RS-485，本轮不接 P10。

串口助手设 115200 bit/s、8 data bits、无校验、1 stop bit、无流控。选择 USB 转串口模块对应的 COM 口，再按 MC02 的 RESET。启动打印 `LCD_SMOKE USART1 115200 8N1`，运行时每秒输出一条 `HB`，按键变化可见 `KEY`。连接了 RX 后发送 ASCII `?` 即回 `STATUS`，无需换行。诊断行包含 `hb`、`st`、`fault`、`key`、`raw`、`adc`、`lcd`。

## Keil5 编译与下载

1. 打开 [MC02_LCD_Smoke.uvprojx](../soft1/development/Firmware/MDK-ARM/MC02_LCD_Smoke.uvprojx)，选择 `LCD_SMOKE`。保持项目原有目录结构，工程以相对路径读取本地 MC02 参考 HAL/CMSIS/FreeRTOS。
2. 在 Keil5 选择 `Project → Build Target`（F7）。当前目标以自带参考源码和 scatter 文件编译，使用 Arm Compiler 5.06。编译不依赖 DFP。AXF 用于源码调试，HEX 用于烧录。二者在根目录 `build/keil/LCD_SMOKE/`。
3. 在 `Options for Target → Debug` 选择 `ST-Link Debugger`，进入 `Settings` 选 `Port: SW`。初次使用约 1 MHz 或探头支持的更低频率，关闭 Trace/SWO，先确认显示 SW-DP。
4. 在 `Utilities` 或调试器 `Flash Download` 设置页选择 H723 对应的 **STM32H72x/H73x 1 MiB 内部 Flash** 算法（`STM32H72x-73x_1024.FLM`，起始地址 `0x08000000`）。如算法列表为空，先在 Pack Installer 安装包含 STM32H723VGTx 的 STM32H7xx_DFP。选择按扇区擦除及编程后校验。Keil 检测到探头、目标且用户确认可覆盖当前应用后，才执行 Download。本项目构建流程不会调用下载器。
5. 用 `Start/Stop Debug Session` 和 Run 跑固件，可在 `main` 或显示任务打断点。若只看到供电而 SWD 无目标，先检查目标 3.3 V、GND、SWCLK/SWDIO 和 VAPP 性质，再调低时钟。只有在有独立 NRST 接线时才可依赖硬件 Connect under Reset。

## 上板验收

| 项目 | 观察点 | 通过条件 |
|---|---|---|
| 供电 | MCU P1.1 对 P1.2、重启现象 | 目标 3.3 V 稳定，背光开启无反复重启 |
| SWD | Keil Debug Settings 和断点 | 识别 SW-DP、程序校验，能暂停于 `main` 并继续 |
| 显示 | LCD 图样 | 可区分红绿蓝与五键十字，底部心跳约每秒变化 |
| 五向键 | 上、下、左、右、中心逐个按压及释放 | 对应块进入按下状态，释放后复原。持续按住时不应乱跳 |
| USART1 | PC 终端 | 启动行、每秒 HB，按键状态随方向变化。发 `?` 收到 STATUS。正常运行时 `st=5 fault=0 adc=1 lcd=1` |
| 调试观察 | `lcd_smoke` | stage=5，fault=0，panel_ready=1，adc_ready=1，heartbeat 递增。观察 adc_raw、key_mv、key_stable 与 key_changes，记录 adc_errors。五个方向均识别后 seen_keys 为 0x1F |
| 重复启动 | RESET 或重新按顺序上电 | 每次启动均可重现图样、心跳和按键。未做实物前，在测试记录填写待测 |

故障码 3 为 LCD SPI 初始化或写入错误，故障码 4 为 ADC 初始化错误。LCD 写入失败后 ADC／串口继续诊断。TFT 为单向发送。调试器里的初始化阶段只能证明 SPI 写入流程完成，屏幕是否显示需要现场观察。五向键以官方子板的名义电压做初始判别，实物电压、窗口和稳定性以记录为准。

[达妙 MC02 官方资料](https://github.com/dmBots/DM-MC02) · [ST-LINK/V2 UM1075](https://www.st.com/resource/en/user_manual/um1075-stsw-link007.pdf) · [ST 调试工具指南 AN4989](https://www.st.com/resource/en/application_note/an4989-stm32-microcontroller-debug-toolbox-stmicroelectronics.pdf) · [Arm STM32H7xx DFP](https://www.keil.arm.com/packs/stm32h7xx_dfp-keil/)
