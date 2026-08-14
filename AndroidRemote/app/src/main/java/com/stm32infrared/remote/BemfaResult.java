package com.stm32infrared.remote;

final class BemfaResult {
    final boolean ok;
    final int code;
    final String message;
    final String raw;

    BemfaResult(boolean ok, int code, String message, String raw) {
        this.ok = ok;
        this.code = code;
        this.message = message == null ? "" : message;
        this.raw = raw == null ? "" : raw;
    }
}
