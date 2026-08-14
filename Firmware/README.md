# STM32 红外空调网关 · 固件

配套硬件与操作步骤见仓库根目录 `README.md` 及 `../docs/`。
本工程是**完整可编译工程**：应用层源码（标准外设库风格）+ CMSIS/SPL 库本体 + 启动文件 +
配好的 Keil 工程文件，一应俱全。直接用 Keil 打开 `../Project/IR_Gateway.uvprojx`，
先按根目录说明创建 `App/app_config.h`，再 Build 即可，无需自己搭模板。

## 目录结构

```
Firmware/
├── User/                 用户层
│   ├── main.c            主程序（初始化 + 主循环）
│   ├── stm32f10x_conf.h  标准库外设裁剪配置
│   ├── stm32f10x_it.c    内核异常处理
│   └── stm32f10x_it.h
├── Bsp/                  板级驱动
│   ├── bsp_delay.c/.h    DWT 微秒延时 + SysTick 毫秒时基
│   ├── bsp_led.c/.h      PC13 状态灯
│   ├── bsp_usart.c/.h    USART1=ESP通信 / USART2=调试printf
│   ├── bsp_ir_tx.c/.h    TIM3 38kHz 载波 + 发送原始帧
│   ├── bsp_ir_rx.c/.h    EXTI0 捕获 1838B，学习原始帧
│   ├── ir_codes.c/.h     学习帧的槽位存储（RAM + Flash 持久化）
│   ├── bsp_flash.c/.h    内部 Flash 数据区擦写（顶部 8KB）
└── App/
    └── cmd_parser.c/.h   ESP 文本命令 -> 动作，学习/回放编排
docs/
├── 接线文档.md
├── 米家接入.md
├── 操作步骤.md
└── 注意事项.md
```

## 引脚分配（STM32F103C8T6，72MHz）

| 功能 | 引脚 | 外设 | 说明 |
|------|------|------|------|
| 状态灯 | PC13 | GPIO | 板载灯，低电平点亮 |
| ESP-01S | PA9/PA10 | USART1 | TX->ESP RX, RX<-ESP TX, 115200 |
| ESP复位 | PB0 | GPIO开漏输出 | 连接ESP RST，RST另经10kΩ上拉到3.3V |
| 调试串口 | PA2/PA3 | USART2 | 接 USB-TTL 看 printf, 115200 |
| 红外接收 | PA0 | EXTI0 | 1838B OUT，双边沿中断 |
| 红外发射 | PA6 | TIM3_CH1 | 38kHz PWM -> 1k -> S8050 基极 |

## 在 Keil MDK 里打开编译

工程已经配好，直接用即可：

1. 双击 `../Project/IR_Gateway.uvprojx` 用 Keil MDK5 打开。
2. 直接 **Build（F7）**，应为 0 错误 0 警告。
3. ST-Link 连好，**Download（F8）** 下载。
4. 接调试串口（PA2 → USB-TTL RX），串口助手 115200 8N1，复位。
   上电应打印 `==== STM32 IR Gateway (bemfa TCP) ====`，并显示已恢复的红外槽位数量，板载灯（PC13）闪 3 下。

工程里已经配好的关键选项（无需手动改）：

- **器件**：STM32F103C8（20KB RAM / 64KB Flash）。
- **IROM1**：`0x08000000` 起 `0x0E000`（56KB）；顶部 8KB 保留给红外码 Flash 存档。
- **宏定义**：`STM32F10X_MD, USE_STDPERIPH_DRIVER`。
- **C99**：已启用（源码有 `for (int i...)` 这类 C99 写法，不开会报错）。
- **Use MicroLIB**：已勾选（`printf`/`fputc` 重定向到调试串口必须）。
- **Include 路径**：`User`、`Bsp`、`App`、`CMSIS`、`StdPeriph_Driver/inc` 都已加。
- **SPL 源文件**：已加入 `rcc/gpio/usart/tim/exti/flash.c` + `misc.c` + 启动文件。

