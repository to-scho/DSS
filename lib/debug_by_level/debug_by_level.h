#ifndef _DEBUG_BY_LEVEL
#define _DEBUG_BY_LEVEL

#include <Arduino.h>

#ifndef DEBUG_LEVEL_DEFAULT
#define DEBUG_LEVEL_DEFAULT 1
#endif

#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL DEBUG_LEVEL_DEFAULT
#endif

#define DEBUG_BY_LEVEL(_DEBUG_LEVEL, ...) __DEBUG_BY_LEVEL(_DEBUG_LEVEL, __VA_ARGS__)
#define __DEBUG_BY_LEVEL(_DEBUG_LEVEL, ...) { DEBUG_BY_LEVEL_ ## _DEBUG_LEVEL(__VA_ARGS__) }
#define DEBUG(...) DEBUG_BY_LEVEL(DEBUG_LEVEL_DEFAULT, __VA_ARGS__)

#define DEBUG_BY_LEVEL_PRINT(_DEBUG_LEVEL, ...) DEBUG_BY_LEVEL(_DEBUG_LEVEL, Serial.print(__VA_ARGS__))
#define DEBUG_PRINT(...) DEBUG_BY_LEVEL_PRINT(DEBUG_LEVEL_DEFAULT, __VA_ARGS__)

#define DEBUG_BY_LEVEL_PRINTLN(_DEBUG_LEVEL, ...) DEBUG_BY_LEVEL(_DEBUG_LEVEL, Serial.println(__VA_ARGS__))
#define DEBUG_PRINTLN(...) DEBUG_BY_LEVEL_PRINTLN(DEBUG_LEVEL_DEFAULT, __VA_ARGS__)

#define DEBUG_BY_LEVEL_PRINTF(_DEBUG_LEVEL, ...) DEBUG_BY_LEVEL(_DEBUG_LEVEL, Serial.printf(__VA_ARGS__))
#define DEBUG_PRINTF(...) DEBUG_BY_LEVEL_PRINTF(DEBUG_LEVEL_DEFAULT, __VA_ARGS__)

#if DEBUG_LEVEL >= 0
#define DEBUG_BY_LEVEL_0(...)  { __VA_ARGS__ ;}
#else
#define DEBUG_BY_LEVEL_0(...)
#endif

#if DEBUG_LEVEL >= 1
#define DEBUG_BY_LEVEL_1(...)  { __VA_ARGS__ ;}
#else
#define DEBUG_BY_LEVEL_1(...)
#endif

#if DEBUG_LEVEL >= 2
#define DEBUG_BY_LEVEL_2(...)  { __VA_ARGS__ ;}
#else
#define DEBUG_BY_LEVEL_2(...)
#endif

#if DEBUG_LEVEL >= 3
#define DEBUG_BY_LEVEL_3(...)  { __VA_ARGS__ ;}
#else
#define DEBUG_BY_LEVEL_3(...)
#endif

#if DEBUG_LEVEL >= 4
#define DEBUG_BY_LEVEL_4(...)  { __VA_ARGS__ ;}
#else
#define DEBUG_BY_LEVEL_4(...)
#endif

#if DEBUG_LEVEL >= 5
#define DEBUG_BY_LEVEL_5(...)  { __VA_ARGS__ ;}
#else
#define DEBUG_BY_LEVEL_5(...)
#endif

#endif
