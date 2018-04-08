#ifndef _TICTOC_H
#define _TICTOC_H
#include "debug_by_level.h"

extern unsigned long tictoc_millis;

#define TIC ({tictoc_millis = millis(); DEBUG_PRINT(F("TIC = ")); DEBUG_PRINT(tictoc_millis); DEBUG_PRINT(F(" ms @ line ")); DEBUG_PRINTLN(__LINE__); tictoc_millis;})
#define TOC ({unsigned long toc_millis = millis() - tictoc_millis; DEBUG_PRINT(F("TOC = ")); DEBUG_PRINT(toc_millis); DEBUG_PRINT(F(" ms @ line ")); DEBUG_PRINTLN(__LINE__); toc_millis;})
#define TICTOC ({unsigned long toc_millis = -tictoc_millis; tictoc_millis = millis(); toc_millis += tictoc_millis; DEBUG_PRINT(F("TICTOC = ")); DEBUG_PRINT(toc_millis); DEBUG_PRINT(F(" ms @ line ")); DEBUG_PRINTLN(__LINE__); toc_millis;})

#endif
