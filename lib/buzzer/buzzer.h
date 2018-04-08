#ifndef _BUZZER
#define _BUZZER

#include <Arduino.h>
#define BUZZER_OFF HIGH

#define TONE_OFF   0
#define TONE_C     261
#define TONE_D     294
#define TONE_E     329
#define TONE_F     349
#define TONE_G     391
#define TONE_GS    415
#define TONE_A     440
#define TONE_AS    455
#define TONE_B     466
#define TONE_CH    523
#define TONE_CSH   554
#define TONE_DH    587
#define TONE_DSH   622
#define TONE_EH    659
#define TONE_FH    698
#define TONE_FSH   740
#define TONE_GH    784
#define TONE_GSH   830
#define TONE_AH    880

typedef struct tone {
  uint16_t freq;
  uint16_t ms;
} TONE;

void buzzer_music(const TONE *play);
void buzzer_set_pin(const uint8_t pin);

#endif
