# 参考资料审查

- 本地 `hard1/hard_lib_1/MC02_data/mc02_control` 为 Apache-2.0 的项目参考快照，初始提交 `f4257af`，origin 为第三方 `rocketman123456/mc02_control`。它给出可用的 HAL、CMSIS、FreeRTOS（MIT）与 Keil 工程。示例 GPIO/主函数会拉高 PC13/14/15，TIM1 为 HAL 时基，TIM2 驱动演示调度，SPI6 用 PA5/PA7，SPI2 使用 DMA1 Stream1/2。新项目复用平台代码和 HSE 时钟思路，另写启动策略。
- 厂商 `dmBots/DM-MC02` 资料树审查快照 `d65a3688981405dc205f628f1a026bde8cf8303e`。审查 V1.0/V1.1 手册、主板原理图第 2/5/6 页、板视图、DM_TFT-V1.1 原理图、ALL 例程 SPI/ADC/GPIO 定义及 KEY 例程。ALL 的 ADC1 扫描 CH4/CH19（PA5），DMA1 Stream0。ALL 的 `BSP_UserKey_Detect` 扫板载 PA15，不能替代五向键 ADC。
- TFT 图纸用旧主板 MCU 名标注接头网络。以功能匹配 MC02。PB10/PB11 在通用插座可作 I2C2，连接官方模块后分别连接 BLK/RES。PA5 名义电平中心/左/右/上/下/释放约 0/0.66/1.32/1.98/2.64/3.30 V。待核实际供电、键值窗和复用。U13 针序冲突见接线图。
- RM2021_Hero 为 STM32F405，Apache-2.0。Chassis/Gimbal 分工程，USP 区分系统数据、任务与业务，SRML 区分组件、设备、算法和控制。参考任务所有权、限幅、周期处理与通用分层。两份 MDK 工程文件已有本地修改。该工程的 F405 引脚、控制硬件及应用代码均未移植。
- 原资料审查包括现行 `机甲杯电控_MC02_ioc引脚配置清单.xlsx` 逐脚配置、定时器、冲突和台架验收，`RM_V1.0_MC02硬件框架复核与修正清单.xlsx`，以及 `4S锂电机器人电控硬件选型与架构方案_软件接口版.docx`。现行工作簿已处理多项比 V1.2 正文更早的冲突。表内“定位已确认”属于资料定位，实物验收栏待核。
- 正点原子官网确认 BLE02 有裸模组和底板物料，原厂入口给百度网盘及提取码。当前取得分享提取页，未取得可审核的用户手册、AT 指令或原厂例程。VCC、逻辑电平、状态脚语义、实际波特率、UUID、写入类型、通知、MTU 和串口分包暂不冻结。初次独立台架只按实物版本与资料验证，先量电平、再查 UART/AT 与 BLE GATT，记录版本和读回值。
- STM32H723 DS13313 Rev 5 表 7/8 用于核对复用和 ADC 映射。PA5 正输入为 ADC12_INP19，图纸网名 CH18 不可直接作为配置值。

[达妙官方资料](https://github.com/dmBots/DM-MC02) · [达妙 TFT 图纸](https://github.com/dmBots/DM-MC02/blob/d65a3688981405dc205f628f1a026bde8cf8303e/drawings/2d/lcd-module/DM_TFT-V1.1.pdf) · [正点原子 BLE02](https://www.openedv.com/docs/modules/iot/ATK-BLE02.html) · [ST 数据手册](https://www.st.com/resource/en/datasheet/stm32h723vg.pdf) · [Android 15 SDK](https://developer.android.com/about/versions/15/setup-sdk)
