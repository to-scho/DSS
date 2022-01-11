#ifndef _DEEP_SLEEP_SENSOR
#define _DEEP_SLEEP_SENSOR

#define PIN_LED 2
#define PIN_SENSOR_RESET_DISABLE 14
#define PIN_BUZZER 13
#define PIN_SENSOR_STATE 12

#ifndef DEEP_SLEEP_LONG_MIN
#define DEEP_SLEEP_LONG_MIN 60
#endif

#ifndef DEEP_SLEEP_DEBUG_FF
#define DEEP_SLEEP_DEBUG_FF 1
#endif

#define DEEP_SLEEP_LONG_S (DEEP_SLEEP_LONG_MIN * 60U)
#define DEEP_SLEEP_LONG_MS (DEEP_SLEEP_LONG_S * 1000U)
#define DEEP_SLEEP_LONG_US (DEEP_SLEEP_LONG_MS * 1000U)
#define DEEP_SLEEP_LONG_US_FF (DEEP_SLEEP_LONG_US/DEEP_SLEEP_DEBUG_FF)

#define DEEP_SLEEP_SHORT_US 1

#define WIFI_DHCP_DWN_CNT_MINUTES_STEP (DEEP_SLEEP_LONG_S/DHCP_COARSE_TIMER_SECS) // decrease DHCP lease timer each sleep, DHCP lease time is given in minutes (DHCP_COARSE_TIMER_SECS=60 seconds)
#define MQTT_SENSOR_UPDATE_HOURS (2*24) // send MQTT update at least each x hours
#define MAIL_UPDATE_HOURS (30*24) // send status mail at least each x hours
#define MAIL_ERROR_HOURS (2*24) // send repeated error mail at least each x hours
#define HTTP_OTA_UPDATE_HOURS (2*24) // OTA update search at least each x hours; set to "0" to check every time WiFi is up
#define RTCMEM_SAVE_TO_MQTT_HOURS (2*24) // Save RTC memory to MQTT each x hours


#define MAIL_FORCE_SEND_AFTER_NTP_ERRORS 3

#define BATTERY_MAH 1000
#define VCC_HTTP_UPDATE_MIN_V 2.7
#define VCC_ERROR_MIN_V 2.6
#define VCC_SHUTDOWN_MIN_V 2.5

#define MQTT_SEND_VCC_PLAIN true

#define VCC_READ_1V (1024.0*0.916) // fix for ESP8266-Arduino release 2.3.0 (SDK 1.5.3)
//#define VCC_READ_1V 1024.0
#define VCC_HTTP_UPDATE_MIN_READ ((uint16_t)(VCC_HTTP_UPDATE_MIN_V*VCC_READ_1V))
#define VCC_ERROR_MIN_READ ((uint16_t)(VCC_ERROR_MIN_V*VCC_READ_1V))
#define VCC_SHUTDOWN_MIN_READ ((uint16_t)(VCC_SHUTDOWN_MIN_V*VCC_READ_1V))

#define ERROR_TASK_STAT_CNT_ESPWWIFISTAT_SINCE_LAST_SUCCESS 3 // buzzer alert after fails
#define ERROR_TASK_STAT_CNT_WIFISTAT_SINCE_LAST_SUCCESS 2 // buzzer alert after fails
#define ERROR_TASK_STAT_CNT_MQTTSTAT_SINCE_LAST_SUCCESS 3 // mail alert after fails
#define ERROR_TASK_STAT_CNT_HTTPUPDATESTAT_SINCE_LAST_SUCCESS 5 // mail alert after fails
#define ERROR_TASK_STAT_CNT_MAILSTAT_SINCE_LAST_SUCCESS 5 // buzzer aler after fails
#define ERROR_TASK_STAT_CNT_NTPSTAT_SINCE_LAST_SUCCESS MAIL_FORCE_SEND_AFTER_NTP_ERRORS // mail alert after fails

#define MILLIS_OFFSET_WORF 209
#define MILLIS_OFFSET_WRF 189
#define MILLIS_OFFSET_UPDATE 30000U // time for OTA

#define CURRENT_UA_SLEEP 19
#define CURRENT_UA_WORF 25000
#define CURRENT_UA_WRF 80000

#define CURRENT_SECONDS_TO_MAH_SLEEP ((float)CURRENT_UA_SLEEP/3600000.0)
#define CURRENT_MILLIS_TO_MAH_WORF ((float)CURRENT_UA_WORF/3600000000.0)
#define CURRENT_MILLIS_TO_MAH_WRF ((float)CURRENT_UA_WRF/3600000000.0)

#endif
