#include <EEPROM.h>

#include "eeprommem.h"
#include "debug_by_level.h"
#include "misc.h"
#include "crc32.h"

EEPROMMEM_CONFIG_S eeprommem_config;

bool eeprommem_write_config(bool calc_crc32) {
  if (calc_crc32) eeprommem_config.crc32 = crc32((byte*) &eeprommem_config.data, sizeof(EEPROMMEM_CONFIG_DATA_S));
  DEBUG_BY_LEVEL_PRINT(2, F("eeprommem_write_config: CRC32 calculated to ")); DEBUG_BY_LEVEL_PRINTLN(2,eeprommem_config.crc32, HEX);
  DEBUG_BY_LEVEL_PRINT(4, F("EEPROMMEM_SIZE = ")); DEBUG_BY_LEVEL_PRINTLN(4,EEPROMMEM_SIZE);
  EEPROM.begin(EEPROMMEM_SIZE);
  EEPROM.put(0, eeprommem_config);
  EEPROM.end();
  return true;
};

bool eeprommem_write_config() {
  eeprommem_write_config(true);
}

bool eeprommem_read_config() {
  DEBUG_BY_LEVEL_PRINT(4, F("EEPROMMEM_SIZE = ")); DEBUG_BY_LEVEL_PRINTLN(4,EEPROMMEM_SIZE);
  EEPROM.begin(EEPROMMEM_SIZE);
  EEPROM.get(0, eeprommem_config);
  EEPROM.end();
  DEBUG_BY_LEVEL_PRINTLN(2, F("eeprommem_read_config: EEPROM memory read success"));
  DEBUG_BY_LEVEL_PRINT(4, F("eeprommem_read_config: CRC32 read from EEPROM ")); DEBUG_BY_LEVEL_PRINTLN(4,eeprommem_config.crc32, HEX);
  uint32_t crc32_calc = crc32((byte*) &eeprommem_config.data, sizeof(EEPROMMEM_CONFIG_DATA_S));
  DEBUG_BY_LEVEL_PRINT(4, F("eeprommem_read_config: CRC32 calculated to ")); DEBUG_BY_LEVEL_PRINTLN(4,crc32_calc, HEX);
  if (crc32_calc == eeprommem_config.crc32) {
    DEBUG_BY_LEVEL_PRINTLN(2, F("eeprommem_read_config: CRC32 matched"));
    return true;
  } else {
    DEBUG_PRINTLN(F("eeprommem_read_config: eeprom CRC missmatch"));
  }
  eeprommem_to_default_config();
  return false;
};

void replace_n_chipid(char* s, size_t n) {
  char buf[100];
  snprintf(buf, NUMEL(buf), s, ESP.getChipId());
  strncpy(s, buf, n);
}

void ICACHE_FLASH_ATTR eeprommem_to_default_config() {
  DEBUG_PRINTLN(F("eeprommem_read_config: Setting eeprommem_config to default"));
  EEPROMMEM_CONFIG_S eeprommem_config_default;
  eeprommem_config = eeprommem_config_default;
  replace_n_chipid(eeprommem_config.data.hostname, NUMEL(eeprommem_config.data.hostname));
  replace_n_chipid(eeprommem_config.data.mqttTopic, NUMEL(eeprommem_config.data.mqttTopic));
  replace_n_chipid(eeprommem_config.data.mqttBatteryTopic, NUMEL(eeprommem_config.data.mqttBatteryTopic));
}


#define ADDSTR_STRUCT_MEM(_string, _struct, type, member, array) \
  IF_ELSE(IS_char(type))(\
    _string+=String(_struct.member) + String(F("\n")); \
  )( \
    IF_ELSE(array)( \
      {_string+=String(_struct.member[0]);for(int _i=1;_i<(array);_i++){_string+=String(F(", ")) + String(_struct.member[_i]);} _string+=String(F("\n"));} \
    )( \
      _string+=String(_struct.member) + String(F("\n")); \
    ) \
  )

void ICACHE_FLASH_ATTR eeprommem_config_add_to_string(String &eeprommem_str) {
    eeprommem_str += F("eeprommem_config.data\n");
    #define EXP_EEPROMMEM_CONFIG_DATA_MEMBER(type, member, array, init) \
      eeprommem_str+=String(F(".")) + String(F(#member)) + String(F(": ")); \
      ADDSTR_STRUCT_MEM(eeprommem_str, eeprommem_config.data, type, member, array)
    EXP_EEPROMMEM_CONFIG_DATA
    #undef EXP_EEPROMMEM_CONFIG_DATA_MEMBER
    DEBUG_BY_LEVEL_PRINT(3, F("eeprommem_str.length: ")); DEBUG_BY_LEVEL_PRINTLN(3, eeprommem_str.length());
}
#undef ADDSTR_STRUCT_MEM
