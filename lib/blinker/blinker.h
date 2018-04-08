#ifndef _BLINKER
#define _BLINKER

#include <Arduino.h>
#define BLINKER_LED_ON LOW
#define BLINKER_LED_OFF HIGH

//void blinker(uint8_t onoff_num);
void blinker_start(uint8_t onoff_num, uint16_t onoff_period_ms, uint8_t repeat_num, uint16_t repeat_period_ms);
void blinker_start();
void blinker_stop();
void blinker_set_pin(const uint8_t pin);
void blinker_set_add_onoff_num_per_wifi_station(const uint8_t add_onoff_num);
void blinker_set_onoff_num(uint8_t onoff_num);
void blinker_set_onoff_period_ms(uint16_t onoff_period_ms);
void blinker_set_repeat_num(uint8_t repeat_num);
void blinker_set_repeat_period_ms(uint16_t repeat_period_ms);

#endif
