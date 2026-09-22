#ifndef SSR_CONTROL_H
#define SSR_CONTROL_H

#include <Arduino.h>
#include "config.h"

// 基礎控制函式
void set_ssr_state(bool state);
void emergency_stop();
bool is_safe_parameters();

// 系統初始化與調速介面
void ssr_init();
void ssr_set_power(float power_pct);
void ssr_task_ramp();

// 監控狀態取得
uint32_t ssr_get_current_delay();
bool     ssr_is_enabled();
uint32_t ssr_get_zc_count();

#endif