> ⚠️ 前提：Keil 需安装 **STM32F1 器件包**（`Keil.STM32F1xx_DFP`）。
> 打开若提示缺器件，装一下 STM32F1 的 DFP 即可（跑过 ESP 点灯工程的话通常已装）。
>
> 说明：中断函数 `EXTI0_IRQHandler` 在 `bsp_ir_rx.c`、`USART1_IRQHandler` 在
> `bsp_usart.c`；本工程的 `stm32f10x_it.c` 里没有这两个空函数，不会重复定义。

## 命令一览（手机/串口发给 STM32，行尾带回车）

| 命令 | 作用 |
|------|------|
| `HELP` | 打印命令列表 |
| `LED_ON` / `LED_OFF` / `LED_TOGGLE` | 里程碑 A：点灯自测 |
| `LEARN 0` … `LEARN 5` | 学习：随后按空调遥控，整帧存入该槽 |
| `SEND 0` … `SEND 5` | 回放某槽 |
| `DUMP` | 打印最近捕获的一帧时序（调试串口）|
| `STATUS` | 回传联网状态、IP 和已学习槽位，如 `SLOTS 01--4-` |
| `SAVE` | 手动把当前 RAM 槽位保存到内部 Flash |
| `CLEAR 0` … `CLEAR 5` | 清空某个槽位并保存 |
| `IR_TEST` | 不用学习码，直接让红外 LED 发 38kHz 载波闪烁，手机摄像头可见 |
| `MIDEA_*` / `MIDEA_F_*` | 已停用的协议猜测码，仅为旧版本兼容保留，不建议再试 |
| `POWER_ON` `POWER_OFF` `COOL_26` `COOL_25` `HEAT_28` | 旧命名回放命令；槽 3/4 名称与实际学习内容不符，推荐使用 `SEND n` |
| `WDG_TEST` | 故意锁死主循环验证看门狗：约 4 秒后 IWDG 自动复位，OLED 第一行显示 `WDG!` 指纹后自动恢复，学习码不受影响 |

命名命令与槽位映射：POWER_ON=0, POWER_OFF=1, COOL_26=2, COOL_25=3, HEAT_28=4。
**注意（2026-07-10）**：实际学习内容为 0=开机、1=关机、2=制冷26、3=制冷23、4=屏显切换、5=ECO，
槽 3/4 的老命令名与实际内容已不符，推荐统一用 `SEND n`（手机 App 按钮即此方式）。

`MIDEA_*` 是早期临时模拟发射，不占用学习槽位，但已实测不匹配当前空调。1838B 已完成六槽真实学习，当前 App 和米家映射都使用 `LEARN/SEND` 槽位，不要再用模拟码排错。

## 米家双主题接入

固件在一条巴法云 TCP 连接上同时订阅：

- `infrared`：原 Android App 的 `SEND/LEARN/STATUS` 等命令与诊断回复。
- `infrared005`：巴法云同步到米家的空调主题，只使用标准空调状态消息。

| 米家消息 | 红外槽位 | 回传到 `infrared005/up` |
|----------|----------|--------------------------|
| `on` | 0 开机 | `on` |
| `off` | 1 关机 | `off` |
| `on#2` / `on#2#26` | 2 制冷 26° | `on#2#26` |
| `on#2#23` | 3 制冷 23° | `on#2#23` |
| `on#7` | 5 ECO | `on#7` |

其他模式或温度不会发射近似红外码。详细错误仍回传到 `infrared/up`，米家主题只保留标准状态。

2026-08-03 已实测：两个主题均显示订阅者在线，`infrared005` 下发 `on#2#26` 后，
`infrared` 返回 `OK sent MI cool26 (299 edges)`，红外动作正常。巴法云设备属于米家第三方平台设备，
不支持分配家庭/房间，通常不显示为米家首页的原生设备卡片。

