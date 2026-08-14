# 手机 APK 使用与打包说明

这份文档记录“冷静星遥控器”Android APK 的用途、安装方法、当前电脑上的打包环境，以及以后如何重新生成 APK。

当前 APK 已生成：

```text
release\冷静星智控-美化版-v1.0.apk
```

> **✅ 2026-07-10 界面改版**：主界面为三个区——
> **「空调控制」**六个按钮（开机/关机/制冷26°/制冷23°/屏显开关/ECO，分别发 `SEND 0`~`SEND 5`）；
> **「调试」**（红外测试/查询状态/读取最近回复）；
> **「重新学习」**（LEARN 0~5，重学时点按钮后按遥控器，再点"读取最近回复"确认）。
> 每次发送约 1.2 秒后自动读取并显示设备的真实回复（如 `OK sent SEND 2 (299 edges)`），
> 形成"发送→确认"闭环。旧版的美的模拟码按钮已移除。

---

## 1. APK 是什么

APK 是 Android 手机的软件安装包，不是电脑环境。

关系如下：

```text
JDK / Android SDK / Gradle  ->  电脑上的打包环境
APK                         ->  打包出来的手机安装包
手机 App                    ->  APK 安装到手机后的程序
```

本项目的 APK 安装后，手机上会出现：

```text
冷静星遥控器
```

它通过巴法云给 `infrared` 主题发送命令，家里的 ESP-01S 收到后转给 STM32，STM32 再执行红外发射。

---

## 2. 控制链路

只要手机联网，并且家里的 STM32 + ESP-01S 已经连上巴法云，就可以异地控制。

```text
手机 App
  -> 巴法云 HTTP 接口
  -> topic: infrared
  -> ESP-01S TCP 长连接
  -> STM32 串口命令解析
  -> 红外 LED 发射
  -> 空调
```

手机不需要和 ESP-01S 在同一个 WiFi。人在外面用 4G/5G 也可以。

前提是 OLED 或日志显示：

```text
WiFi:OK Net:OK
```

---

## 3. 手机安装方法

1. 把下面这个 APK 发到手机：
   ```text
   release\冷静星智控-美化版-v1.0.apk
   ```
2. 在手机文件管理器或微信/QQ 下载目录里点开 APK。
3. 如果提示“禁止安装未知来源应用”，给当前应用临时授权。
4. 安装完成后打开 `冷静星遥控器`。
5. 首次打开填写：
   ```text
   UID / 私钥：你的巴法云私钥
   topic：infrared
   ```
6. 点 `保存配置`。
7. 先点 `查询状态` 或 `红外测试` 验证链路。

> 这个 APK 是 debug 版，适合自己测试使用，不建议公开发给别人。

---

## 4. App 按钮对应命令

| App 按钮 | 发送给 STM32 的命令 | 说明 |
|----------|---------------------|------|
| 红外测试 | `IR_TEST` | 不控制空调，只测试红外 LED 是否发光 |
| 查询状态 | `STATUS` | 查询联网状态和学习槽位 |
| 读取最近消息 | 不发送命令 | 从巴法云读取 topic 最近一条消息 |
| 开机 | `SEND 0` | 回放已学习的开机状态 |
| 关机 | `SEND 1` | 回放已学习的关机状态 |
| 制冷 26° | `SEND 2` | 回放已学习的制冷 26° 状态 |
| 制冷 23° | `SEND 3` | 回放已学习的制冷 23° 状态 |
| 屏显 开/关 | `SEND 4` | 每发送一次切换一次屏显 |
| ECO 模式 | `SEND 5` | 回放已学习的 ECO 状态 |
| 学习槽 0~5 | `LEARN 0`~`LEARN 5` | 仅在需要覆盖对应槽位时使用 |

旧版美的模拟码按钮已经移除。固件仍可能保留部分兼容命令，但 App 操作和日常控制统一使用 `SEND 0~5`，避免旧命令名与槽内实际内容不一致。

---

## 5. 当前电脑上的环境路径

已经安装或创建的路径如下：

| 路径 | 作用 | 说明 |
|------|------|------|
| `C:\Android SDK` | Android SDK + JDK 17 | 当前 JDK 也装在这个根目录 |
| `C:\Android SDK\bin\java.exe` | Java 运行环境 | JDK 17 |
| `C:\Android SDK\bin\javac.exe` | Java 编译器 | JDK 17 |
| `C:\Android SDK\cmdline-tools\latest` | Android SDK 命令行工具 | 含 `sdkmanager.bat` |
| `C:\Android SDK\platform-tools` | Android 平台工具 | 含 `adb.exe` |
| `C:\Android SDK\platforms\android-35` | Android 35 平台 API | 编译用 |
| `C:\Android SDK\build-tools\35.0.0` | Android Build Tools 35 | 编译/签名用 |
| `C:\Android SDK\build-tools\34.0.0` | Android Build Tools 34 | Gradle 自动补装 |
| `C:\Gradle\gradle-8.10.2` | Gradle 构建工具 | 用来运行 `assembleDebug` |
| `AndroidRemote` | Android App 源码工程 | 当前 APK 项目 |
| `%USERPROFILE%\.gradle` | Gradle 依赖缓存 | 自动生成 |
| `%USERPROFILE%\.android\debug.keystore` | debug 签名证书 | 自动生成 |

