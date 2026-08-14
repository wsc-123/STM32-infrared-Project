# ESP-01S 巴法云 TCP 配置说明

这份文档讲清楚：怎么让 STM32 通过 ESP-01S 连上巴法云，实现**人在任何地方用手机控制空调**。

整条链路：

```text
手机 App ──发指令──> 巴法云(公网 TCP) ──推送──> ESP-01S ──串口──> STM32 ──红外──> 空调
```

关键点：本方案用巴法云的 **TCP 协议**（不是 MQTT），你现有的 ESP-01S **出厂 AT 固件（v1.5.4.1）就够用，不用重烧固件**。

---

## 一、为什么用 TCP 而不是 MQTT

你的 ESP-01S 是安信可 2017 年的出厂 AT 固件 v1.5.4.1，这个版本**没有 `AT+MQTT*` 系列指令**（那是乐鑫 ESP-AT v2.1.0 以后才有的）。

巴法云同时提供 MQTT 和 TCP 两种接入，**主题系统是同一套**、手机 App 用法完全一样。所以我们走 TCP：STM32 用 `AT+CIPSTART` 建一条到 `bemfa.com:8344` 的 TCP 连接，用简单的文本协议订阅主题、收指令、发心跳。效果和 MQTT 一致，但你不用动 ESP 固件。

---

## 二、注册巴法云、拿私钥、建主题

1. 打开 <https://cloud.bemfa.com> 注册并登录。
2. 进入控制台，找到 **TCP 设备云** → **私钥（uid）**，复制那串（约 32 位）。
3. 在 TCP 设备云下建立两个主题：
   - `infrared`：原 Android App、学习和诊断，对应固件 `BEMFA_TOPIC`。
   - `infrared005`：米家空调标准消息，对应固件 `BEMFA_MI_TOPIC`；`005` 后缀不能改。

> 巴法云的具体页面/入口可能随版本调整，以官网当前界面为准。核心就两样东西：**私钥** 和 **主题名**。

---

## 三、把私钥/密码填进固件（只改一个文件）

先复制配置模板：

```powershell
Copy-Item .\Firmware\App\app_config.example.h .\Firmware\App\app_config.h
```

然后打开新生成的 `Firmware/App/app_config.h`，改这几项：

```c
#define WIFI_SSID     "YOUR_WIFI_SSID"      // 2.4GHz，ESP8266 不支持 5G
#define WIFI_PASS     "YOUR_WIFI_PASSWORD"

#define BEMFA_UID     "YOUR_BEMFA_UID"       // 第二步复制的私钥
#define BEMFA_TOPIC      "infrared"            // 原 App / 学习 / 诊断
#define BEMFA_MI_TOPIC   "infrared005"         // 米家空调，必须以 005 结尾
```

其余（`BEMFA_HOST`=bemfa.com、`BEMFA_PORT`=8344）一般不用改。建议 WiFi 名、密码、主题名和私钥使用 ASCII 字符；ARMCC V5 对 UTF-8 字符串字面量支持不稳定。

改完在 Keil 里重新 Build、下载即可。**WiFi 密码和私钥只存在这一个头文件里。**

> ⚠️ 私钥相当于你账号的密码，别把填好的 `app_config.h` 上传到公开的 GitHub 等地方。

---

## 四、手机端怎么发指令

两种方式，任选：

### 方式 A：巴法云官方 App / 小程序

1. 手机装「巴法云」App（或用其微信小程序），登录同一个账号。
2. 进到主题 `infrared`。
3. 用「发送消息」把命令文本发出去，比如发 `SEND 0`。
4. STM32 收到后回放对应红外，空调动作。

### 方式 B：任意 MQTT/TCP 工具或自建网页

巴法云也支持 HTTP/MQTT 接口，可以用它的 API 从网页或自动化脚本发消息。维护命令发到 `infrared`；米家标准空调命令由 `infrared005` 接收，具体映射见 `米家接入.md`。

### 支持的命令（发到主题的文本）

| 命令 | 作用 |
|------|------|
| `LED_ON` / `LED_OFF` / `LED_TOGGLE` | 板载灯自测 |
| `LEARN 0` … `LEARN 5` | 学习：随后按空调遥控，整帧存入该槽 |
| `SEND 0` … `SEND 5` | 回放某槽 |
| `STATUS` | 查询联网状态、IP 和哪些槽已学习，会回传到 `<主题>/up` |
| `SAVE` | 手动保存当前槽位到内部 Flash |
| `CLEAR 0` … `CLEAR 5` | 清空某个槽位并保存 |
| `POWER_ON` `POWER_OFF` `COOL_26` `COOL_25` `HEAT_28` | 旧命名兼容命令；推荐用 `SEND n`，避免槽 3/4 名称与实际内容不符 |
| `HELP` / `DUMP` | 帮助 / 打印最近学到的时序（调试串口看）|

当前实际槽位：0 开机、1 关机、2 制冷26°、3 制冷23°、4 屏显切换、5 ECO。日常使用 `SEND 0`~`SEND 5` 回放。

