package com.stm32infrared.remote;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.BufferedReader;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URLEncoder;
import java.net.URL;
import java.nio.charset.StandardCharsets;

final class BemfaClient {
    private static final String PUSH_URL = "https://apis.bemfa.com/va/postJsonMsg";
    private static final String GET_URL = "https://apis.bemfa.com/va/getmsg";
    private static final int DEVICE_TYPE_TCP = 3;

    private BemfaClient() {
    }

    static BemfaResult sendCommand(String uid, String topic, String command) {
        HttpURLConnection conn = null;
        try {
            JSONObject body = new JSONObject();
            body.put("uid", uid);
            body.put("topic", topic);
            body.put("type", DEVICE_TYPE_TCP);
            body.put("msg", command);

            byte[] payload = body.toString().getBytes(StandardCharsets.UTF_8);
            conn = (HttpURLConnection) new URL(PUSH_URL).openConnection();
            conn.setRequestMethod("POST");
            conn.setConnectTimeout(8000);
            conn.setReadTimeout(8000);
            conn.setDoOutput(true);
            conn.setRequestProperty("Content-Type", "application/json; charset=utf-8");
            conn.setRequestProperty("Accept", "application/json");

            try (OutputStream os = conn.getOutputStream()) {
                os.write(payload);
            }

            String response = readResponse(conn);
            JSONObject json = new JSONObject(response);
            int code = json.optInt("code", -1);
            String message = json.optString("message", json.optString("msg", ""));
            return new BemfaResult(code == 0, code, message, response);
        } catch (Exception e) {
            return new BemfaResult(false, -1, e.getMessage(), "");
        } finally {
            if (conn != null) {
                conn.disconnect();
            }
        }
    }

    static BemfaResult getLatestMessage(String uid, String topic) {
        HttpURLConnection conn = null;
        try {
            String url = GET_URL
                    + "?uid=" + enc(uid)
                    + "&topic=" + enc(topic)
                    + "&type=" + DEVICE_TYPE_TCP
                    + "&num=1";
            conn = (HttpURLConnection) new URL(url).openConnection();
            conn.setRequestMethod("GET");
            conn.setConnectTimeout(8000);
            conn.setReadTimeout(8000);
            conn.setRequestProperty("Accept", "application/json");

            String response = readResponse(conn);
            JSONObject json = new JSONObject(response);
            int code = json.optInt("code", -1);
            if (code != 0) {
                return new BemfaResult(false, code,
                        json.optString("message", json.optString("msg", "")), response);
            }

            JSONArray data = json.optJSONArray("data");
            if (data == null || data.length() == 0) {
                return new BemfaResult(true, code, "暂无消息", response);
            }

            JSONObject latest = data.getJSONObject(0);
            String msg = latest.optString("msg", "");
            String time = latest.optString("time", "");
            return new BemfaResult(true, code, msg + (time.isEmpty() ? "" : "\n" + time), response);
        } catch (Exception e) {
            return new BemfaResult(false, -1, e.getMessage(), "");
        } finally {
            if (conn != null) {
                conn.disconnect();
            }
        }
    }

    private static String enc(String text) throws Exception {
        return URLEncoder.encode(text, "UTF-8");
    }

    private static String readResponse(HttpURLConnection conn) throws Exception {
        int status = conn.getResponseCode();
        InputStream stream = status >= 200 && status < 400 ? conn.getInputStream() : conn.getErrorStream();
        if (stream == null) {
            return "";
        }

        StringBuilder sb = new StringBuilder();
        try (BufferedReader br = new BufferedReader(new InputStreamReader(stream, StandardCharsets.UTF_8))) {
            String line;
            while ((line = br.readLine()) != null) {
                sb.append(line);
            }
        }
        return sb.toString();
    }
}
