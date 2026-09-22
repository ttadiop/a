#include "ADS.h"
#include <Wire.h>
#include <Arduino.h>
#include <math.h>

// 計算 NTC 溫度
float NTCTemperature(int16_t voltage) {
    float v = (float)voltage;
    if (v <= 10.0f || v >= (NTC_VREF_MV - 10.0f)) return -999.0f; // 斷路/短路防護
    
    // 電路架構: 2V5 -> R10 (10k) -> [AIN1] -> NTC -> GND
    float resistance = NTC_PULLUP_R * (v / (NTC_VREF_MV - v));
    float temp = resistance / NOMINAL_RESISTANCE;
    temp = log(temp);
    temp /= BETA;
    temp += 1.0f / (NOMINAL_TEMPERATURE + 273.15f);
    return (1.0f / temp) - 273.15f;
}

float CRW4Temperature(int16_t voltage) {
    float current = (float)voltage;
    return map(current, 400, 2000, 0, 500);
}

float CRW4Humidity(int16_t voltage) {
    float current = (float)voltage;
    return map(current, 400, 2000, 0, 1000);
}

float Input_current(int16_t voltage) {   
    float Viout = (float)voltage * 2.0f; // unit: mV
    float current = (Viout + CALIBRATION_OFFSET - VREF) * SENSITIVITY; // unit: mA
    return current;
}

float convert_voltage(int16_t voltage) {
    return (float)voltage * 4.0f;
}

ADS::ADS(int address) {
    ads = ADS1115(address);
}

void ADS::begin() {
    ads.begin();
    ads.setGain(0);     // 增益 0: ±6.144V
    ads.setDataRate(4); // 128 SPS
    ads.setMode(0);     // 連續轉換模式
}

// 快速讀取單通道電壓 (mV)
float ADS::readChannelMilliVolts(uint8_t channel) {
    int16_t adcValue = ads.readADC(channel);
    float f = ads.toVoltage();
    return adcValue * f * 1000.0f;
}

// 讀取 AIN1 上的 NTC 溫度
float ADS::readNTCTemperature(uint8_t channel) {
    float v_mv = readChannelMilliVolts(channel);
    return NTCTemperature((int16_t)v_mv);
}

// 讀取全部通道 (已移除 delay 阻塞)
void ADS::read(int16_t voltages[ADS_NUM_PINS]) {
    float f = ads.toVoltage();
    for (int i = 0; i < ADS_NUM_PINS; i++) {
        int16_t adcValue = ads.readADC(i);
        voltages[i] = (int16_t)(adcValue * f * 1000.0f);
    }
}