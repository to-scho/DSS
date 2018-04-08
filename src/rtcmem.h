#ifndef _RTCMEM
#define _RTCMEM
#include <Arduino.h>
#include "misc.h"

#define RTCMEM_TOKEN_FINGERPRINT 0xaa55

#define STATE_RFON (1<<31)
#define STATE_LONG_SLEEP (1<<30)

#define STATE_TASKS_ALWAYS_ONCE (0)
#define STATE_TASKS_ACQUIRE_RF (STATE_TASK_CONFIG_PORTAL | STATE_TASK_SENSOR_ALERT | STATE_TASK_SENSOR_UPDATE | STATE_TASK_SEND_MAIL | STATE_TASK_MQTT_RTCMEM_RESTORE)
#define STATE_TASKS (STATE_TASKS_ACQUIRE_RF | STATE_TASK_DHCP)

#define STATE_TASK_CONFIG_PORTAL (1<<0)
#define STATE_TASK_SENSOR_ALERT_CLOSED (1<<1)
#define STATE_TASK_SENSOR_ALERT_OPEN (1<<2)
#define STATE_TASK_SENSOR_ALERT_NO_CHANGE (1<<3)
#define STATE_TASK_SENSOR_ALERT (STATE_TASK_SENSOR_ALERT_CLOSED | STATE_TASK_SENSOR_ALERT_OPEN | STATE_TASK_SENSOR_ALERT_NO_CHANGE)
#define STATE_TASK_SENSOR_UPDATE (1<<4)
#define STATE_TASK_SEND_MAIL_STATUS_UPDATE (1<<5)
#define STATE_TASK_SEND_MAIL_SENSOR_UPDATE (1<<6)
#define STATE_TASK_SEND_MAIL (STATE_TASK_SEND_MAIL_STATUS_UPDATE|STATE_TASK_SEND_MAIL_SENSOR_UPDATE)
#define STATE_TASK_DHCP (1<<7)
#define STATE_TASK_MQTT_RTCMEM_RESTORE_NORMAL (1<<8)
#define STATE_TASK_MQTT_RTCMEM_RESTORE_SPECIAL (1<<9)
#define STATE_TASK_MQTT_RTCMEM_RESTORE (STATE_TASK_MQTT_RTCMEM_RESTORE_NORMAL|STATE_TASK_MQTT_RTCMEM_RESTORE_SPECIAL)

#define STATE_ERROR_ALL (STATE_ERROR_VCC | STATE_ERROR_ESPWWIFI | STATE_ERROR_WIFI | STATE_ERROR_MQTT | STATE_ERROR_HTTPUPDATE | STATE_ERROR_MAIL | STATE_ERROR_NTP)
#define STATE_ERROR_NOTIFY_BY_MAIL (STATE_ERROR_VCC | STATE_ERROR_ESPWWIFI | STATE_ERROR_MQTT | STATE_ERROR_HTTPUPDATE | STATE_ERROR_NTP)
#define STATE_ERROR_NOTIFY_BY_BUZZER (STATE_ERROR_ALL)

#define STATE_WARNING_ALL (STATE_ERROR_HTTPUPDATE_VCC)

#define STATE_ERROR_VCC (1<<16)
#define STATE_ERROR_ESPWWIFI (1<<17)
#define STATE_ERROR_WIFI (1<<18)
#define STATE_ERROR_MQTT (1<<19)
#define STATE_ERROR_HTTPUPDATE (1<<20)
#define STATE_ERROR_HTTPUPDATE_VCC (1<<21)
#define STATE_ERROR_MAIL (1<<22)
#define STATE_ERROR_NTP (1<<23)

