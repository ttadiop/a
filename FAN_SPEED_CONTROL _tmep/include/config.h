#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// I2C 腳位
#define SDA_PIN 4
#define SCL_PIN 5

// 控制腳位
#define ZERO_CROSS_PIN     42  // 過零檢測 (GPIO 42)
#define SSR_PIN            41  // SSR 控制腳位 (GPIO 41)

// 交流半週期 (10000us) 切相觸發延遲安全範圍
#define DELAY_MIN_SPEED_US 8500  // 10% 功率延遲 (1.0V)
#define DELAY_MAX_SPEED_US 1500  // 100% 全速延遲 (10.0V)

// 0-10V 電壓啟停門檻
#define VOLT_OFF_THRESHOLD 0.5f  // 小於 0.5V 關閉風扇 (0%)
#define VOLT_ON_THRESHOLD  0.8f  // 大於 0.8V 啟動

// ADS1115 設定 (AIN0 連接 0-10V，30k + 10k 分壓還原 4 倍)[cite: 5]
#define ADS1_ADDRESS        0x48
#define AIN_0_10V_CH        0[cite: 5]
#define VOLT_DIVIDER_RATIO  4.0f[cite: 5]

#endif