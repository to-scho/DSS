#include "buzzer.h"
#include <Ticker.h>
#include "debug_by_level.h"

Ticker buzzer_ticker;
uint8_t buzzer_pin = 255;

void buzzer_set_pin(const uint8_t pin) {
  buzzer_pin = pin;
}

void buzzer_music(const TONE *play) {
  if (buzzer_pin!=255) {
    TONE this_play = *play;
    DEBUG_BY_LEVEL_PRINT(4, F("TONE {")); DEBUG_BY_LEVEL_PRINT(4, this_play.freq); DEBUG_BY_LEVEL_PRINT(4, F(", ")); DEBUG_BY_LEVEL_PRINT(4, this_play.ms); DEBUG_BY_LEVEL_PRINTLN(4, F("}"));
    if (this_play.ms) {
      pinMode(buzzer_pin, OUTPUT);
      if (this_play.freq){
        analogWriteFreq(this_play.freq);
        analogWrite(buzzer_pin, 512);
      } else {
        analogWrite(buzzer_pin, 0);
      }
      buzzer_ticker.once_ms(this_play.ms, buzzer_music, play+1);
    } else {
      buzzer_ticker.detach();
      analogWrite(buzzer_pin, 0);
      analogWriteFreq(1000);
      pinMode(buzzer_pin, INPUT_PULLUP);
    }
  } else {
    DEBUG_PRINTLN(F("No buzzer pin specified!"));
  }
}
