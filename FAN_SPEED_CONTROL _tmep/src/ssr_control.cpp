#include "ssr_control.h"

hw_timer_t *timer = NULL;

uint32_t g_current_delay_us = DELAY_MIN_SPEED_US;
uint32_t g_target_delay_us  = DELAY_MIN_SPEED_US;
bool     g_fan_enable       = false;
uint32_t g_zc_count         = 0;

// SSR 控制函數
void set_ssr_state(bool state) {
    digitalWrite(SSR_PIN, state ? HIGH : LOW);
}

// 緊急停止
void emergency_stop() {
    g_fan_enable = false;
    set_ssr_state(false);
    Serial.println(F("\nEMERGENCY STOP - SSR turned OFF"));
}

// 安全檢查函數
bool is_safe_parameters() {
    if (g_target_delay_us < DELAY_MAX_SPEED_US || g_target_delay_us > DELAY_MIN_SPEED_US) {
        Serial.println(F("Error: Delay time out of range!"));
        return false;
    }
    return true;
}

// 定時器中斷：觸發 SSR 導通
void onTimer() {
    set_ssr_state(true);
}

// 過零中斷：半週期開始，關閉 SSR 並啟動定時器
void onZeroCross() {
    g_zc_count++;
    set_ssr_state(false);

    if (!g_fan_enable) return;

    timerStop(timer);
    timerWrite(timer, 0);
    timerAlarmWrite(timer, g_current_delay_us, false);
    timerAlarmEnable(timer);
    timerStart(timer);
}

// 初始化 SSR、過零檢測與硬體計時器
void ssr_init() {
    pinMode(ZERO_CROSS_PIN, INPUT_PULLUP);
    pinMode(SSR_PIN, OUTPUT);
    set_ssr_state(false);

    attachInterrupt(digitalPinToInterrupt(ZERO_CROSS_PIN), onZeroCross, FALLING);

    // 1 tick = 1 us
    timer = timerBegin(0, 80, true);
    timerAttachInterrupt(timer, onTimer, true);
}

// 設定調速百分比 (10% ~ 100%)
void ssr_set_power(float power_pct) {
    if (power_pct <= 0.0f) {
        g_fan_enable = false;
        set_ssr_state(false);
        return;
    }

    g_fan_enable = true;
    float pct = constrain(power_pct, 10.0f, 100.0f);
    float ratio = (pct - 10.0f) / 90.0f; // 歸一化到 0.0 ~ 1.0

    // 功率越高，延遲越小
    g_target_delay_us = DELAY_MIN_SPEED_US - (uint32_t)(ratio * (DELAY_MIN_SPEED_US - DELAY_MAX_SPEED_US));
    is_safe_parameters();
}

// 每 20ms 平滑改變一次延遲，消除嗡鳴聲
void ssr_task_ramp() {
    static unsigned long last_ramp = 0;
    if (millis() - last_ramp < 20) return;
    last_ramp = millis();

    if (g_current_delay_us < g_target_delay_us) {
        g_current_delay_us += 30;
        if (g_current_delay_us > g_target_delay_us) g_current_delay_us = g_target_delay_us;
    } else if (g_current_delay_us > g_target_delay_us) {
        if (g_current_delay_us > g_target_delay_us + 30) {
            g_current_delay_us -= 30;
        } else {
            g_current_delay_us = g_target_delay_us;
        }
    }
}

uint32_t ssr_get_current_delay() {
    return g_current_delay_us;
}

bool ssr_is_enabled() {
    return g_fan_enable;
}

uint32_t ssr_get_zc_count() {
    uint32_t count = g_zc_count;
    g_zc_count = 0;
    return count;
}