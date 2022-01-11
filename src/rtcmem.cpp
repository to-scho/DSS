#include "rtcmem.h"
#include "debug_by_level.h"
#include "crc32.h"
#include "SNTPtime.h"
#include <time.h>
#include "deep_sleep_sensor.h"

extern "C" {
#include "user_interface.h" // RTC memory, RESET reason, ...
}

alignas(4) RTCMEM_TOKEN_S rtcmem_token;
alignas(4) RTCMEM_STATE_S rtcmem_state;

bool rtcmem_write_token(bool set_fingerprint) {
  if (set_fingerprint) rtcmem_token.fingerprint = RTCMEM_TOKEN_FINGERPRINT;
  DEBUG_BY_LEVEL_PRINT(2, F("rtcmem_write_token: fingerprint set to ")); DEBUG_BY_LEVEL_PRINTLN(2, RTCMEM_TOKEN_FINGERPRINT, HEX);
  DEBUG_BY_LEVEL_PRINT(4, F("RTCMEM_TOKEN_OFFSET: ")); DEBUG_BY_LEVEL_PRINT(4, RTCMEM_TOKEN_OFFSET); DEBUG_BY_LEVEL_PRINT(4, F(", &rtcmem_token: ")); DEBUG_BY_LEVEL_PRINT(4, (uint32_t) &rtcmem_token, HEX); DEBUG_BY_LEVEL_PRINT(4, F(", sizeof(rtcmem_token): ")); DEBUG_BY_LEVEL_PRINTLN(4, sizeof(rtcmem_token));
  if (ESP.rtcUserMemoryWrite(RTCMEM_TOKEN_OFFSET, (uint32_t*) &rtcmem_token, sizeof(rtcmem_token))) {
    DEBUG_BY_LEVEL_PRINTLN(2, F("rtcmem_write_token: RTC memory write success"));
    return true;
  }
  else
  {
    DEBUG_PRINTLN(F("rtcmem_write_token: RTC memory write failure\n"));
  }
  return false;
}

bool rtcmem_write_token() {
  return rtcmem_write_token(true);
}

bool rtcmem_read_token() {
  if (ESP.rtcUserMemoryRead(RTCMEM_TOKEN_OFFSET, (uint32_t*) &rtcmem_token, sizeof(rtcmem_token))) {
    DEBUG_BY_LEVEL_PRINTLN(2, F("rtcmem_read_token: RTC memory read success"));
    DEBUG_BY_LEVEL_PRINT(4, F("rtcmem_read_token: fingerprint read from RTC ")); DEBUG_BY_LEVEL_PRINTLN(4,rtcmem_token.fingerprint, HEX);
    DEBUG_BY_LEVEL_PRINT(4, F("rtcmem_read_token: fingerprint should be ")); DEBUG_BY_LEVEL_PRINTLN(4,RTCMEM_TOKEN_FINGERPRINT, HEX);
    if (RTCMEM_TOKEN_FINGERPRINT == rtcmem_token.fingerprint) {
      DEBUG_BY_LEVEL_PRINTLN(2, F("rtcmem_read_token: fingerprint matched"));
      return true;
    } else {
      DEBUG_PRINTLN(F("rtcmem_read_token: fingerprint missmatch"));
    }
  }
  else
  {
    DEBUG_PRINTLN(F("rtcmem_read_token: RTC memory read failure"));
  }
  rtcmem_to_default_token();
  return false;
};

void ICACHE_FLASH_ATTR rtcmem_to_default_token() {
  DEBUG_PRINTLN(F("rtcmem_to_default_token: Setting rtcmem_token to default"));
  RTCMEM_TOKEN_S rtcmem_token_default;
  rtcmem_token = rtcmem_token_default;
}

bool rtcmem_write_state(bool calc_crc32) {
  if (calc_crc32) rtcmem_state.crc32 = crc32((byte*) &rtcmem_state.data, sizeof(RTCMEM_STATE_DATA_S));
  DEBUG_BY_LEVEL_PRINT(2, F("rtcmem_write_state: CRC32 calculated to ")); DEBUG_BY_LEVEL_PRINTLN(2,rtcmem_state.crc32, HEX);
  DEBUG_BY_LEVEL_PRINT(4, F("RTCMEM_STATE_OFFSET: ")); DEBUG_BY_LEVEL_PRINT(4,RTCMEM_STATE_OFFSET); DEBUG_BY_LEVEL_PRINT(4,F(", &rtcmem_state: ")); DEBUG_BY_LEVEL_PRINT(4,(uint32_t) &rtcmem_state, HEX); DEBUG_BY_LEVEL_PRINT(4,F(", sizeof(rtcmem_state): ")); DEBUG_BY_LEVEL_PRINTLN(4,sizeof(rtcmem_state));
  if (ESP.rtcUserMemoryWrite(RTCMEM_STATE_OFFSET, (uint32_t*) &rtcmem_state, sizeof(rtcmem_state))) {
    DEBUG_BY_LEVEL_PRINTLN(2, F("rtcmem_write_state: RTC memory write success"));
    return true;
  }
  else
  {
    DEBUG_PRINTLN(F("rtcmem_write_state: RTC memory write failure"));
  }
  return false;
};

bool rtcmem_write_state() {
  return rtcmem_write_state(true);
}

