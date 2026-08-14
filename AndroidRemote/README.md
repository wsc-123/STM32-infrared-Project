# 冷静星遥控器 Android App

这是给 `STM32 infrared Project` 配套的最小 Android 遥控器工程。

## 功能

- 首次打开填写巴法云 UID / 私钥。
- topic 默认 `infrared`，协议类型固定 TCP `type=3`。
- 通过巴法云官方 `https://apis.bemfa.com/va/postJsonMsg` 接口发送命令。
- 可读取 topic 最近一条消息，用于查看 `STATUS` 或设备回传。
- `allowBackup=false`，避免系统备份 App 内保存的 UID/topic。

## 按钮命令

| 按钮 | 发送命令 |
|------|----------|
| 红外测试 | `IR_TEST` |
| 查询状态 | `STATUS` |
| 开机 | `SEND 0` |
| 关机 | `SEND 1` |
| 制冷 26° | `SEND 2` |
| 制冷 23° | `SEND 3` |
| 屏显 开/关 | `SEND 4` |
| ECO 模式 | `SEND 5` |
| 学习开机/关机/制冷26/制冷23/屏显/ECO | `LEARN 0`~`LEARN 5` |

美的模拟码按钮已删除。每次发送后 App 会自动读取设备回复，手动“读取最近回复”时读取主主题 `infrared`，不要填写 `infrared/up`。

## 打包

需要 JDK 17、Android SDK（API 35）和 Gradle 8.10.2。先在本目录创建 `local.properties`，例如：

```properties
sdk.dir=C:/Android/Sdk
```

然后执行：

```powershell
gradle assembleDebug --no-daemon
```

生成的 APK：

```text
AndroidRemote\app\build\outputs\apk\debug\app-debug.apk
```

## 说明

UID 不写在源码里，只保存在手机本地 `SharedPreferences`。`release` 中的 APK 不包含任何用户 UID 或 Wi-Fi 配置。

当前联调结论：

- `STATUS`、`IR_TEST` 和 `SEND 0`~`SEND 5` 已验证正常，六个槽均为原装遥控器真实学习码。
- 米家使用独立主题 `infrared005`，原 App 继续使用 `infrared`，两者可以同时在线。
- OLED 显示 `IP 0.0.0.0` 不影响控制；以 `WiFi:OK Net:OK` 和命令执行结果为准。

更完整的安装、使用、环境路径说明见：

```text
..\docs\手机APK使用与打包.md
```
