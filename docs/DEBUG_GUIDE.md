# 调试与标定

## 主机模拟和回放

```powershell
python soft1/development/Tools/build.py host
python soft1/development/PC_Debug/monitor.py --sim --samples 20 --csv build/status.csv --record build/status.bin
python soft1/development/PC_Debug/monitor.py --replay build/status.bin --csv build/replay.csv
python soft1/development/PC_Debug/test_vectors.py
```

CSV 与接收日志使用独占新建模式。示例文件已存在时换一个输出名。SIM 由 C 流协议进程响应真实编码请求，生成带 simulated 标签的固定示例电压。串口适配基于 PySerial，可在项目虚拟环境安装后使用 `--list-ports` 和 `--serial COMx`。实物需先核 P10 针序和 TTL 电平。PC 当前只查询状态。

## 标定记录

| 类别 | 当前 | 后续验收记录 |
|---|---|---|
| 可写参数 | RAM 仅 ID 1 遥测建议周期。默认 50 ms，20–1000 ms | 本地、待机、维护授权；写后读回；重启回默认 |
| 电压 | PC4 板端分压名义 11:1 | 双点记录码值、VREF、参考仪器、温度、误差以及衰减 |
| 独立舵机 | 分段映射函数已做软件测试，以毫度和 μs 表示 | 人工设置机械中心和两端限位，逐步核动作、脉宽、掉电状态 |
| 底盘 | 限幅、斜坡、换向零速指令等待已测 | 阻塞、温升和硬件关断；测速方案选型后标定
| 保护 | 真实值未启用 | 硬急停、欠压、自检、复位、回供和故障保持需实物签认 |

真实反馈缺失时输出未安装/未标定或 N/A。标定持久化须有区域、版本、CRC、掉电恢复及磨损预算。
