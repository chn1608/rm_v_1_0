# 机甲杯机器人电控 V1.2

项目首次里程碑为硬件候选审查与 STM32H723 HAL＋FreeRTOS 可构建基线。工作目录为本项目根目录。

- [开发进度与接续计划](docs/DEVELOPMENT_STATUS.md)
- [MC02＋LCD 台架接线和 Keil5 验证](docs/LCD_SMOKE_GUIDE.md)
- [接线图（四页 PDF）](docs/wiring-v1.2.pdf) · [接线说明](docs/HARDWARE_INTERFACE.md) · [GPIO 分配](docs/GPIO_MAP.md) · [新旧对照](docs/GPIO_CHANGELOG.md)
- [参考资料审查](docs/REFERENCES.md) · [试验记录](docs/TEST_REPORT.md) · [硬件待确认项](docs/HARDWARE_TODO.md)
- [协议](docs/PROTOCOL.md) · [调试指南](docs/DEBUG_GUIDE.md) · [阶段计划](docs/DEVELOPMENT_PLAN.md)

```powershell
python soft1/development/Tools/build.py host
python soft1/development/Tools/build.py arm
python soft1/development/PC_Debug/monitor.py --sim --samples 5
```

主机测试需要 C99 GCC 和 Python，Arm 构建复用工程既有的 `hard1/hard_lib_1/MC02_data/mc02_control` 及本机 Keil Arm Compiler 5。非本机路径用 `--arm-bin`，单一配置用 `--profile SIM`、`BENCH` 或 `RUN`。生成文件在 `build/`，不随 Git 发布。原始参考工程在各自目录内独立管理。物理接线和带载前置条件见硬件待确认项。

| 配置 | 当前软件行为 |
|---|---|
| SIM | 以模拟设备返回标注为 simulated 的状态。兼容主机和 MCU 目标。 |
| BENCH | 仅开放有线只读调试协议，未完成真实互锁时自检锁存故障。 |
| RUN | 同一硬件保护，构建中不含模拟设备与故障注入入口。 |

三个 MCU 镜像都把板载外部电源使能保持低，候选继电器、PWM、STBY 等动力脚保持复位态。软件构建和主机测试不能检验实际复位波形。项目尚无实物试验。
