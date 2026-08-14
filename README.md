# STM32 红外空调网关

一个基于 STM32F103C8T6 的红外空调网关。它把手机 App、米家和巴法云的指令转成空调红外信号，也可以通过 1838B 接收头学习原装遥控器的完整按键帧。

```text
Android App / 米家
        |
      巴法云 TCP
        |
     ESP-01S AT
        |
   STM32F103C8T6
     /        \
  1838B      红外 LED
 (学习)       (发射)
```

## 功能

- 通过 ESP-01S 连接 Wi-Fi 和巴法云 TCP 服务。
- 学习、保存和回放空调原装遥控器的红外帧。
- 6 个学习槽，当前示例映射为开机、关机、制冷 26°C、制冷 23°C、屏显和 ECO。
- 学习数据保存到 STM32 内部 Flash，断电后仍然保留。
- 配套 Android 遥控器，支持状态查询和重新学习。
- 通过 `infrared005` 主题接入米家，支持项目中已有的空调状态映射。
- OLED 显示 Wi-Fi、TCP、学习和回放状态，IWDG 看门狗负责异常复位。

## 快速开始

### 1. 配置固件

仓库不包含任何真实 Wi-Fi 密码或巴法云 UID。编译前复制配置模板：

```powershell
Copy-Item Firmware/App/app_config.example.h Firmware/App/app_config.h
```

编辑 `Firmware/App/app_config.h`，填写：

```c
#define WIFI_SSID      "你的 2.4GHz WiFi 名称"
#define WIFI_PASS      "你的 WiFi 密码"
#define BEMFA_UID      "你的巴法云 UID"
#define BEMFA_TOPIC    "infrared"
#define BEMFA_MI_TOPIC "infrared005"
```

`app_config.h` 只保存在本地，已经被 `.gitignore` 排除，不能上传到公开仓库。

### 2. 编译和下载 STM32 固件

使用 Keil MDK 打开 `Project/IR_Gateway.uvprojx`，执行 Build，然后下载到 STM32F103C8T6。也可以使用 Keil 命令行：

```powershell
& 'C:\Keil\UV4\UV4.exe' -b 'Project\IR_Gateway.uvprojx'
```

编译输出位于 `Project/Objects/`，这些文件属于本机构建产物，不提交到仓库。

### 3. 安装 Android App

可以直接安装 `release/冷静星智控-美化版-v1.0.apk`，或者在 `AndroidRemote` 目录重新构建：

```powershell
gradle assembleDebug --no-daemon
```

首次启动时填写巴法云 UID，主题使用 `infrared`。UID 只保存在手机本地，不写入源码。

## 首次使用流程

1. 按 `docs/接线文档.md` 接好 STM32、ESP-01S、1838B、OLED 和红外发射电路，并确认 ESP 使用 3.3V、所有模块共地。
2. 配置 `app_config.h`，用 Keil 编译并下载固件。
3. 安装 APK，填写同一个巴法云 UID，主题填写 `infrared`。
4. 在 App 的学习区执行 `LEARN 0` 到 `LEARN 5`，每次将原装遥控器对准 1838B 按下对应按键，等待提示保存成功。
5. 在 App 的控制区发送 `SEND 0` 到 `SEND 5`，确认空调能正常响应。米家控制还需要在巴法云中创建 `infrared005` 主题并完成绑定。

## 指令和学习槽

维护命令发送到 `infrared` 主题：

| 命令 | 作用 |
| --- | --- |
| `SEND 0` ... `SEND 5` | 回放对应学习槽 |
| `LEARN 0` ... `LEARN 5` | 学习并保存一帧红外码 |
| `STATUS` | 查询联网状态和已学习槽位 |
| `CLEAR 0` ... `CLEAR 5` | 清除对应学习槽 |
| `HELP` | 查看固件支持的命令 |

当前示例槽位：

| 槽位 | 功能 |
| --- | --- |
| 0 | 开机 |
| 1 | 关机 |
| 2 | 制冷 26°C |
| 3 | 制冷 23°C |
| 4 | 屏显切换 |
| 5 | ECO |

空调红外协议和按键内容因机型而异。换用其他空调时，应重新学习对应的完整按键帧。

## 引脚速查

| 功能 | STM32 引脚 | 说明 |
| --- | --- | --- |
| 板载 LED | PC13 | 低电平点亮 |
| ESP-01S 串口 | PA9 / PA10 | USART1，115200；TX/RX 交叉连接 |
| ESP 复位 | PB0 | 开漏输出，RST 通过 10kΩ 上拉到 3.3V |
| 调试串口 | PA2 / PA3 | USART2，115200 |
| 红外接收 | PA0 | 1838B OUT |
| 红外发射 | PA6 | TIM3 载波，经三极管驱动红外 LED |
| OLED | PB8 / PB9 | I2C |

ESP-01S 必须使用稳定的 3.3V 供电，所有模块共地；红外 LED 侧使用限流电阻，不能直接由 STM32 GPIO 驱动大电流。

## 目录

| 路径 | 内容 |
| --- | --- |
| `Firmware/` | STM32 应用、板级驱动和启动代码 |
| `Libraries/` | CMSIS 与 STM32F10x 标准外设库 |
| `Project/` | Keil MDK 工程文件 |
| `AndroidRemote/` | Android 遥控器工程 |
| `docs/` | 接线、学习、巴法云和米家说明 |
| `release/` | 已检查的 Android APK |

建议阅读顺序：

1. `docs/ESP巴法云配置.md`
2. `docs/接线文档.md`
3. `docs/操作步骤.md`
4. `docs/红外学习操作步骤.md`

第三方库的版权和上游说明见 `THIRD_PARTY_NOTICES.md`。项目当前没有提交真实凭据，也不发布按个人配置编译的 `.hex` 固件。