## ESP 在线健康检查与恢复

旧固件进入 `ESP_ST_ONLINE` 后只等待 ESP 主动上报 `CLOSED/ERROR`。ESP 直接断电时不会上报，OLED 会错误地一直保留 `WiFi:OK Net:OK`。当前固件增加了主动检查：

- 每 `ESP_HEALTH_CHECK_MS=10000ms` 非阻塞发送 `AT+CIPSTATUS`。
- `STATUS:3` 表示 TCP 在线；`STATUS:2/4` 进入 TCP 重连；`STATUS:5` 进入 WiFi 重连。
- `ESP_HEALTH_TIMEOUT_MS=1500ms` 内没有 AT 应答，判定 ESP 无响应或断电。
- 每 `BEMFA_PING_MS=30000ms` 发送心跳，必须依次收到 CIPSEND 的 `>` 和 `SEND OK`。
- 完整 App/米家下行消息本身也视为链路健康，并优先交付，避免健康检查与 `+IPD` 互相干扰。
- TCP 失败只重连 TCP，WiFi 失败从 `CWJAP` 重连，AT 无响应从基础 AT 检测重来。
- 每次进入联网错误状态都会把连续失败计数加 `1`；成功订阅并恢复到在线状态后计数清零。连续失败达到 `3` 次才通过 PB0 硬复位 ESP；故障硬复位之间有 `60s` 冷却，断电时不会高频拉低 RST。

OLED 错误显示：

| 显示 | 含义 |
|------|------|
| `ESP:ERR Net:--` | ESP 无 AT 应答，可能断电、RST 被拉低或串口异常 |
| `WiFi:ERR Net:--` | ESP 正常应答，但没有连接路由器 |
| `WiFi:OK Net:ERR` | WiFi 正常，巴法云 TCP 或订阅异常 |

PB0/RST 不能给 ESP 供电。2026-08-04 已完成硬件拔电/恢复测试：ESP 断电后 OLED 能正确退出假在线状态并显示 `ESP:ERR Net:--`，重新接通 3.3V 后无需重启 STM32即可自动回到 `WiFi:OK Net:OK`。该版本已由 Keil ARMCC V5 编译通过，`0 Error(s), 0 Warning(s)`。

OLED 与巴法云网页的离线时间不会完全同步。OLED 来自 STM32 每 10 秒执行的本地主动探测；ESP 突然断电时来不及向服务器发送 TCP `FIN`/关闭通知，巴法云只能等心跳或 TCP 超时清理这条半开连接，网页本身也可能有刷新延迟。因此巴法云晚几十秒到几分钟显示离线是正常现象，不代表健康检查失效。

## 已知限制 / 后续可做

- 学习帧会保存到内部 Flash 顶部 8KB（`0x0800E000` 起），掉电后自动恢复。工程已用 `IR_Gateway.sct` 把程序区限制在前 56KB。
- 已加入 IWDG 独立看门狗，主循环正常运行时持续喂狗；若固件卡死，约数秒后自动复位。
- 巴法云下发按 `+IPD` 整包长度解析，避免半包命令被提前执行；状态回传时不再清空 USART1 环形接收缓冲，降低吞下行命令的概率。
- 只做**整帧录制回放**，不解析空调协议、不动态生成温度/模式帧。
- 红外链路没有空调状态回读；米家展示的是最后一次成功发送的命令，不是物理空调的实时状态。
- 载波固定 38kHz、占空比 1/2（2026-08-04 从 1/3 提高以增强发射功率）；个别品牌用 36/40kHz，回放不灵时可在
  `bsp_ir_tx.c` 调 `IR_TIM_ARR`。
- ESP-01S 使用出厂 AT 固件，STM32 负责 `CIPSTART/CIPSEND/CIPSTATUS` 和巴法云 TCP 协议；不要刷成只做自定义串口透传且不响应 AT 的固件。
