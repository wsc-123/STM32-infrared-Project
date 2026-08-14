# STM32 红外空调网关

自用的美的冷静星空调红外网关项目：

```text
手机 APK / 米家 / 巴法云
  -> ESP-01S AT 固件
  -> STM32F103C8T6
  -> 红外 LED 发射 / 1838B 学习
  -> 空调
```

## 首次配置

仓库不包含 Wi-Fi 密码、巴法云 UID 或已按私有配置编译的固件。编译前先在仓库根目录执行：

```powershell
Copy-Item .\Firmware\App\app_config.example.h .\Firmware\App\app_config.h
```

然后编辑 `Firmware\App\app_config.h`，填写自己的 2.4GHz Wi-Fi 和巴法云 UID。这个本地配置文件已被 `.gitignore` 排除，不会被 `git add .` 加入提交。详细步骤见 `docs\ESP巴法云配置.md`。

## 当前状态（更新至 2026-08-04）

**全链路已实测可用**：手机 App 六个按钮 → 巴法云 → ESP-01S → STM32 → 红外发射 → 空调，指哪打哪。

- 六个学习槽全部学自原装遥控器并验证有效：

  | 槽 | 功能 | App 按钮 | 命令 |
  |----|------|----------|------|
  | 0 | 开机 | 开机 | `SEND 0` |
  | 1 | 关机 | 关机 | `SEND 1` |
  | 2 | 制冷 26° | 制冷 26° | `SEND 2` |
  | 3 | 制冷 23° | 制冷 23° | `SEND 3` |
  | 4 | 屏显切换 | 屏显 开/关 | `SEND 4` |
  | 5 | ECO 模式 | ECO 模式 | `SEND 5` |

- 学习码存 Flash（0x0800E000），断电不丢；App 上有 LEARN 0~5 重学按钮。
- App 发送后约 1.2 秒自动读取设备回复（读主 topic；固件回复用 `/up` 后缀发布，那是巴法云的"更新云端不推送"模式，不是独立主题）。
- 红外发射驱动已增强：限流 3×100Ω 并联（约 100mA 脉冲）+ 基极电阻减小，响应灵敏度接近原装遥控器。
- 美的模拟码（MIDEA_*）已从 App 移除，固件里保留但不再使用。

### 米家接入（2026-08-03 已实测）

- 固件通过同一条 TCP 连接订阅两个主题：`infrared` 供原 App 学习/控制，`infrared005` 供米家空调控制。
- 米家支持开机、关机、制冷 26°、制冷 23°和 ECO；其他温度、制热、风速与扫风因没有对应学习码会明确拒绝。
- 原 App 执行槽 0/1/2/3/5 后，也会用 `/up` 同步更新 `infrared005` 的标准空调状态。
- 米家 App 绑定入口：`我的 -> 其他平台设备 -> 添加 -> 巴法`，使用当前巴法云账号登录。
- 实测 `on#2#26` 可下发到 STM32，`infrared` 返回 `OK sent MI cool26 (299 edges)`，红外动作正常。
- 巴法设备属于第三方平台设备，不支持设置家庭/房间，通常不会作为原生设备卡片显示在米家首页；主要用于小爱语音控制。
- 红外是单向控制，米家显示的是最后一次成功发送的命令；使用原装遥控器后，界面状态可能与空调实际状态不同。

### 供电（洞洞板当前方案，2026-08-07）

- 最终采用 Blue Pill 的 USB 口作为唯一电源入口：USB 5V 从 Blue Pill 的 `5V` 引脚引出，供 Blue Pill 和红外 LED 发射回路使用；Blue Pill 的 `3.3V` 引脚供 ESP-01S、OLED、1838B 使用。
- 2026-08-07 USB 上电实测：`5V=5.15V`、`3.3V=3.32V`，空载电压正常。
- ESP-01S 的 VCC/GND 旁就近并 `220~470µF` 电解电容和 `100nF` 陶瓷电容；使用额定 `5V/1A` 或更高的充电头和短 USB 线。
- 之前的 MB102 独立 3.3V 方案保留为带载不稳定时的备用方案；不要把两个 3.3V 输出并联。
- 系统总功耗约 120~150mA，最终封板前还要在联网和连续红外发送时复测 3.3V 是否明显下降。

