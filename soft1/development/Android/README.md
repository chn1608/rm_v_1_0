# Android 15 客户端基础

`protocol/Protocol.kt` 可独立编译，用于 BLE GATT 回调后的字节流解析。测试读取 `../Protocol/vectors.txt`。编码器和解码器无 Android API 依赖。

```powershell
kotlinc protocol/Protocol.kt protocol/VectorTest.kt -include-runtime -d protocol-test.jar
java -jar protocol-test.jar ../Protocol/vectors.txt
```

目标为 Android 15 API 35。APP 工程、遥控交互和 BLE GATT 适配待开发。真实接入前，记录 BLE 板版、固件、UUID、写入方式、CCC 通知和实际 MTU。连接事件清空解析器和待发运动帧。权限用 BLUETOOTH_SCAN 和 BLUETOOTH_CONNECT，后台及触控取消须清空运动请求。控制许可由 MCU 状态机处理。
