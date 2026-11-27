# 来源与范围声明

本仓库为**私有**仓库，保存本项目自身的开发产物：

1. `chen_bot_v_1_0/` —— 本次会话按《V_1.2》新建的固件框架；
2. `soft1/development/` —— 台架固件、PC 调试、Android 协议、Tools 等开发内容；
3. 项目规范与设计文档（`AGENTS.md`、`V_1.2.md`、`PROJECT_REQUIREMENTS.md`、`README.md`、`docs/`）。

**不包含**：达妙 DM-MC02 官方工程的 HAL/CMSIS/FreeRTOS 源码（`hard1/`，仅本地按相对路径引用）
与 `soft1/soft_lib_1/` 下的 RM2021_Hero 参考工程。这两部分属于第三方资料，只保留在本地，
不得上传或再分发。

## 代码来源分级

| 范围 | 来源 | 说明 |
|---|---|---|
| `Firmware/Config/*`（除 FreeRTOSConfig.h） | 本会话原创 | profile / features / board_config / params |
| `Firmware/Control/*` | 本会话原创 | safety / chassis / servo_ctrl / pid |
| `Firmware/Service/*` | 本会话原创（协议沿用既有线格式） | protocol / faults / snapshot |
| `Firmware/Debug/*` | 本会话原创 | debug_service、权限矩阵 |
| `Firmware/APP/*`、`Firmware/main.c` | 本会话原创 | 任务划分与启动顺序 |
| `Firmware/Device/devices.h`、`Real/*`、`Mock/*` | 本会话原创 | 统一设备抽象与适配 |
| `Firmware/Device/keys.c/.h` | 本会话重写 | 判据改为最近档位；档位电压来自模块原理图 |
| `Firmware/BSP/board.c`、`uart_link.c` | 本会话原创 | 时钟树数值沿用厂商参考工程 |
| `Firmware/BSP/timebase.c` | 厂商派生 | HAL timebase 契约与实现来自 MC02 官方工程（ST 版权头保留） |
| `Firmware/BSP/lcd.c/.h` | 继承 + 移植 | 由上一轮会话的 `LcdSmoke/panel.c` 移植，新增 blit |
| `Firmware/HMI/*` | 继承 | hmi.c/h、hmi_font.h、hmi_text.h、generate_font.py、text.json 来自上一轮会话 |
| `Firmware/Config/FreeRTOSConfig.h`、`BSP/layout.sct` | 厂商派生 | 来自 MC02 官方工程，按本工程内存布局调整 |
| `MDK-ARM/config/stm32h7xx_hal_conf.h` | 厂商派生 | ST 官方配置副本，仅打开本工程用到的 HAL 模块 |
| `Tools/*`、`docs/*`（本目录内） | 本会话原创 | 构建、Keil 工程生成、ST-Link 设置、接线图与文档 |

## 许可证提示

- ST HAL 驱动与 `stm32h7xx_hal_conf.h`：ST 的 BSD-3-Clause 类许可，文件内版权头不得删除。
- FreeRTOS：MIT，`FreeRTOSConfig.h` 属配置而非内核源码。
- 字体：见 `Firmware/HMI/FONT_LICENSE.txt`，随字体数据一并保留。
- 达妙 DM-MC02 的说明书、原理图与参考代码：**未上传**，仅本地只读引用；
  对外展示或再分发前需自行确认厂商授权。

## 维护约定

- 本仓库只通过"按单白名单打包"的方式更新，避免把厂商资料或他人工程误传上网。
- 需要新增目录进入仓库时，先在此文件登记来源，再提交。
