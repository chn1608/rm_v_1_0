# MC02 交付界面台架验证

适用 DM-MC-Board02、达妙配套 1.69 英寸 240×280 LCD／五向键和 ST-LINK/V2。用户已确认当前核心板供电正常，LCD 能显示官方测试画面。工程新镜像的下载、显示和按键尚待上板确认。

## 接线

使用 [台架接线图](lcd-smoke-wiring.pdf)。保留已验证可显示官方画面的主板、LCD 和 USB-C 供电方式。MC02 通过自己的 USB-C 接电脑，ST-LINK/V2 通过另外的 USB 接电脑。P10 留空。XT30 VIN、受控 OUT、电机、舵机、继电器和外接 5 V 均留空。

原装盒装 ST-LINK/V2 用 CN3。P1.1 接 VAPP 参考输入（CN3.1/.2），P1.2 接 GND（例如 CN3.4），P1.3 接 SWCLK（CN3.9），P1.4 接 SWDIO（CN3.7）。CN3.19 VDD 输出留空。小棒版按实际 GND、SWCLK、SWDIO 接。只有确认是电压参考输入才接 P1.1，所有供电脚留空。

P1 没有 NRST。使用普通 SWD 和软件复位，SWO 留空以保留 PB3 的 LCD SPI1 时钟。需要检查排线时先断开 USB；主板 U13 和子板 U2 图上是 .9 KEY、.10 GND，手册文字表编号有冲突，按板版和断电通断核对。

## Keil5 下载算法

`No Algorithm found for: 08000000H - 0800955FH` 说明 Keil 读到了 AXF 的 Flash 段，但当前下载配置没有覆盖该地址的编程算法。`Erase skipped` 表示这次擦除被跳过。日志末尾的 Cortex-M7 是目标内核名称。

1. 退出当前调试会话，重新打开 [MC02_LCD_Smoke.uvprojx](../soft1/development/Firmware/MDK-ARM/MC02_LCD_Smoke.uvprojx)，选目标 `LCD_SMOKE`。按 F7 构建。输出在 `build/keil/LCD_SMOKE/`，同一个 ARM-MDK 项目相对引用项目的 HAL/CMSIS/FreeRTOS。
2. 打开 `Options for Target → Debug → ST-Link Debugger → Settings`，确认 `Port: SW`、Trace 关闭，并先检查能否识别 SW-DP。必要时降低 SWD 频率。
3. 在 `Flash Download → Programming Algorithm` 查看 STM32H72x/H73x **1 MiB 内部 Flash**。官方文件名为 `STM32H72x-73x_1024.FLM`。Start 为 `0x08000000`，Size 为 `0x00100000`，覆盖至 `0x080FFFFF`。RAM for Algorithm 使用 `0x20000000`，大小 `0x00008000`。
4. `Options for Target → Utilities` 使用目标调试器执行 Flash Programming。使用 `Erase Sectors`、`Program`、`Verify`。要保持官方演示时可仅编译和检查设置。确认接受覆盖当前官方应用后，再点 `Flash → Download`。
5. 观察 Keil 的擦除、编程和校验结果。下载完成后运行固件，或按一次板上 RESET。若仍失败，记录新的完整日志，以及 Flash Download 中的算法、Start 和 Size。

## 界面与验收

台架与常规固件共用 LCD 驱动、五向键扫描、页面、导航和渲染器。现阶段页面为系统主页、底盘电机、舵机、通信、故障诊断和系统设置。未提供的数据和未开放的设置显示“未知”。运行时间等实际可读的软件数据正常显示。

上板检查先确认进入主页，再逐页检查标题、数据标签和“未知”显示，核对五向键转页和返回。通过 Keil Watch 观察 `lcd_runtime` 的运行状态和按键 ADC 码。物理 LCD 采用单向 SPI，界面正常点亮以实物观察为准。

[达妙 MC02 官方资料](https://github.com/dmBots/DM-MC02) · [ST-LINK/V2 UM1075](https://www.st.com/resource/en/user_manual/um1075-stsw-link007.pdf) · [Keil Flash 编程算法](https://www.keil.com/support/man/docs/ulinkme/ulinkme_su_programming_algorithm.htm) · [Arm STM32H7xx_DFP](https://www.keil.arm.com/packs/stm32h7xx_dfp-keil/versions/)
