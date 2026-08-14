# 发布文件

此目录只存放已经检查、可直接下载使用的发布包。

- `冷静星智控-美化版-v1.0.apk`：Android 调试 APK。首次打开时填写自己的巴法云 UID 和主题，不包含 Wi-Fi 密码或巴法云 UID。

固件 `.hex` 不随仓库发布：它会嵌入每位使用者自己的 Wi-Fi 和巴法云配置。请从源码创建 `Firmware/App/app_config.h` 后在 Keil 中自行编译。
