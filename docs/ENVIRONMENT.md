# 本机开发环境 2026-11-27

| 项目 | 查验结果 |
|---|---|
| Git | 2.56.0.windows.4。根目录初始无仓库，现初始化 `development/v1.2`，身份配置存在。参考 MC02 仓库干净，RM2021_Hero 两个 MDK 文件已有用户修改 |
| 主机 C | MinGW-w64 GCC 8.1.0，正常编译和执行 C99 测试 |
| Arm | `D:/keil5/ARM/ARMCC/bin` 的 Arm Compiler 5.06 update 5 build 528 完成三种构建。另检测到 Keil UV4、Armclang 6.7 和 CubeMX 目录。arm-none-eabi-gcc、CMake、Ninja 未在 PATH 检出 |
| Python | `D:/Anaconda3/python.exe` 3.14.6，主机测试可运行。检查到 openpyxl、pypdf 和 tkinter，未检测到 PySerial。接线图使用 Codex 随附 reportlab 4.4.9、Poppler。 |
| Android | CubeMX 内有 Temurin 17.0.8.1 JRE。未发现 API 35、adb、Gradle 或 Android Studio，在常用路径与 PATH 均未核实。Kotlin 协议验证见测试报告。完整 APK 需 JDK、Android SDK Platform 35/Build Tools 及兼容 AGP/Gradle。|

工程依赖现有参考快照，不要求修改机器全局 PATH。目录中现行构建只读取参考代码，不使用其旧目标文件。