> 注意：**学习（LEARN）通常在家里当面做**——要拿原装遥控对着 1838B 按。学习成功后会自动保存到内部 Flash，
> 之后断电重上电也能直接回放。人在外地时可先发 `STATUS` 确认 `SLOTS` 里有对应槽位，再发 `POWER_ON` 等回放命令。

---

## 五、OLED 上能看到什么

STM32 上电后 OLED（PB8/PB9）四行显示：

```text
第1行  IR AirCon OK          设备名 + 状态(boot/OK)
第2行  WiFi:OK Net:OK        WiFi 和巴法云连接状态
第3行  IP 192.168.x.x        ESP 拿到的局域网 IP
第4行  OK sent POWER_ON      最近一次学习/回放结果
```

第2行状态含义：

| 显示 | 含义 |
|------|------|
| `WiFi:.. Net:..` | 刚上电，正在初始化 ESP |
| `WiFi:...` | 正在连路由器 |
| `WiFi:OK Net:..` | WiFi 连上，正在连巴法云 |
| `WiFi:OK Net:OK` | **全部就绪，可以远程控制了** |
| `ESP:ERR Net:--` | ESP 无 AT 应答，可能断电或 RST/串口异常 |
| `WiFi:ERR Net:--` | ESP 在线，但没有连接路由器 |
| `WiFi:OK Net:ERR` | WiFi 在线，但巴法云 TCP/订阅失败 |

固件每 10 秒主动执行一次 `AT+CIPSTATUS`，每 30 秒发送一次需确认 `SEND OK` 的心跳。2026-08-04 实测：拔掉 ESP 电源后 OLED 能正确离开 `WiFi:OK Net:OK` 并显示 `ESP:ERR Net:--`；重新接通电源后无需重启 STM32即可自动恢复连接。RST 不能给断电的 ESP 供电。

OLED 与巴法云网页显示的在线状态不是同一个检测源。OLED 是 STM32 每 10 秒在本地主动询问 ESP；巴法云显示则依赖服务器看到 TCP 连接关闭或等到心跳/TCP 超时。ESP 突然断电时无法发送正常的 TCP `FIN`/关闭通知，服务器会暂时保留半开连接，加上网页刷新延迟，巴法云可能比 OLED 晚几十秒到几分钟才显示离线。这属于正常的服务器端判定延迟，不表示固件仍然可用或健康检查失败。

---

## 六、上电到能控制的完整流程

1. `app_config.h` 填好 WiFi + 私钥，确认 `infrared` / `infrared005` 两个主题，Build 下载。
2. 接好 ESP-01S（VCC 3.3V、去耦电容、TX/RX 交叉、共地）——见接线文档。
3. 上电，看 OLED：
   - 第2行走到 `WiFi:OK Net:OK`，第3行显示到 IP → **联网成功**。
   - 调试串口（USART2, 115200）会同步打印 `[net] ...` 状态。
4. 当面学习：手机发 `LEARN 0`，拿空调遥控对着 1838B 按“开机”，OLED 第4行显示 `OK learned slot 0 ... (saved)`。
5. 可选验证：断电重上电后发 `STATUS`，看到 `SLOTS` 中含 `0`。
6. 远程回放：人走到任何地方，手机发 `POWER_ON`，空调开机。

---

## 七、连不上时怎么排查

按 OLED 第2行卡在哪一步来定位：

| 卡在 | 可能原因 | 检查 |
|------|---------|------|
| `WiFi:...` 一直不动 | WiFi 名/密码错、填了 5G 频段、信号弱 | ESP8266 只连 **2.4GHz**；确认 SSID/密码；靠近路由 |
| `WiFi:OK` 但 `Net` 上不去 | 私钥错、主题没建、网络出不去公网 | 核对 `BEMFA_UID`；确认两个主题都已建立；换个网络试 |
| `ESP:ERR Net:--` 反复 | ESP 断电/供电不足、RST 异常、TX/RX 接反 | ESP 用独立 3.3V(≥500mA)、加去耦电容；检查 RST 和 TX/RX |
| `WiFi:ERR Net:--` 反复 | 路由器不可用、SSID/密码或2.4GHz配置错误 | 检查路由器和 WiFi 配置 |
| `WiFi:OK Net:ERR` 反复 | 巴法云 TCP、私钥或主题异常 | 检查网络出口、UID、`infrared` 和 `infrared005` |
| 联网 OK 但发命令没反应 | 主题名不一致、命令拼写错 | 原 App 用 `infrared`，米家用 `infrared005`；命令区分大小写 |
| 空调不响应但学习/回放都 OK | 载波频率或学习质量问题 | 见「注意事项」红外部分；重学、缩短学习距离 |

调试串口（USART2 接 USB-TTL，115200 8N1）能看到每一步 AT 交互日志，是排查联网问题最有力的工具。

---

## 八、已知限制

- 学习数据已保存到内部 Flash 顶部 8KB，掉电后自动恢复；若后续固件明显增大，需要复核 Flash 数据区和程序区是否重叠。
- 出厂 AT 固件走 TCP，稳定性依赖 ESP 供电质量，务必给足电流 + 去耦电容。
- 巴法云免费版对消息频率/主题数有额度限制，家用足够；具体以官网为准。
- 只做整帧录制回放，不解析空调协议，学一个**固定状态**（开机/关机/制冷26）最稳。