### 启动提速与稳定性（2026-08-04 已实测 ✅）

- 上电不再无条件硬复位 ESP：先 `AT` 探测（1s×3 次），有响应就直接沿用 ESP 当前状态；全部无响应才拉 PB0 硬复位（省 2.7s，且探测失败到硬复位比老逻辑快约 18s）。
- 连 WiFi 前先查 `AT+CWJAP?`：ESP 自动重连成功（或 STM32 单独重启、WiFi 本来没断）就跳过 `AT+CWJAP`，不再每次开机强制重新关联（省 4~8s，也避开关联时 300mA+ 的电流峰值）；等自动重连约 8 秒无果才主动 `CWJAP`。
- TCP 连接前先 `AT+CIPCLOSE` 清残留旧连接，根治订阅超时重试时 `CIPSTART` 一直回 `ALREADY CONNECTED` 导致连续失败 3 次被迫硬复位整轮重来的问题。
- 订阅巴法云的 `>` 提示符 / `SEND OK` 超时从 2s 放宽到 5s（`BEMFA_SUB_TIMEOUT_MS`），服务器偶尔慢不再引发整轮重连。
- `AT+CIFSR` 查到 `0.0.0.0` 时每 700ms 重查（最多 7 轮），修掉 OLED 一直显示 `IP 0.0.0.0` 的遗留问题。
- 开机顺带下发 `AT+CWAUTOCONN=1`（存 ESP Flash），保证自动重连始终开启。
- 预期效果：STM32 重刷固件/单独重启时约 2 秒内回到在线；整机冷启动约 5~8 秒；只关联一次 WiFi，降低供电压降导致 ESP 反复重启的概率。
- 注意：若启动时仍反复出现 `ESP:ERR Net:--`，那是 ESP 对 AT 无响应，属供电问题——在 ESP 的 VCC/GND 就近并 220~470µF 电解 + 100nF 瓷片电容，并用正经 5V/1A 充电头。

### ESP 在线健康检查（2026-08-04 已实测）

- 在线时每 `10s` 发送一次 `AT+CIPSTATUS`，不再把上一次的 `WiFi:OK Net:OK` 永久当成实时状态。
- ESP 无响应超过 `1.5s` 后进入错误状态；2026-08-04 实测拔掉 ESP 电源后 OLED 能正确改为 `ESP:ERR Net:--`。
- 每 `30s` 的巴法云心跳现在必须收到 `>` 和 `SEND OK`；WiFi 正常但 TCP 失效时显示 `WiFi:OK Net:ERR`。
- 恢复顺序为 TCP 重连（先 `AT+CIPCLOSE` 再重连）、WiFi 重连、连续进入联网错误 3 次后 PB0 硬复位；只要成功恢复到在线状态，连续失败计数就清零。两次故障硬复位至少间隔 `60s`。
- PB0/RST 只能复位仍有 3.3V 供电的 ESP。ESP 真正断电时固件只能正确显示离线并持续重试；2026-08-04 实测恢复供电后可自动回到 `WiFi:OK Net:OK`，无需重启 STM32。
- OLED 是 STM32 通过 `AT+CIPSTATUS` 得到的本地实时判断；ESP 突然断电时无法向巴法云发送 TCP 关闭通知，巴法云网页要等服务器心跳/连接超时后才会改为离线，因此可能比 OLED 晚几十秒到几分钟，属于正常现象。
- 2026-08-04 二次修正：状态机每一步都等应答的终止行（如 `CONNECT`+`OK` 同时在场）才前进。此前 `AT+CIPCLOSE` 应答里迟到的 `OK` 会残留到下一步，被 `CIPMUX`/`CIPSTART` 误当成自己的应答导致抢跑订阅，表现为开机 `WiFi:OK Net:ERR` 反复 1~2 轮才上线。订阅失败现在 OLED 显示 `WiFi:OK Sub:ERR`，与 TCP 失败（`Net:ERR`）区分。
- 红外载波占空比从 1/3 提到 1/2（`bsp_ir_tx.c`）：同样 100mA 峰值下平均辐射功率 +50%，接收头兼容，属免费增程。
- 当前 Keil 构建：Code `17580`、RO-data `2012`、RW-data `124`、ZI-data `11996`，`0 Error(s), 0 Warning(s)`。