/* user data structs -------------------------------------------------------- */
typedef uint16_t bin_vector_16;
#define IS_bin_vector_16(_type) __IS_PROBE(__CAT(___IS_bin_vector_16_, _type))
#define ___IS_bin_vector_16_bin_vector_16 __PROBE()
typedef uint32_t bin_vector_32;
#define IS_bin_vector_32(_type) __IS_PROBE(__CAT(___IS_bin_vector_32_, _type))
#define ___IS_bin_vector_32_bin_vector_32 __PROBE()
typedef uint16_t voltage_16;
#define IS_voltage_16(_type) __IS_PROBE(__CAT(___IS_voltage_16_, _type))
#define ___IS_voltage_16_voltage_16 __PROBE()
typedef uint32_t timestamp_32;
#define IS_timestamp_32(_type) __IS_PROBE(__CAT(___IS_timestamp_32_, _type))
#define ___IS_timestamp_32_timestamp_32 __PROBE()
typedef uint32_t ipv4_32;
#define IS_ipv4_32(_type) __IS_PROBE(__CAT(___IS_ipv4_32_, _type))
#define ___IS_ipv4_32_ipv4_32 __PROBE()

#define EXP_RTCMEM_TOKEN \
  EXP_RTCMEM_TOKEN_MEMBER(uint16_t, fingerprint, , RTCMEM_TOKEN_FINGERPRINT) \
  EXP_RTCMEM_TOKEN_MEMBER(uint16_t, sleep_cnt_wo_wifi, , 0x0000) \
  EXP_RTCMEM_TOKEN_MEMBER(bin_vector_16, gpio_last, , 0x0000) \
  EXP_RTCMEM_TOKEN_MEMBER(bin_vector_16, gpio_next, , 0x0000) \
  EXP_RTCMEM_TOKEN_MEMBER(uint32_t, EspWoWifiStat_millis, , 0) \
  EXP_RTCMEM_TOKEN_MEMBER(uint32_t, EspWoWifiStat_cnt, , 0) \
  EXP_RTCMEM_TOKEN_MEMBER(uint16_t, EspResetWdt_cnt, , 0) \
  EXP_RTCMEM_TOKEN_MEMBER(uint16_t, EspResetExcept_cnt, , 0) \
  EXP_RTCMEM_TOKEN_MEMBER(bin_vector_32, state, , (STATE_RFON|STATE_TASK_CONFIG_PORTAL|STATE_TASK_SEND_MAIL_STATUS_UPDATE|STATE_TASK_DHCP|STATE_TASK_SENSOR_UPDATE|STATE_TASK_MQTT_RTCMEM_RESTORE_NORMAL))

typedef struct tcmem_token_data_s {
  #define EXP_RTCMEM_TOKEN_MEMBER(type, member, array, init) IF_ELSE(array)(type member[array] = init)(type member = init);
  EXP_RTCMEM_TOKEN
  #undef  EXP_RTCMEM_TOKEN_MEMBER
} RTCMEM_TOKEN_S;

typedef struct task_stat {
  uint32_t millis;
  uint32_t cnt;
  uint16_t errorsSinceBattery;
  uint16_t errorsSinceSuccess;
} TASK_STAT_S;
#define TASK_STAT_S0 {0,0,0,0}
#define IS_TASK_STAT_S(_type) __IS_PROBE(__CAT(___IS_TASK_STAT_S_, _type))
#define ___IS_TASK_STAT_S_TASK_STAT_S __PROBE()

#define TASK_STAT_UPDATE(_taskStat, _success, _toc) \
  INCSAT(_taskStat.cnt); \
  UADDSAT(_taskStat.millis,(_toc)); \
  if (_success) \
    _taskStat.errorsSinceSuccess = 0; \
  else { \
    INCSAT(_taskStat.errorsSinceSuccess); \
    INCSAT(_taskStat.errorsSinceBattery); \
  }

#define BSSID_NONE {0,0,0,0,0,0}

