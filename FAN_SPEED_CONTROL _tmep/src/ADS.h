#ifndef ADS_H
#define ADS_H

#include <Arduino.h>
#include <ADS1X15.h>
#include "config.h"

// ADS Definitions
#define ADS1_ADDRESS 0x48
#define ADS2_ADDRESS 0x49

// NTC Definitions
#define NUM_NTC 3
#define NUM_SAMPLES 1
#define ADS_NUM_PINS 4

#define NOMINAL_RESISTANCE 10000
#define NOMINAL_TEMPERATURE 25
#define BETA 3950
#define RREF 10000

// 輔助轉換原型宣告
float NTCTemperature(int16_t voltage);
float CRW4Temperature(int16_t voltage);
float CRW4Humidity(int16_t voltage);
float Input_current(int16_t voltage);
float convert_voltage(int16_t voltage);

// ACS712 Definitions
#define VREF 2480.0f
#define SENSITIVITY 10.0f
#define CALIBRATION_OFFSET 0

class ADS {
public:
    ADS1115 ads;

    ADS(int address = ADS1_ADDRESS);

    // 啟動硬體通訊配置
    void begin();

    // 原始多通道讀取 (非阻塞)
    void read(int16_t voltages[ADS_NUM_PINS]);

    // 快速單通道電壓讀取 (mV)
    float readChannelMilliVolts(uint8_t channel);

    // 讀取 2-Pin NTC 溫度 (°C)
    float readNTCTemperature(uint8_t channel = AIN_NTC_CH);
};

#endif