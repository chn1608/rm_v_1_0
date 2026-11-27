# 2026-11-27 构建与验证记录

## 已编译

运行 `python soft1/development/Tools/build.py arm`，实际使用 Arm Compiler 5.06u5。三个目标均成功链接，最终构建未报告错误或警告。链接映射确认 SysTick、TIM6_DAC 和 USART1 路由至实际处理函数，并确认模拟设备仅在 SIM 目标链接。

| STM32H723 配置 | 镜像 | ROM 字节 | RW/ZI DTCM 字节 |
|---|---|---:|---:|
| SIM | build/arm/SIM/rm_mc02.axf | 18388 | 4856 |
| BENCH | build/arm/BENCH/rm_mc02.axf | 18148 | 4840 |
| RUN | build/arm/RUN/rm_mc02.axf | 18148 | 4840 |

Flash 1 MiB，DTCM 128 KiB。本次最高约为 Flash 1.76%、DTCM 3.70%。IRQ、实际栈高水位、CPU 占用、队列占用、DMA 缓存和串口带宽为后续运行测量，编译映射不能推导上述结果。BSP 关注点为 HAL TIM6 时基与 SysTick 分离，TX 缓冲生命周期由单一所有者维护，UART 失步清残帧，ARM5 汇编宏使用可接受的整数形式。

## 软件测试和模拟

| 运行项 | 成功证据 |
|---|---|
| `python soft1/development/Tools/build.py host` | GCC 8.1.0，在 `-Wall -Wextra -Werror -pedantic` 下通过三种配置。每种读取五组协议向量，检查 CRC/错帧/分包/粘包/截断/非法长度及 300,000 随机字节恢复。 |
| 同一命令的安全测试 | 初始禁用、自检失败、人工确认、来源冲突、越界、重复及回绕、过期、断链、急停锁存、故障恢复后旧代次拒绝、BENCH/RUN 拒绝使能。 |
| 同一命令的参数、按键、控制测试 | PC 无参数写权限、本地授权和范围、无效数据标识、五方向与释放、消抖、限幅、斜坡和换向等待、独立舵机脉宽映射及非法配置；SIM 设备关断后清输出。 |
| `python soft1/development/PC_Debug/test_vectors.py` | Python 3.14.6，经同五组协议向量的编解码、分包和 CRC 错误用例。 |
| Kotlin JVM | Kotlin 2.1.21 CLI 和 Temurin 17.0.8.1 JRE，编译 `Android/protocol` 下两个文件并运行向量程序，5/5 通过。尚无 Android APK 构建。 |
| `monitor.py --sim --samples 1000 --period-ms 20` 与 `--replay` | Python/C 真实进程间字节流，收 1000 条状态，末帧有效计数 1000，错误 0。原 CSV 与回放 CSV 的 SHA-256 相同，`B0F26559FB131FD28240DEB55657516D09954C48047257B545D802D696770F53`。电压值带模拟标签。 |
| 四页接线图 | Poppler 四页渲染人工检查，正文可提取，项目版与交付版 SHA-256 相同。 |

发现并修正的问题包括 CRC 固定测试值转写错误、FreeRTOS 旧汇编端口不接受带 C 后缀的立即数宏、HAL 与 CMSIS 默认 HSE 值一处为 24 MHz 一处为 25 MHz、UART 发生帧错时提前重装 RX 可能擦掉 HAL 错误标志。审查 ARM 映射、供电默认态、候选引脚未配置、数据有效性、解析缓冲上界、调试命令权限和错误恢复。后续在实板核 UART 超限恢复和优先级。

## 尚未执行

- 未烧录。未进行 SWD、实物 UART、BLE 对频、屏幕、ADC 或无动力 MCU 运行验证。
- 未执行硬急停/假负载、带载、温升、编码器、底盘速度闭环、整车与赛事验收。
- 未执行 30 分钟实时压力、看门狗和实物调度测量。按硬件待确认清单完成对应试验后逐项追加日志、板号、条件与证据。

## LCD 与 SWD 技术验证构建

- Keil µVision 5.24a 使用 Arm Compiler 5.06u5（build 528），通过 `UV4.exe -b <MC02_LCD_Smoke.uvprojx> -t LCD_SMOKE -o <keil-build.log>` 从项目入口构建并链接 30 个编译／汇编单元。退出码 0，0 错误、0 警告。
- 构建目录 `build/keil/LCD_SMOKE/` 含 AXF、HEX、MAP 和 `keil-build.log`。ROM 38,240 字节，DTCM RW/ZI 5,440 字节。映射保留 `lcd_smoke` 调试结构。编译不依赖 STM32H7 DFP。本机 Flash 下载算法仍待通过 DFP 提供并核对。
- 使用 MinGW GCC 的 C99 模拟验证，编译生产 LCD 调度逻辑及实际按键滤波器，替换 HAL/RTOS 接口。正常、LCD 初始化失败、LCD 绘制失败、ADC 初始化失败、UART 不可用五个场景均通过。覆盖五键按下与释放、心跳、串口问号查询和显示失败后的独立诊断。原三种配置的主机测试和 Python 协议向量复查通过。
- 三页 [台架接线图](lcd-smoke-wiring.pdf) 已逐页渲染检查。该图的主控 USB-C 是唯一台架主板供电入口，转串口 VCC/3.3 V/5 V 全部留空。
- 尚未连接探头、烧录、读取目标电压或测量屏幕与串口实物状态。上板验收条件见 [LCD 台架指南](LCD_SMOKE_GUIDE.md)。
