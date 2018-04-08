#include "blinker.h"
#include <Ticker.h>
#include "debug_by_level.h"

extern "C" {
  #include "user_interface.h" // wifi_softap_get_station_num
}

Ticker blinker_ticker;
uint8_t blinker_pin = 255;
uint8_t blinker_add_onoff_num = 0;
uint8_t blinker_onoff_num, blinker_onoff_cnt;
uint16_t blinker_onoff_period_ms;
uint8_t blinker_repeat_num, blinker_repeat_cnt;
uint16_t blinker_repeat_period_ms, blinker_repeat_ticker_ms;

void blinker_set_pin(const uint8_t pin) {
  blinker_pin = pin;
  pinMode(blinker_pin, OUTPUT);
  digitalWrite(blinker_pin, BLINKER_LED_OFF);
}
void blinker_set_onoff_num(uint8_t onoff_num){
  blinker_onoff_cnt = blinker_onoff_num = onoff_num;
}
void blinker_set_onoff_period_ms(uint16_t onoff_period_ms) {
  blinker_onoff_period_ms = onoff_period_ms;
}
void blinker_set_repeat_num(uint8_t repeat_num) {
  blinker_repeat_cnt = blinker_repeat_num = repeat_num;
}
void blinker_set_repeat_period_ms(uint16_t repeat_period_ms) {
  blinker_repeat_ticker_ms = blinker_repeat_period_ms = repeat_period_ms;
}
void blinker_set_add_onoff_num_per_wifi_station(const uint8_t add_onoff_num) {
  blinker_add_onoff_num = add_onoff_num;
}
void blinker_ticker_fun(uint8_t onoff) {
  if (onoff) {
    // led on for onoff period
    digitalWrite(blinker_pin, BLINKER_LED_ON);
    blinker_repeat_ticker_ms-=blinker_onoff_period_ms;
    blinker_ticker.once_ms((uint32_t)blinker_onoff_period_ms, blinker_ticker_fun, (uint8_t)0);
  } else {
    digitalWrite(blinker_pin, BLINKER_LED_OFF);
    if ((blinker_onoff_num==0) || (--blinker_onoff_cnt)) {
      // continue onoff sequence, led off for onoff period
      blinker_repeat_ticker_ms-=blinker_onoff_period_ms;
      blinker_ticker.once_ms((uint32_t)blinker_onoff_period_ms, blinker_ticker_fun, (uint8_t)1);
    } else {
      if ((blinker_repeat_num==0) || (--blinker_repeat_cnt)) {
        // continue onoff sequence done, repeat onoff sequence with given period
        blinker_onoff_cnt = blinker_onoff_num;
        if (blinker_add_onoff_num) blinker_onoff_cnt+=wifi_softap_get_station_num() * blinker_add_onoff_num;
        while (blinker_repeat_ticker_ms>blinker_repeat_period_ms) blinker_repeat_ticker_ms+=blinker_repeat_period_ms;
        blinker_ticker.once_ms((uint32_t)blinker_repeat_ticker_ms, blinker_ticker_fun, (uint8_t)1);
        blinker_repeat_ticker_ms = blinker_repeat_period_ms;
      } else {
        // all done
        blinker_ticker.detach();
        pinMode(blinker_pin, INPUT);
      }
    }
  }
}
void blinker_start(uint8_t onoff_num, uint16_t onoff_period_ms, uint8_t repeat_num, uint16_t repeat_period_ms) {
  blinker_onoff_num = onoff_num;
  blinker_onoff_period_ms = onoff_period_ms;
  blinker_repeat_num = repeat_num;
  blinker_repeat_period_ms = repeat_period_ms;
  blinker_start();
}
void blinker_start() {
  if (blinker_pin!=255) {
    DEBUG_BY_LEVEL_PRINT(4,F("Start blinker @ PIN ")); DEBUG_BY_LEVEL_PRINT(4, blinker_pin);
    DEBUG_BY_LEVEL_PRINT(4,F("with onoff_num = ")); DEBUG_BY_LEVEL_PRINT(4, blinker_onoff_num);
    DEBUG_BY_LEVEL_PRINT(4,F(", onoff_period_ms = ")); DEBUG_BY_LEVEL_PRINT(4, blinker_onoff_period_ms);
    DEBUG_BY_LEVEL_PRINT(4,F(", repeat_num = ")); DEBUG_BY_LEVEL_PRINT(4, blinker_repeat_num);
    DEBUG_BY_LEVEL_PRINT(4,F(", repeat_period_ms = ")); DEBUG_BY_LEVEL_PRINTLN(4, blinker_repeat_period_ms);
    blinker_onoff_cnt = blinker_onoff_num ;
    blinker_repeat_cnt = blinker_repeat_num;
    blinker_repeat_ticker_ms = blinker_repeat_period_ms;
    blinker_ticker_fun(1);
  } else {
    DEBUG_PRINTLN(F("No blinker pin specified!"));
  }
}
void blinker_stop() {
  if (blinker_pin!=255) {
    blinker_ticker.detach();
    digitalWrite(blinker_pin, BLINKER_LED_OFF);
  }
}
