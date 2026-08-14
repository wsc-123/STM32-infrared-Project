package com.stm32infrared.remote;

import android.app.Activity;
import android.content.SharedPreferences;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.RippleDrawable;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.text.InputType;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class MainActivity extends Activity {
    private static final String PREFS = "bemfa_config";
    private static final String KEY_UID = "uid";
    private static final String KEY_TOPIC = "topic";
    private static final String DEFAULT_TOPIC = "infrared";

    private static final int NAVY = Color.rgb(15, 35, 67);
    private static final int BLUE = Color.rgb(42, 111, 239);
    private static final int BLUE_DARK = Color.rgb(28, 82, 190);
    private static final int INK = Color.rgb(24, 35, 54);
    private static final int MUTED = Color.rgb(104, 116, 136);
    private static final int CANVAS = Color.rgb(244, 247, 251);
    private static final int LINE = Color.rgb(225, 231, 240);
    private static final int GREEN = Color.rgb(31, 157, 102);
    private static final int ORANGE = Color.rgb(234, 142, 46);
    private static final int RED = Color.rgb(218, 72, 78);

    private static final int TONE_NEUTRAL = 0;
    private static final int TONE_PROGRESS = 1;
    private static final int TONE_SUCCESS = 2;
    private static final int TONE_ERROR = 3;

    private final ExecutorService executor = Executors.newSingleThreadExecutor();
    private final Handler mainHandler = new Handler(Looper.getMainLooper());

    private EditText uidEdit;
    private EditText topicEdit;
    private TextView statusDot;
    private TextView statusTitle;
    private TextView statusText;
    private TextView topicText;
    private TextView temperatureText;
    private TextView lastActionText;
    private SharedPreferences prefs;
    private String lastSentCommand;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        configureSystemBars();
        prefs = getSharedPreferences(PREFS, MODE_PRIVATE);
        setContentView(buildContent());
    }

    @Override
    protected void onDestroy() {
        mainHandler.removeCallbacksAndMessages(null);
        executor.shutdownNow();
        super.onDestroy();
    }

    private void configureSystemBars() {
        Window window = getWindow();
        window.setStatusBarColor(NAVY);
        window.setNavigationBarColor(Color.WHITE);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            window.getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR);
        }
    }

    private View buildContent() {
        ScrollView scrollView = new ScrollView(this);
        scrollView.setFillViewport(true);
        scrollView.setBackgroundColor(CANVAS);
        scrollView.setClipToPadding(false);
        scrollView.setOverScrollMode(View.OVER_SCROLL_NEVER);

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(dp(16), dp(14), dp(16), dp(32));
        scrollView.addView(root, new ScrollView.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT));

        addHeader(root);
        addStatusCard(root);
        addDeviceCard(root);
        addUtilityCard(root);
        addLearningCard(root);
        addConfigCard(root);

        boolean configured = !prefs.getString(KEY_UID, "").trim().isEmpty();
        if (configured) {
            updateStatus(TONE_NEUTRAL, "遥控器已就绪", "选择一个操作，指令将通过巴法云发送");
        } else {
            updateStatus(TONE_NEUTRAL, "还差一步即可使用", "请在页面底部填写巴法云 UID / 私钥");
        }
        return scrollView;
    }

    private void addHeader(LinearLayout root) {
        LinearLayout header = new LinearLayout(this);
        header.setOrientation(LinearLayout.VERTICAL);
        header.setPadding(dp(20), dp(20), dp(20), dp(20));
        header.setElevation(dp(3));

        GradientDrawable headerBackground = new GradientDrawable(
                GradientDrawable.Orientation.TL_BR,
                new int[]{NAVY, Color.rgb(24, 67, 126)});
        headerBackground.setCornerRadius(dp(24));
        header.setBackground(headerBackground);

        LinearLayout top = new LinearLayout(this);
        top.setOrientation(LinearLayout.HORIZONTAL);
        top.setGravity(Gravity.CENTER_VERTICAL);

        TextView logo = text("IR", 16, Color.WHITE, Typeface.BOLD);
        logo.setGravity(Gravity.CENTER);
        logo.setLetterSpacing(0.08f);
        logo.setBackground(shape(BLUE, 15, 0, 0));
        top.addView(logo, new LinearLayout.LayoutParams(dp(48), dp(48)));

        LinearLayout brand = new LinearLayout(this);
        brand.setOrientation(LinearLayout.VERTICAL);
        brand.setPadding(dp(12), 0, 0, 0);
        brand.addView(text("冷静星智控", 21, Color.WHITE, Typeface.BOLD));
        TextView brandSub = text("SMART INFRARED CONTROL", 10,
                Color.rgb(180, 202, 232), Typeface.BOLD);
        brandSub.setLetterSpacing(0.12f);
        brand.addView(brandSub);
        top.addView(brand, new LinearLayout.LayoutParams(0,
                ViewGroup.LayoutParams.WRAP_CONTENT, 1f));

        TextView badge = text("远程控制", 11, Color.rgb(220, 233, 252), Typeface.BOLD);
        badge.setGravity(Gravity.CENTER);
        badge.setPadding(dp(10), dp(6), dp(10), dp(6));
        badge.setBackground(shape(Color.rgb(40, 78, 128), 99, 1,
                Color.rgb(75, 116, 169)));
        top.addView(badge);
        header.addView(top, matchWrap());

        TextView description = text("把手机变成你的空调遥控器", 14,
                Color.rgb(218, 229, 244), Typeface.NORMAL);
        description.setPadding(0, dp(18), 0, dp(5));
        header.addView(description);

        TextView gateway = text("STM32 网关  ·  巴法云  ·  红外控制", 12,
                Color.rgb(151, 182, 221), Typeface.NORMAL);
        header.addView(gateway);

        root.addView(header, cardParams(dp(14)));
    }

    private void addStatusCard(LinearLayout root) {
        LinearLayout card = card();
        card.setOrientation(LinearLayout.HORIZONTAL);
        card.setGravity(Gravity.CENTER_VERTICAL);
        card.setPadding(dp(16), dp(15), dp(13), dp(15));

        statusDot = text("●", 16, BLUE, Typeface.NORMAL);
        statusDot.setGravity(Gravity.CENTER);
        statusDot.setBackground(shape(Color.rgb(232, 240, 255), 14, 0, 0));
        card.addView(statusDot, new LinearLayout.LayoutParams(dp(42), dp(42)));

        LinearLayout copy = new LinearLayout(this);
        copy.setOrientation(LinearLayout.VERTICAL);
        copy.setPadding(dp(12), 0, dp(6), 0);
        statusTitle = text("", 14, INK, Typeface.BOLD);
        statusText = text("", 12, MUTED, Typeface.NORMAL);
        statusText.setPadding(0, dp(3), 0, 0);
        copy.addView(statusTitle);
        copy.addView(statusText);
        card.addView(copy, new LinearLayout.LayoutParams(0,
                ViewGroup.LayoutParams.WRAP_CONTENT, 1f));

        TextView refresh = compactButton("刷新");
        refresh.setOnClickListener(v -> fetchLatest());
        card.addView(refresh);
        root.addView(card, cardParams(dp(14)));
    }

    private void addDeviceCard(LinearLayout root) {
        LinearLayout card = card();
        addCardHeading(card, "客厅空调", "美的冷静星", "云端控制");

        topicText = text("网关主题  ·  " + prefs.getString(KEY_TOPIC, DEFAULT_TOPIC),
                12, MUTED, Typeface.NORMAL);
        topicText.setPadding(0, dp(3), 0, dp(16));
        card.addView(topicText);

        LinearLayout display = new LinearLayout(this);
        display.setOrientation(LinearLayout.HORIZONTAL);
        display.setGravity(Gravity.CENTER_VERTICAL);
        display.setPadding(dp(18), dp(15), dp(18), dp(15));
        display.setBackground(shape(Color.rgb(239, 245, 255), 18, 1,
                Color.rgb(216, 229, 252)));

        LinearLayout setting = new LinearLayout(this);
        setting.setOrientation(LinearLayout.VERTICAL);
        setting.addView(text("上次设定", 11, MUTED, Typeface.BOLD));
        temperatureText = text("--", 38, NAVY, Typeface.BOLD);
        temperatureText.setIncludeFontPadding(false);
        setting.addView(temperatureText);
        display.addView(setting, new LinearLayout.LayoutParams(0,
                ViewGroup.LayoutParams.WRAP_CONTENT, 1f));

        LinearLayout actionCopy = new LinearLayout(this);
        actionCopy.setOrientation(LinearLayout.VERTICAL);
        actionCopy.setGravity(Gravity.END);
        TextView snow = text("COOL", 11, BLUE, Typeface.BOLD);
        snow.setGravity(Gravity.CENTER);
        snow.setPadding(dp(10), dp(5), dp(10), dp(5));
        snow.setBackground(shape(Color.WHITE, 99, 0, 0));
        actionCopy.addView(snow);
        lastActionText = text("等待操作", 12, MUTED, Typeface.NORMAL);
        lastActionText.setPadding(0, dp(8), 0, 0);
        actionCopy.addView(lastActionText);
        display.addView(actionCopy);
        card.addView(display, matchWrap());

        LinearLayout powerRow = new LinearLayout(this);
        powerRow.setOrientation(LinearLayout.HORIZONTAL);
        powerRow.setPadding(0, dp(14), 0, 0);
        powerRow.addView(actionButton("开启空调", BLUE, Color.WHITE,
                "SEND 0"), actionCell(true));
        powerRow.addView(actionButton("关闭空调", Color.rgb(241, 244, 248),
                INK, "SEND 1"), actionCell(false));
        card.addView(powerRow, matchWrap());

        TextView quickLabel = eyebrow("快捷设定");
        quickLabel.setPadding(0, dp(20), 0, dp(9));
        card.addView(quickLabel);
        addTileRow(card,
                new Command("制冷 26°", "舒适温度", "26°", "SEND 2", true),
                new Command("制冷 23°", "快速降温", "23°", "SEND 3", true));
        addTileRow(card,
                new Command("屏幕显示", "切换亮灭", "LED", "SEND 4", true),
                new Command("ECO 模式", "节能运行", "ECO", "SEND 5", true));

        root.addView(card, cardParams(dp(14)));
    }

    private void addUtilityCard(LinearLayout root) {
        LinearLayout card = card();
        addCardHeading(card, "网关工具", "测试与设备反馈", null);
        TextView intro = text("用于检查红外发射和查看 STM32 的最新回复", 12,
                MUTED, Typeface.NORMAL);
        intro.setPadding(0, dp(3), 0, dp(12));
        card.addView(intro);

        addTileRow(card,
                new Command("红外测试", "检查发射", "IR", "IR_TEST", true),
                new Command("查询状态", "请求设备", "?", "STATUS", true));

        TextView latest = outlinedButton("读取最近回复");
        latest.setOnClickListener(v -> fetchLatest());
        LinearLayout.LayoutParams latestParams = matchWrap();
        latestParams.setMargins(0, dp(8), 0, 0);
        card.addView(latest, latestParams);
        root.addView(card, cardParams(dp(14)));
    }

    private void addLearningCard(LinearLayout root) {
        LinearLayout card = card();
        addCardHeading(card, "红外码学习", "需要重新匹配时使用", "高级");

        TextView tip = text("点击对应按键后，请在设备端按一次原遥控器按键。",
                12, Color.rgb(139, 91, 35), Typeface.NORMAL);
        tip.setPadding(dp(12), dp(10), dp(12), dp(10));
        tip.setBackground(shape(Color.rgb(255, 247, 233), 12, 1,
                Color.rgb(248, 224, 185)));
        LinearLayout.LayoutParams tipParams = matchWrap();
        tipParams.setMargins(0, dp(5), 0, dp(12));
        card.addView(tip, tipParams);

        addTileRow(card,
                new Command("学习开机", "按键 0", "0", "LEARN 0", true),
                new Command("学习关机", "按键 1", "1", "LEARN 1", true));
        addTileRow(card,
                new Command("学习 26°", "按键 2", "2", "LEARN 2", true),
                new Command("学习 23°", "按键 3", "3", "LEARN 3", true));
        addTileRow(card,
                new Command("学习屏显", "按键 4", "4", "LEARN 4", true),
                new Command("学习 ECO", "按键 5", "5", "LEARN 5", true));

        root.addView(card, cardParams(dp(14)));
    }

    private void addConfigCard(LinearLayout root) {
        LinearLayout card = card();
        addCardHeading(card, "云端连接", "巴法云账户配置", "本机保存");

        TextView intro = text("UID / 私钥只保存在当前手机，不会写入安装包。",
                12, MUTED, Typeface.NORMAL);
        intro.setPadding(0, dp(3), 0, dp(16));
        card.addView(intro);

        card.addView(fieldLabel("UID / 私钥"));
        uidEdit = input("请输入巴法云 UID");
        uidEdit.setText(prefs.getString(KEY_UID, ""));
        card.addView(uidEdit, inputParams());

        card.addView(fieldLabel("主题 Topic"));
        topicEdit = input("例如 infrared");
        topicEdit.setText(prefs.getString(KEY_TOPIC, DEFAULT_TOPIC));
        card.addView(topicEdit, inputParams());

        TextView save = actionButton("保存并启用", BLUE, Color.WHITE, null);
        save.setOnClickListener(v -> saveConfig());
        LinearLayout.LayoutParams saveParams = matchWrap();
        saveParams.setMargins(0, dp(6), 0, 0);
        card.addView(save, saveParams);
        root.addView(card, cardParams(0));
    }

    private void saveConfig() {
        String uid = uidEdit.getText().toString().trim();
        String topic = topicEdit.getText().toString().trim();
        if (uid.isEmpty()) {
            updateStatus(TONE_ERROR, "缺少 UID / 私钥", "填写后才能连接巴法云");
            uidEdit.requestFocus();
            return;
        }
        if (topic.isEmpty()) {
            topic = DEFAULT_TOPIC;
            topicEdit.setText(topic);
        }
        prefs.edit().putString(KEY_UID, uid).putString(KEY_TOPIC, topic).apply();
        topicText.setText("网关主题  ·  " + topic);
        updateStatus(TONE_SUCCESS, "配置已保存", "现在可以使用上方的遥控按键");
        Toast.makeText(this, "配置已保存", Toast.LENGTH_SHORT).show();
    }

    private void addTileRow(LinearLayout parent, Command left, Command right) {
        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.addView(commandTile(left), tileCell(true));
        row.addView(commandTile(right), tileCell(false));
        parent.addView(row, matchWrap());
    }

    private View commandTile(Command command) {
        LinearLayout tile = new LinearLayout(this);
        tile.setOrientation(LinearLayout.HORIZONTAL);
        tile.setGravity(Gravity.CENTER_VERTICAL);
        tile.setMinimumHeight(dp(72));
        tile.setPadding(dp(12), dp(11), dp(10), dp(11));
        tile.setClickable(true);
        tile.setFocusable(true);
        tile.setBackground(ripple(Color.rgb(247, 249, 252), 16,
                Color.rgb(216, 226, 242), LINE));

        TextView icon = text(command.icon, command.icon.length() > 2 ? 10 : 13,
                BLUE, Typeface.BOLD);
        icon.setGravity(Gravity.CENTER);
        icon.setBackground(shape(Color.rgb(229, 238, 255), 12, 0, 0));
        tile.addView(icon, new LinearLayout.LayoutParams(dp(39), dp(39)));

        LinearLayout copy = new LinearLayout(this);
        copy.setOrientation(LinearLayout.VERTICAL);
        copy.setPadding(dp(10), 0, 0, 0);
        copy.addView(text(command.label, 13, INK, Typeface.BOLD));
        TextView subtitle = text(command.subtitle, 11, MUTED, Typeface.NORMAL);
        subtitle.setPadding(0, dp(2), 0, 0);
        copy.addView(subtitle);
        tile.addView(copy, new LinearLayout.LayoutParams(0,
                ViewGroup.LayoutParams.WRAP_CONTENT, 1f));

        tile.setOnClickListener(v -> send(command.command, command.refreshAfterSend));
        return tile;
    }

    private TextView actionButton(String label, int fillColor, int textColor, String command) {
        TextView button = text(label, 14, textColor, Typeface.BOLD);
        button.setGravity(Gravity.CENTER);
        button.setMinHeight(dp(50));
        button.setPadding(dp(14), dp(12), dp(14), dp(12));
        int stroke = fillColor == BLUE ? BLUE_DARK : LINE;
        button.setBackground(ripple(fillColor, 14,
                Color.argb(45, 20, 55, 100), stroke));
        button.setClickable(true);
        button.setFocusable(true);
        if (command != null) {
            button.setOnClickListener(v -> send(command, true));
        }
        return button;
    }

    private TextView outlinedButton(String label) {
        TextView button = text(label, 13, BLUE, Typeface.BOLD);
        button.setGravity(Gravity.CENTER);
        button.setMinHeight(dp(46));
        button.setPadding(dp(14), dp(10), dp(14), dp(10));
        button.setBackground(ripple(Color.WHITE, 13,
                Color.rgb(220, 231, 249), Color.rgb(193, 210, 238)));
        button.setClickable(true);
        button.setFocusable(true);
        return button;
    }

    private TextView compactButton(String label) {
        TextView button = text(label, 12, BLUE, Typeface.BOLD);
        button.setGravity(Gravity.CENTER);
        button.setMinWidth(dp(54));
        button.setPadding(dp(10), dp(8), dp(10), dp(8));
        button.setBackground(ripple(Color.WHITE, 99,
                Color.rgb(220, 231, 249), Color.rgb(205, 218, 238)));
        button.setClickable(true);
        button.setFocusable(true);
        return button;
    }

    private void send(String command, boolean refreshAfterSend) {
        Config config = config();
        if (config == null) {
            return;
        }

        String label = prettyCommand(command);
        updateStatus(TONE_PROGRESS, "正在发送", label);
        executor.execute(() -> {
            BemfaResult result = BemfaClient.sendCommand(config.uid, config.topic, command);
            mainHandler.post(() -> {
                if (result.ok) {
                    lastSentCommand = command;
                    reflectCommand(command);
                    updateStatus(TONE_SUCCESS, "指令已发送", label + " · 正在等待设备回复");
                    if (refreshAfterSend) {
                        mainHandler.postDelayed(this::fetchLatest, 1200);
                    }
                } else {
                    updateStatus(TONE_ERROR, "发送失败",
                            label + " · code=" + result.code + readableMessage(result.message));
                }
            });
        });
    }

    private void fetchLatest() {
        Config config = config();
        if (config == null) {
            return;
        }

        updateStatus(TONE_PROGRESS, "正在读取设备回复", "正在连接巴法云…");
        executor.execute(() -> {
            /* 固件用 <topic>/up 后缀发布回复。巴法云的 /up 表示只更新云端，
             * 消息仍保存在主主题，因此这里继续查询原 topic。 */
            BemfaResult result = BemfaClient.getLatestMessage(config.uid, config.topic);
            mainHandler.post(() -> {
                if (result.ok) {
                    if (lastSentCommand != null && result.message.startsWith(lastSentCommand)) {
                        updateStatus(TONE_NEUTRAL, "等待设备响应",
                                "云端最新消息仍是刚才的指令，请稍后再刷新");
                    } else {
                        updateStatus(TONE_SUCCESS, "设备已回复", result.message);
                    }
                } else {
                    updateStatus(TONE_ERROR, "读取失败",
                            "code=" + result.code + readableMessage(result.message));
                }
            });
        });
    }

    private Config config() {
        String uid = uidEdit.getText().toString().trim();
        String topic = topicEdit.getText().toString().trim();
        if (uid.isEmpty()) {
            updateStatus(TONE_ERROR, "请先完成云端配置", "在页面底部填写巴法云 UID / 私钥");
            uidEdit.requestFocus();
            return null;
        }
        if (topic.isEmpty()) {
            topic = DEFAULT_TOPIC;
            topicEdit.setText(topic);
        }
        prefs.edit().putString(KEY_UID, uid).putString(KEY_TOPIC, topic).apply();
        topicText.setText("网关主题  ·  " + topic);
        return new Config(uid, topic);
    }

    private void reflectCommand(String command) {
        switch (command) {
            case "SEND 0":
                temperatureText.setText("ON");
                lastActionText.setText("开启空调");
                break;
            case "SEND 1":
                temperatureText.setText("OFF");
                lastActionText.setText("关闭空调");
                break;
            case "SEND 2":
                temperatureText.setText("26°");
                lastActionText.setText("制冷模式");
                break;
            case "SEND 3":
                temperatureText.setText("23°");
                lastActionText.setText("快速制冷");
                break;
            case "SEND 4":
                lastActionText.setText("切换屏显");
                break;
            case "SEND 5":
                lastActionText.setText("ECO 节能");
                break;
            default:
                break;
        }
    }

    private String prettyCommand(String command) {
        switch (command) {
            case "SEND 0": return "开启空调";
            case "SEND 1": return "关闭空调";
            case "SEND 2": return "制冷 26°";
            case "SEND 3": return "制冷 23°";
            case "SEND 4": return "切换屏幕显示";
            case "SEND 5": return "开启 ECO 模式";
            case "IR_TEST": return "红外发射测试";
            case "STATUS": return "查询设备状态";
            case "LEARN 0": return "学习开机按键";
            case "LEARN 1": return "学习关机按键";
            case "LEARN 2": return "学习制冷 26°";
            case "LEARN 3": return "学习制冷 23°";
            case "LEARN 4": return "学习屏显按键";
            case "LEARN 5": return "学习 ECO 按键";
            default: return command;
        }
    }

    private String readableMessage(String message) {
        return message == null || message.trim().isEmpty() ? "" : " · " + message.trim();
    }

    private void updateStatus(int tone, String title, String detail) {
        int accent;
        int soft;
        int border;
        switch (tone) {
            case TONE_PROGRESS:
                accent = BLUE;
                soft = Color.rgb(237, 244, 255);
                border = Color.rgb(207, 222, 247);
                break;
            case TONE_SUCCESS:
                accent = GREEN;
                soft = Color.rgb(235, 249, 242);
                border = Color.rgb(201, 234, 218);
                break;
            case TONE_ERROR:
                accent = RED;
                soft = Color.rgb(255, 240, 241);
                border = Color.rgb(244, 207, 210);
                break;
            default:
                accent = ORANGE;
                soft = Color.rgb(255, 248, 237);
                border = Color.rgb(244, 225, 194);
                break;
        }
        statusDot.setTextColor(accent);
        statusDot.setBackground(shape(soft, 14, 0, 0));
        statusTitle.setText(title);
        statusText.setText(detail);
        View parent = (View) statusTitle.getParent().getParent();
        parent.setBackground(shape(Color.WHITE, 20, 1, border));
    }

    private LinearLayout card() {
        LinearLayout card = new LinearLayout(this);
        card.setOrientation(LinearLayout.VERTICAL);
        card.setPadding(dp(18), dp(18), dp(18), dp(18));
        card.setBackground(shape(Color.WHITE, 20, 1, LINE));
        card.setElevation(dp(2));
        return card;
    }

    private void addCardHeading(LinearLayout card, String title, String subtitle, String badgeText) {
        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setGravity(Gravity.CENTER_VERTICAL);

        LinearLayout copy = new LinearLayout(this);
        copy.setOrientation(LinearLayout.VERTICAL);
        copy.addView(text(title, 18, INK, Typeface.BOLD));
        if (subtitle != null) {
            TextView sub = text(subtitle, 12, MUTED, Typeface.NORMAL);
            sub.setPadding(0, dp(2), 0, 0);
            copy.addView(sub);
        }
        row.addView(copy, new LinearLayout.LayoutParams(0,
                ViewGroup.LayoutParams.WRAP_CONTENT, 1f));

        if (badgeText != null) {
            TextView badge = text(badgeText, 10, BLUE, Typeface.BOLD);
            badge.setGravity(Gravity.CENTER);
            badge.setPadding(dp(9), dp(5), dp(9), dp(5));
            badge.setBackground(shape(Color.rgb(235, 242, 255), 99, 0, 0));
            row.addView(badge);
        }
        card.addView(row, matchWrap());
    }

    private TextView eyebrow(String label) {
        TextView view = text(label, 11, MUTED, Typeface.BOLD);
        view.setLetterSpacing(0.06f);
        return view;
    }

    private TextView fieldLabel(String label) {
        TextView view = text(label, 12, INK, Typeface.BOLD);
        view.setPadding(dp(2), 0, 0, dp(6));
        return view;
    }

    private EditText input(String hint) {
        EditText edit = new EditText(this);
        edit.setSingleLine(true);
        edit.setHint(hint);
        edit.setHintTextColor(Color.rgb(157, 168, 185));
        edit.setTextColor(INK);
        edit.setTextSize(14);
        edit.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD);
        edit.setPadding(dp(14), 0, dp(14), 0);
        edit.setBackground(ripple(Color.rgb(248, 250, 253), 13,
                Color.rgb(220, 231, 249), Color.rgb(214, 222, 234)));
        return edit;
    }

    private TextView text(String value, float size, int color, int style) {
        TextView view = new TextView(this);
        view.setText(value);
        view.setTextSize(size);
        view.setTextColor(color);
        view.setTypeface(Typeface.create("sans", style));
        return view;
    }

    private Drawable ripple(int fill, int radiusDp, int rippleColor, int strokeColor) {
        GradientDrawable content = shape(fill, radiusDp, strokeColor == 0 ? 0 : 1, strokeColor);
        GradientDrawable mask = shape(Color.WHITE, radiusDp, 0, 0);
        return new RippleDrawable(ColorStateList.valueOf(rippleColor), content, mask);
    }

    private GradientDrawable shape(int fill, int radiusDp, int strokeDp, int strokeColor) {
        GradientDrawable drawable = new GradientDrawable();
        drawable.setColor(fill);
        drawable.setCornerRadius(dp(radiusDp));
        if (strokeDp > 0) {
            drawable.setStroke(dp(strokeDp), strokeColor);
        }
        return drawable;
    }

    private LinearLayout.LayoutParams cardParams(int bottomMarginDp) {
        LinearLayout.LayoutParams params = matchWrap();
        params.setMargins(0, 0, 0, dp(bottomMarginDp));
        return params;
    }

    private LinearLayout.LayoutParams inputParams() {
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, dp(50));
        params.setMargins(0, 0, 0, dp(13));
        return params;
    }

    private LinearLayout.LayoutParams actionCell(boolean left) {
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(0,
                ViewGroup.LayoutParams.WRAP_CONTENT, 1f);
        params.setMargins(left ? 0 : dp(5), 0, left ? dp(5) : 0, 0);
        return params;
    }

    private LinearLayout.LayoutParams tileCell(boolean left) {
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(0,
                ViewGroup.LayoutParams.WRAP_CONTENT, 1f);
        params.setMargins(left ? 0 : dp(4), dp(4), left ? dp(4) : 0, dp(4));
        return params;
    }

    private LinearLayout.LayoutParams matchWrap() {
        return new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT);
    }

    private int dp(int value) {
        return (int) (value * getResources().getDisplayMetrics().density + 0.5f);
    }

    private static final class Command {
        final String label;
        final String subtitle;
        final String icon;
        final String command;
        final boolean refreshAfterSend;

        Command(String label, String subtitle, String icon, String command,
                boolean refreshAfterSend) {
            this.label = label;
            this.subtitle = subtitle;
            this.icon = icon;
            this.command = command;
            this.refreshAfterSend = refreshAfterSend;
        }
    }

    private static final class Config {
        final String uid;
        final String topic;

        Config(String uid, String topic) {
            this.uid = uid;
            this.topic = topic;
        }
    }
}