### 待办（低优先级）

- 可选优化：OLED 无操作自动熄屏（延缓屏幕老化）。

### 看门狗（已修复验证 ✅）

IWDG 已启用（约 4 秒超时），死机自动重启。曾因初始化顺序错误（使能前死等 PVU/RVU 同步）导致启动卡死，已按 ST 标准顺序修复（`Firmware/Bsp/bsp_watchdog.c`）。验证命令 `WDG_TEST`：设备故意锁死，约 4 秒后看门狗复位，OLED 第一行显示 `WDG!` 指纹后自动恢复在线。

## 目录

| 目录/文件 | 说明 |
|-----------|------|
| `Firmware` | STM32 标准库固件源码 |
| `Project` | Keil MDK 工程文件 |
| `AndroidRemote` | Android 遥控器 APK 工程源码 |
| `docs` | Markdown 文档 |
| `release` | 预编译 APK |
| `Libraries` | STM32 标准外设库 + CMSIS |

## 快速使用

1. 手机安装 APK（覆盖安装保留配置）：
   ```text
   release\冷静星智控-美化版-v1.0.apk
   ```
2. APK 首次打开填写巴法云 UID，topic 填：
   ```text
   infrared
   ```
3. 「空调控制」区六个按钮直接用；某个码不灵时用「重新学习」区对应按钮重学（点按钮 → 遥控器凑近 1838B 按一次 → 点「读取最近回复」确认 edges 数）。
4. 米家控制需要巴法云中同时存在 `infrared005`，并在米家 App 的“其他平台设备”中绑定巴法账号。
5. 完成上面的配置后，在 Keil 中编译，生成并烧录：
   ```text
   Project\Objects\IR_Gateway.hex
   ```
   注意：烧录不会清掉已学习的码（存在 Flash 顶部 8KB）。

## 已做的稳健性处理

- 巴法云下发命令按 `+IPD` 整包长度解析，避免 `LED_ON` 被截成 `LE` 的半包问题。
- STM32 加入 IWDG 独立看门狗，主循环正常运行时持续喂狗，卡死后自动重启。
- ESP 在线时主动查询 WiFi/TCP 状态并确认心跳发送结果，可发现 ESP 断电和静默掉线。
- `esp_at_publish()` 回传状态时不再清空 USART1 环形接收缓冲，降低吞掉下行命令的概率。
- Android APK 设置 `allowBackup=false`，避免保存的巴法云 UID 被系统备份。

## 主要文档

| 文档 | 说明 |
|------|------|
| `Firmware\README.md` | 固件目录、引脚、命令和编译说明 |
| `docs\接线文档.md` | 硬件接线 |
| `docs\红外学习操作步骤.md` | 当前槽位说明及需要覆盖时的重学/回放步骤 |
| `docs\手机APK使用与打包.md` | APK 安装、环境路径、重新打包 |
| `docs\ESP巴法云配置.md` | ESP 和巴法云配置 |
| `docs\米家接入.md` | 双主题、米家绑定、命令映射和实测结果 |
| `docs\注意事项.md` | 供电、共地、Flash 等注意事项 |
| `docs\洞洞板移植.md` | 元件布局、逐点焊接、万用表检查和分段上电验收 |

## 重新编译

STM32 固件（Keil MDK 打开 `Project\IR_Gateway.uvprojx` 直接 Build，或命令行）：

```powershell
& 'C:\Keil\UV4\UV4.exe' -b 'Project\IR_Gateway.uvprojx'
```

Android APK（需要 JDK 17 + Android SDK + Gradle，并配置 `AndroidRemote\local.properties` 的 `sdk.dir`）：

```powershell
cd 'AndroidRemote'
& gradle assembleDebug --no-daemon
# 产物：AndroidRemote\app\build\outputs\apk\debug\app-debug.apk
```

> `Firmware\App\app_config.h` 是本地私有配置，不在仓库中。首次使用时从同目录的 `app_config.example.h` 复制生成。
