#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "ADS.h"

// ==========================================
// 1. 全域變數與控制狀態 (Global States)
// ==========================================
hw_timer_t *timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

// 交流切相觸發延遲時間 (微秒)
volatile uint32_t g_current_delay_us = DELAY_MIN_SPEED_US; // 當前平滑輸出延遲
volatile uint32_t g_target_delay_us  = DELAY_MIN_SPEED_US; // 目標延遲
volatile bool g_fan_enable           = false;              // 風扇運轉致能旗標

ADS ads1(ADS1_ADDRESS);
float g_filtered_temp = 25.0f; // EMA 濾波後的平滑溫度

// ==========================================
// 2. 中斷服務常式 (ISRs)
// ==========================================

// 定時器中斷：延遲到達，送出短脈衝觸發可控矽導通
void IRAM_ATTR onTimer() {
    portENTER_CRITICAL_ISR(&timerMux);
    // AQG22205 光耦 Triac 僅需 80us 觸發脈衝即可自鎖導通
    digitalWrite(SSR_PIN, HIGH);
    delayMicroseconds(80);
    digitalWrite(SSR_PIN, HIGH); // 恢復低電位，由交流零點自動關斷
    portEXIT_CRITICAL_ISR(&timerMux);
}

// 過零中斷：同步交流電半週期起點 (50Hz = 10,000us)
void IRAM_ATTR onZeroCross() {
    if (!g_fan_enable) {
        digitalWrite(SSR_PIN, HIGH);
        return;
    }

    timerStop(timer);
    timerWrite(timer, 0);
    timerAlarmWrite(timer, g_current_delay_us, false); // 單次觸發 (One-shot)
    timerAlarmEnable(timer);
    timerStart(timer);
}

// ==========================================
// 3. 調速計算與背景任務
// ==========================================

// 溫度映射與遲滯判斷 (Hysteresis & Linear Mapping)
void calculate_speed_target(float temp) {
    if (!g_fan_enable) {
        // 風扇待機：溫度達到啟動門檻才開機
        if (temp >= TEMP_START_C) {
            g_fan_enable = true;
            g_target_delay_us = DELAY_MIN_SPEED_US; // 最低速起步
            Serial.printf("[FAN] 達到啟動溫度: %.2f °C -> 開啟\n", temp);
        }
    } else {
        // 風扇運轉：溫度低於停止門檻才關閉 (防止臨界點反覆跳動)
        if (temp < TEMP_STOP_C) {
            g_fan_enable = false;
            Serial.printf("[FAN] 低於停止溫度: %.2f °C -> 關閉\n", temp);
            return;
        }

        // 溫度對應延遲計算 (28°C ~ 45°C -> 8000us ~ 1500us)
        float clamped_temp = constrain(temp, TEMP_START_C, TEMP_MAX_C);
        float ratio = (clamped_temp - TEMP_START_C) / (TEMP_MAX_C - TEMP_START_C);
        uint32_t calculated_delay = DELAY_MIN_SPEED_US - (uint32_t)(ratio * (DELAY_MIN_SPEED_US - DELAY_MAX_SPEED_US));
        
        // 限制在安全邊界內
        if (calculated_delay < DELAY_MAX_SPEED_US) calculated_delay = DELAY_MAX_SPEED_US;
        if (calculated_delay > DELAY_MIN_SPEED_US) calculated_delay = DELAY_MIN_SPEED_US;
        g_target_delay_us = calculated_delay;
    }
}

// 溫度取樣任務：每 200ms 非阻塞讀取一次 NTC
void task_sample_temperature() {
    static unsigned long last_sample_time = 0;
    if (millis() - last_sample_time < 200) return;
    last_sample_time = millis();

    // 讀取 AIN1 上的 2-Pin NTC 溫度
    float raw_temp = ads1.readNTCTemperature(AIN_NTC_CH);

    // 排除開路 / 短路異常讀數
    if (raw_temp < -40.0f || raw_temp > 130.0f) return;

    // EMA 指數平滑濾波 (alpha = 0.2)，濾除交流切換時的雜訊干擾
    g_filtered_temp = (0.2f * raw_temp) + (0.8f * g_filtered_temp);
    calculate_speed_target(g_filtered_temp);
}

// 斜率漸進任務：每 20ms 微調延遲，消除電流撞擊聲與馬達嗡鳴 (Slew Rate Limiter)
void task_slew_rate_ramp() {
    static unsigned long last_ramp_time = 0;
    if (millis() - last_ramp_time < 20) return;
    last_ramp_time = millis();

    uint32_t cur = g_current_delay_us;
    uint32_t target = g_target_delay_us;

    if (cur < target) {
        // 延遲增加 (風扇降速)
        cur += 25;
        if (cur > target) cur = target;
        g_current_delay_us = cur;
    } else if (cur > target) {
        // 延遲減少 (風扇升速，加入防下溢保護)
        if (cur > target + 25) {
            cur -= 25;
        } else {
            cur = target;
        }
        g_current_delay_us = cur;
    }
}

// ==========================================
// 4. 初始化與主循環 (Setup & Loop)
// ==========================================
void setup() {
    Serial.begin(115200);

    // I2C 初始化 (SDA: 4, SCL: 5)
    Wire.begin(SDA_PIN, SCL_PIN);
    ads1.begin();

    // 腳位設定
    pinMode(ZERO_CROSS_PIN, INPUT_PULLUP);
    pinMode(SSR_PIN, OUTPUT);
    digitalWrite(SSR_PIN, HIGH); // 初始為高電位，避免 SSR 導通

    // 綁定邊沿中斷 (捕捉過零下降沿)
    attachInterrupt(digitalPinToInterrupt(ZERO_CROSS_PIN), onZeroCross, FALLING);

    // 初始化定時器 0 (1 us tick)
    timer = timerBegin(0, 80, true);
    timerAttachInterrupt(timer, &onTimer, true);

    Serial.println(F("NTC 交流風扇調速系統啟動完成"));
}

void loop() {
    task_sample_temperature(); // 1. 非阻塞取樣 NTC 並計算目標延遲
    task_slew_rate_ramp();     // 2. 毫秒級平滑過渡相位角

    // 每秒輸出一次監控狀態
    static unsigned long last_log = 0;
    if (millis() - last_log >= 1000) {
        last_log = millis();
        if (g_fan_enable) {
            float power_pct = map(g_current_delay_us, DELAY_MIN_SPEED_US, DELAY_MAX_SPEED_US, 30, 100);
            Serial.printf("[監控] 溫度: %.2f °C | 延遲: %u us | 功率: %.0f %%\n",
                          g_filtered_temp, g_current_delay_us, power_pct);
        } else {
            Serial.printf("[監控] 溫度: %.2f °C | 風扇: 待機停止\n", g_filtered_temp);
        }
    }

    yield(); // 餵看門狗
}