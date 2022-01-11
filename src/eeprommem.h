#ifndef _EEPROMMEM
#define _EEPROMMEM
#include <Arduino.h>
#include "misc.h"
#include "deep_sleep_sensor.h"

#define POW2_CEIL(v) (1 + \
(((((((((v) - 1) | (((v) - 1) >> 0x10) | \
      (((v) - 1) | (((v) - 1) >> 0x10) >> 0x08)) | \
     ((((v) - 1) | (((v) - 1) >> 0x10) | \
      (((v) - 1) | (((v) - 1) >> 0x10) >> 0x08)) >> 0x04))) | \
   ((((((v) - 1) | (((v) - 1) >> 0x10) | \
      (((v) - 1) | (((v) - 1) >> 0x10) >> 0x08)) | \
     ((((v) - 1) | (((v) - 1) >> 0x10) | \
      (((v) - 1) | (((v) - 1) >> 0x10) >> 0x08)) >> 0x04))) >> 0x02))) | \
 ((((((((v) - 1) | (((v) - 1) >> 0x10) | \
      (((v) - 1) | (((v) - 1) >> 0x10) >> 0x08)) | \
     ((((v) - 1) | (((v) - 1) >> 0x10) | \
      (((v) - 1) | (((v) - 1) >> 0x10) >> 0x08)) >> 0x04))) | \
   ((((((v) - 1) | (((v) - 1) >> 0x10) | \
      (((v) - 1) | (((v) - 1) >> 0x10) >> 0x08)) | \
     ((((v) - 1) | (((v) - 1) >> 0x10) | \
      (((v) - 1) | (((v) - 1) >> 0x10) >> 0x08)) >> 0x04))) >> 0x02))) >> 0x01))))

#define EEPROMMEM_SIZE POW2_CEIL(sizeof(EEPROMMEM_CONFIG_S)) // bytes

/* user data structs -------------------------------------------------------- */

#define EXP_EEPROMMEM_CONFIG_DATA \
  EXP_EEPROMMEM_CONFIG_DATA_MEMBER(char, hostname,        24, "DSS_%06X") \
  EXP_EEPROMMEM_CONFIG_DATA_MEMBER(char, WiFissid,        33, "") \
  EXP_EEPROMMEM_CONFIG_DATA_MEMBER(char, WiFipass,        65, "") \
  EXP_EEPROMMEM_CONFIG_DATA_MEMBER(char, mqttHostname,    33, "") \
  EXP_EEPROMMEM_CONFIG_DATA_MEMBER(uint16_t, mqttPort,      , 1883) \
  EXP_EEPROMMEM_CONFIG_DATA_MEMBER(char, mqttTopic,       33, "stat/DSS_%06X") \
  EXP_EEPROMMEM_CONFIG_DATA_MEMBER(char, mqttBatteryTopic,33, "battery/DSS_%06X") \
  EXP_EEPROMMEM_CONFIG_DATA_MEMBER(char, smtpServer,      33, "") \
  EXP_EEPROMMEM_CONFIG_DATA_MEMBER(uint16_t, smtpPort,      , 465) \
  EXP_EEPROMMEM_CONFIG_DATA_MEMBER(char, smtpUser,        33, "") \
  EXP_EEPROMMEM_CONFIG_DATA_MEMBER(char, smtpPass,        65, "") \
  EXP_EEPROMMEM_CONFIG_DATA_MEMBER(char, smtpTo,          33, "") \
  EXP_EEPROMMEM_CONFIG_DATA_MEMBER(char, ntpServer,       33, "de.pool.ntp.org") \
  EXP_EEPROMMEM_CONFIG_DATA_MEMBER(char, httpUpdateServer,61, "") \
  EXP_EEPROMMEM_CONFIG_DATA_MEMBER(uint16_t, vcc_error_min_read,      , VCC_ERROR_MIN_READ) \
  EXP_EEPROMMEM_CONFIG_DATA_MEMBER(uint16_t, vcc_shutdown_min_read,   , VCC_SHUTDOWN_MIN_READ)

typedef struct eeprommem_config_data_s {
    #define EXP_EEPROMMEM_CONFIG_DATA_MEMBER(type, member, array, init) IF_ELSE(array)(type member[array] = init)(type member = init);
    EXP_EEPROMMEM_CONFIG_DATA
    #undef  EXP_EEPROMMEM_CONFIG_DATA_MEMBER
  } EEPROMMEM_CONFIG_DATA_S;

/* user data structs -------------------------------------------------------- */

typedef struct eeprommem_config_s {
  EEPROMMEM_CONFIG_DATA_S data;
  uint32_t crc32 = 0x00000000;
} EEPROMMEM_CONFIG_S;

bool eeprommem_read_config();
bool eeprommem_write_config(bool calc_crc32);
bool eeprommem_write_config();
void eeprommem_to_default_config();
void eeprommem_config_add_to_string(String &eeprommem_str);

extern EEPROMMEM_CONFIG_S eeprommem_config;

#endif