注意：`C:\Android SDK` 现在同时放了 JDK 和 Android SDK，不要只看名字就删除里面的 `bin`、`lib`、`jmods`、`conf` 等目录。

---

## 6. 已下载的安装包

| 路径 | 说明 |
|------|------|
| `%USERPROFILE%\Downloads\commandlinetools-win-14742923_latest.zip` | Android SDK command-line tools 原始 zip |
| `%USERPROFILE%\Downloads\gradle-8.10.2-bin.zip` | Gradle 原始 zip |

这些 zip 不是运行必需文件。环境已经解压好后，可以留作备份。

---

## 7. 重新打包 APK

以后修改 App 源码后，在 PowerShell 执行：

```powershell
$env:JAVA_HOME='C:\Android SDK'
$env:ANDROID_HOME='C:\Android SDK'
$env:ANDROID_SDK_ROOT='C:\Android SDK'
$env:PATH='C:\Android SDK\bin;C:\Android SDK\platform-tools;C:\Gradle\gradle-8.10.2\bin;' + $env:PATH
& 'C:\Gradle\gradle-8.10.2\bin\gradle.bat' assembleDebug --no-daemon
```

执行目录必须是：

```text
AndroidRemote
```

打包成功后会看到：

```text
BUILD SUCCESSFUL
```

新的 APK 仍然在：

```text
AndroidRemote\app\build\outputs\apk\debug\app-debug.apk
```

---

## 8. 当前 App 工程文件

| 文件 | 作用 |
|------|------|
| `AndroidRemote\settings.gradle` | Gradle 仓库和模块配置 |
| `AndroidRemote\build.gradle` | Android Gradle 插件版本 |
| `AndroidRemote\local.properties` | 指向 `C:\Android SDK` |
| `AndroidRemote\app\build.gradle` | App 编译参数 |
| `AndroidRemote\app\src\main\AndroidManifest.xml` | App 权限和入口 |
| `AndroidRemote\app\src\main\java\com\stm32infrared\remote\MainActivity.java` | 主界面和按钮逻辑 |
| `AndroidRemote\app\src\main\java\com\stm32infrared\remote\BemfaClient.java` | 巴法云 HTTP 请求 |

源码里没有写死巴法云 UID。UID 只会保存在手机本地。
Manifest 已设置 `android:allowBackup="false"`，避免 UID/topic 被 Android 系统备份。

---

## 9. 常见问题

| 现象 | 处理 |
|------|------|
| 手机提示未知来源 | 允许当前文件管理器/微信安装一次 |
| App 发命令失败 | 检查手机网络、UID、topic 是否正确 |
| App 显示发送成功但 STM32 没反应 | 看 OLED 是否 `WiFi:OK Net:OK`，检查 ESP-01S 是否在线 |
| `SEND n` 返回空槽 | 对应槽位数据丢失或被清空，用 `LEARN n` 重新学习 |
| App 显示 `OK sent` 但空调没动作 | 检查红外 LED、S8050、供电、共地、方向和距离 |
| 重新打包提示找不到 Java | 先设置 `$env:JAVA_HOME='C:\Android SDK'` |
| 重新打包提示找不到 SDK | 确认 `local.properties` 里是 `sdk.dir=C\:\\Android SDK` |

---

## 10. 当前联调记录

当前 APK / 巴法云 / ESP-01S / STM32 命令链路已验证可用：

| App 或巴法云命令 | 结果 |
|------------------|------|
| `LED_ON` | 正常 |
| `LED_OFF` | 正常 |
| `STATUS` | 正常 |
| `IR_TEST` | 正常打印 |
| `SEND 0`~`SEND 5` | 六个真实学习槽均已回放成功 |
| `infrared005` 的 `on#2#26` | 返回 `OK sent MI cool26 (299 edges)`，空调动作正常 |

已修复固件中的半包解析问题：巴法云下发 `LED_ON` 时，ESP 串口可能先只收到 `LE`，旧固件会提前执行并显示 `ERR cmd LE`。当前固件会等 `+IPD` 整包收齐后再解析。

OLED 仍可能显示：

```text
IP 0.0.0.0
```

这不影响 App 控制。只要 `WiFi:OK Net:OK` 且命令能正常返回，说明设备在线。

美的模拟码不匹配当前空调/遥控器，已停止使用。当前 App 和米家映射都回放 1838B 从原装遥控器学到的真实码。

---

## 11. 后续建议

1. 日常先用当前 debug APK 的 `查询状态` 和六个场景按钮测试。
2. 只有槽位需要覆盖时，才点对应的 `重新学习` 按钮并按原装遥控器。
3. 米家/小爱控制和绑定说明见 `docs\米家接入.md`；原 App 仍使用 `infrared` 主题。
4. 如果 App 长期稳定可用，再考虑制作 release 签名版 APK。