#define EXP_RTCMEM_STATE_DATA \
  EXP_RTCMEM_STATE_DATA_MEMBER(timestamp_32, BootTime, , 0x0000) \
  EXP_RTCMEM_STATE_DATA_MEMBER(uint16_t, WiFiDhcpDwnCntMinutes, , 0x0000) \
  EXP_RTCMEM_STATE_DATA_MEMBER(uint8_t, WiFibssid, 6, BSSID_NONE) \
  EXP_RTCMEM_STATE_DATA_MEMBER(int32_t, WiFichannel, , -1) \
  EXP_RTCMEM_STATE_DATA_MEMBER(ipv4_32, ip, , 0) \
  EXP_RTCMEM_STATE_DATA_MEMBER(ipv4_32, gateway, , 0) \
  EXP_RTCMEM_STATE_DATA_MEMBER(ipv4_32, subnet, , 0) \
  EXP_RTCMEM_STATE_DATA_MEMBER(voltage_16, vccMin, , 0xffff) \
  EXP_RTCMEM_STATE_DATA_MEMBER(voltage_16, vccMax, , 0x0000) \
  EXP_RTCMEM_STATE_DATA_MEMBER(uint16_t, mailUpdateDwnCntHours, , 0x0000) \
  EXP_RTCMEM_STATE_DATA_MEMBER(uint16_t, mailOnErrorDwnCntHours, , 0x0000) \
  EXP_RTCMEM_STATE_DATA_MEMBER(uint16_t, httpOTAUpdateDwnCntHours, , 0x0000) \
  EXP_RTCMEM_STATE_DATA_MEMBER(uint16_t, rtcmemSaveToMqttDwnCntHours, , 0x0000) \
  EXP_RTCMEM_STATE_DATA_MEMBER(uint16_t, mailStatusCnt, , 0x0000) \
  EXP_RTCMEM_STATE_DATA_MEMBER(uint32_t, sensorChangeCnt, , 0x0000) \
  EXP_RTCMEM_STATE_DATA_MEMBER(TASK_STAT_S, EspWWifiStat, , TASK_STAT_S0) \
  EXP_RTCMEM_STATE_DATA_MEMBER(TASK_STAT_S, WiFiStat, , TASK_STAT_S0) \
  EXP_RTCMEM_STATE_DATA_MEMBER(TASK_STAT_S, mqttStat, , TASK_STAT_S0) \
  EXP_RTCMEM_STATE_DATA_MEMBER(TASK_STAT_S, httpUpdateStat, , TASK_STAT_S0) \
  EXP_RTCMEM_STATE_DATA_MEMBER(TASK_STAT_S, mailStat, , TASK_STAT_S0) \
  EXP_RTCMEM_STATE_DATA_MEMBER(TASK_STAT_S, ntpStat, , TASK_STAT_S0)
typedef struct tcmem_state_data_s {
    #define EXP_RTCMEM_STATE_DATA_MEMBER(type, member, array, init) IF_ELSE(array)(type member[array] = init)(type member = init);
    EXP_RTCMEM_STATE_DATA
    #undef  EXP_RTCMEM_STATE_DATA_MEMBER
  } RTCMEM_STATE_DATA_S;

/* user data structs -------------------------------------------------------- */

typedef struct rtcmem_state_s {
  RTCMEM_STATE_DATA_S data;
  uint32_t crc32 = 0x00000000;
} RTCMEM_STATE_S;

bool rtcmem_read_token();
bool rtcmem_write_token(bool set_fingerprint);
bool rtcmem_write_token();
void rtcmem_to_default_token();
void rtcmem_token_add_to_string(String &rtcmem_str);

bool rtcmem_read_state();
bool rtcmem_write_state(bool calc_crc32);
bool rtcmem_write_state();
void rtcmem_to_default_state();
void rtcmem_state_add_to_string(String &rtcmem_str);

extern RTCMEM_TOKEN_S rtcmem_token;
extern RTCMEM_STATE_S rtcmem_state;

#define CEILto4(__xx) ( ( ( (__xx) + 3 ) / 4 ) * 4 )

#define RTCMEM_TOKEN_OFFSET (CEILto4(0)/4)
#define RTCMEM_STATE_OFFSET (CEILto4(RTCMEM_TOKEN_OFFSET*4 + sizeof(RTCMEM_TOKEN_S))/4)
#define RTCMEM_SIZE (CEILto4(RTCMEM_STATE_OFFSET*4 + sizeof(RTCMEM_STATE_S))/4)
BUILD_BUG_ON(RTCMEM_SIZE > (512/4));

#endif