bool rtcmem_read_state() {
  if (ESP.rtcUserMemoryRead(RTCMEM_STATE_OFFSET, (uint32_t*) &rtcmem_state, sizeof(rtcmem_state))) {
    DEBUG_BY_LEVEL_PRINTLN(2, F("rtcmem_read_state: RTC memory read success"));
    DEBUG_BY_LEVEL_PRINT(4, F("rtcmem_read_state: CRC32 read from RTC ")); DEBUG_BY_LEVEL_PRINTLN(4,rtcmem_state.crc32, HEX);
    uint32_t crc32_calc = crc32((byte*) &rtcmem_state.data, sizeof(RTCMEM_STATE_DATA_S));
    DEBUG_BY_LEVEL_PRINT(4, F("rtcmem_read_state: CRC32 calculated to ")); DEBUG_BY_LEVEL_PRINTLN(4,crc32_calc, HEX);
    if (crc32_calc == rtcmem_state.crc32) {
      DEBUG_BY_LEVEL_PRINTLN(2, F("rtcmem_read_state: CRC32 matched"));
      return true;
    } else {
      DEBUG_PRINTLN(F("rtcmem_read_state: RTC CRC missmatch"));
    }
  }
  else
  {
    DEBUG_PRINTLN(F("rtcmem_read_state: RTC memory read failure"));
  }
  rtcmem_to_default_state();
  return false;
};

void ICACHE_FLASH_ATTR rtcmem_to_default_state() {
  DEBUG_PRINTLN(F("rtcmem_to_default_state: Setting rtcmem_state to default"));
  RTCMEM_STATE_S rtcmem_state_default;
  rtcmem_state = rtcmem_state_default;
}

#define STR_SATVAL(_val) ((_val<UMAXVALTYPE(_val))?String(_val):String(F("MAX")))
#define ADDSTR_STRUCT_MEM(_string, _struct, type, member, array) \
  IF_ELSE(IS_TASK_STAT_S(type)) ( \
    _string+=String(F("millis = ")) + STR_SATVAL(_struct.member.millis) + String(F(", ")); \
    _string+=String(F("cnt = ")) + STR_SATVAL(_struct.member.cnt) + String(F(", ")); \
    _string+=String(F("millis/cnt = ")); if ((_struct.member.cnt) && (_struct.member.cnt<UMAXVALTYPE(_struct.member.cnt)) && (_struct.member.millis<UMAXVALTYPE(_struct.member.millis))) _string+=String(_struct.member.millis/_struct.member.cnt); else _string+=String(F("NaN")); _string+=String(F(", ")); \
    _string+=String(F("errorsSinceBattery = ")) + STR_SATVAL(_struct.member.errorsSinceBattery) + String(F(", ")); \
    _string+=String(F("errorsSinceSuccess = ")) + STR_SATVAL(_struct.member.errorsSinceSuccess); \
    _string+=String(F("\n"));\
  )( \
    IF_ELSE(IS_bin_vector_16(type)) ( \
      _string+=String(F("0b")) + u20binstr(_struct.member) + String(F("\n")); \
    )( \
      IF_ELSE(IS_bin_vector_32(type)) ( \
        _string+=String(F("0b")) + u20binstr(_struct.member) + String(F("\n")); \
      )( \
        IF_ELSE(IS_voltage_16(type)) ( \
          _string+=String(float(_struct.member)/VCC_READ_1V) + String(F("V\n")); \
        )( \
          IF_ELSE(IS_timestamp_32(type)) ( \
            {\
              _string+=time_to_localtime_str(_struct.member) + String(F("\n")); \
            }\
          )( \
            IF_ELSE(IS_ipv4_32(type)) ( \
              {\
                ipv4_32 iptmp = _struct.member; \
                _string += String(iptmp&0xff); iptmp >>= 8; \
                _string += String(F(".")) + String(iptmp&0xff); iptmp >>= 8; \
                _string += String(F(".")) + String(iptmp&0xff); iptmp >>= 8; \
                _string += String(F(".")) + String(iptmp&0xff); \
                _string += String(F("\n"));\
              }\
            )( \
              IF_ELSE(IS_char(type)) ( \
                _string+=String(_struct.member) + String(F("\n")); \
              )( \
                IF_ELSE(array)( \
                  {_string+=String(_struct.member[0]);for(int _i=1;_i<(array);_i++){_string+=String(F(", ")) + String(_struct.member[_i]);} _string+=String(F("\n"));} \
                )( \
                  _string+=String(_struct.member) + String(F("\n")); \
                ) \
              ) \
            ) \
          ) \
        ) \
      ) \
    ) \
  )

void ICACHE_FLASH_ATTR rtcmem_token_add_to_string(String &rtcmem_str) {
  rtcmem_str += F("rtcmem_token\n");
  #define EXP_RTCMEM_TOKEN_MEMBER(type, member, array, init) \
    rtcmem_str+=String(F("-")) + String(F(#member)) + String(F(": ")); \
    ADDSTR_STRUCT_MEM(rtcmem_str, rtcmem_token, type, member, array)
  EXP_RTCMEM_TOKEN
  #undef EXP_RTCMEM_TOKEN_MEMBER
  DEBUG_BY_LEVEL_PRINT(3, F("rtcmem_str.length: ")); DEBUG_BY_LEVEL_PRINTLN(3, rtcmem_str.length());
}

void ICACHE_FLASH_ATTR rtcmem_state_add_to_string(String &rtcmem_str) {
  rtcmem_str += F("rtcmem_state.data\n");
  #define EXP_RTCMEM_STATE_DATA_MEMBER(type, member, array, init) \
    rtcmem_str+=String(F("-")) + String(F(#member)) + String(F(": ")); \
    ADDSTR_STRUCT_MEM(rtcmem_str, rtcmem_state.data, type, member, array)
  EXP_RTCMEM_STATE_DATA
  #undef EXP_RTCMEM_STATE_DATA_MEMBER
  DEBUG_BY_LEVEL_PRINT(3, F("rtcmem_str.length: ")); DEBUG_BY_LEVEL_PRINTLN(3, rtcmem_str.length());
}
#undef ADDSTR_STRUCT_MEM
#undef ADDSTR_STRUCT_MEM
