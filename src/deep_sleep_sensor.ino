ADC_MODE(ADC_VCC); //vcc read-mode

#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <PubSubClient.h>        //https://github.com/Imroy/pubsubclient
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <Arduino.h>
#include <time.h>

#include "debug_by_level.h"
#include "tictoc.h"
#include "misc.h"
#include "deep_sleep_sensor.h"
#include "rtcmem.h"
#include "eeprommem.h"
#include "buzzer.h"
#include "blinker.h"
#include "WiFiManager.h"
#include "ESP8266SMTPClient.h"
#include "SNTPtime.h"
#include "fw_version.h"
#include "crc32.h"

extern "C" {
#include "user_interface.h" // RTC memory, RESET reason, ...
#include <uart_register.h> // Flush Serial FIFOs before deep sleep
#include <lwip/netif.h>
#include <lwip/dhcp.h>
#include <netif/wlan_lwip_if.h>
}

const TONE tone_wifi_manager_welcome[] = {{TONE_G, 200}, {TONE_OFF,  50}, {TONE_A,  50}, {TONE_OFF,  50}, {TONE_B, 150}, {0, 0}};
const TONE tone_wifi_manager_closed[]  = {{TONE_B, 200}, {TONE_OFF,  50}, {TONE_A,  50}, {TONE_OFF,  50}, {TONE_G, 150}, {0, 0}};
const TONE tone_ota_update[]           = {{TONE_G, 100}, {TONE_OFF,  50}, {TONE_B, 100}, {TONE_OFF, 200}, {TONE_G, 100}, {TONE_OFF,  50}, {TONE_B, 100}, {0, 0}};
const TONE tone_restore_rtcmem[]       = {{TONE_B, 100}, {TONE_OFF,  50}, {TONE_G, 100}, {TONE_OFF, 200}, {TONE_B, 100}, {TONE_OFF,  50}, {TONE_G, 100}, {0, 0}};
const TONE tone_error[]                = {{TONE_B, 150}, {TONE_OFF,  50}, {TONE_G,  500}, {0, 0}};

//flag for saving data from WifiManager
bool shouldSaveConfig = false;

// WifiManager callback notifying us of the need to save config
void saveConfigCallback () {
  DEBUG_PRINTLN(F("WifiManager should save config"));
  shouldSaveConfig = true;
}

// WifiManager callback notifying us of the need to reset RTC & EEPROM
void resetCallback () {
  DEBUG_PRINTLN(F("WifiManager reset config"));
  ESP.eraseConfig();
  rtcmem_to_default_token();
  rtcmem_write_token(false);
  rtcmem_to_default_state();
  rtcmem_write_state(false);
  eeprommem_to_default_config();
  eeprommem_write_config(false);
}

void httpUdateServerCheck(uint16_t vcc) {
  bool result = true;
  if (eeprommem_config.data.httpUpdateServer[0]!='\0') {
    TIC;
    if (vcc >= VCC_HTTP_UPDATE_MIN_READ) {
      rtcmem_token.state &= ~STATE_ERROR_HTTPUPDATE_VCC;
    }
    rtcmem_state.data.httpOTAUpdateDwnCntHours = HTTP_OTA_UPDATE_HOURS;

    String fwURL = String(eeprommem_config.data.httpUpdateServer);
    String fwVersionURL = fwURL;
    fwVersionURL.concat(F("/firmware.fwv"));

    DEBUG_PRINTLN(F("Checking for firmware updates."));
    DEBUG_BY_LEVEL_PRINT(4, F("Firmware version URL: ")); DEBUG_BY_LEVEL_PRINTLN(4,fwVersionURL);

    HTTPClient httpClient;
    httpClient.begin(fwVersionURL);
    int httpCode = httpClient.GET();
    if(httpCode == 200) {
      String newFWVersion_str = httpClient.getString();
      unsigned long new_fw_version = fw_version_char2ul(newFWVersion_str.c_str());

      DEBUG_BY_LEVEL_PRINT(4, F("Current firmware version: ")); DEBUG_BY_LEVEL_PRINTLN(4, fw_version_ul2char(fw_version,8));
      DEBUG_BY_LEVEL_PRINT(4, F("Available firmware version: ")); DEBUG_BY_LEVEL_PRINTLN(4, fw_version_ul2char(new_fw_version,8));

      if ((new_fw_version > fw_version) | (new_fw_version==0xFFFFFFFF)) {
        if (vcc >= VCC_HTTP_UPDATE_MIN_READ) {
          TASK_STAT_S EspWWifiStat_bak = rtcmem_state.data.EspWWifiStat;
          TASK_STAT_UPDATE(rtcmem_state.data.EspWWifiStat, true, millis()+MILLIS_OFFSET_UPDATE);
          bool mqttSave_success = true;
          if (!(rtcmem_token.state & STATE_TASK_MQTT_RTCMEM_RESTORE)) mqttSave_success = mqttSaveRestoreRtcMem(0);
          rtcmem_state.data.EspWWifiStat = EspWWifiStat_bak; // restore for case when no update/reset is done next
          if ((mqttSave_success)||(rtcmem_state.data.mqttStat.errorsSinceSuccess>5)) {
            buzzer_music(tone_ota_update);
            DEBUG_PRINT(F("Preparing to update to firmware version: ")); DEBUG_PRINTLN(fw_version_ul2char(new_fw_version,8));
            String fwImageURL = fwURL;
            fwImageURL.concat( F("/firmware.bin") );
            delay(1000); // allow buzzer to finish!
            DEBUG_PRINTLN(F("go!"));
            t_httpUpdate_return ret = ESPhttpUpdate.update(fwImageURL);
            switch(ret) {
              case HTTP_UPDATE_FAILED:
                DEBUG_PRINT(F("HTTP_UPDATE_FAILD Error (")); DEBUG_PRINT(ESPhttpUpdate.getLastError()); DEBUG_PRINT(F("): ")); DEBUG_PRINTLN(ESPhttpUpdate.getLastErrorString());
                result = false;
                break;

              case HTTP_UPDATE_NO_UPDATES:
                DEBUG_PRINTLN(F("HTTP_UPDATE_NO_UPDATES"));
                break;
            }
          } else {
            DEBUG_PRINTLN(F("Failed to store RTCMEM to MQTT"));
          }
        } else {
          DEBUG_PRINTLN(F("Insufficient VCC voltage for FW update!"));
          //result = false;
          rtcmem_token.state |= STATE_ERROR_HTTPUPDATE_VCC;
        }
      } else {
        DEBUG_BY_LEVEL_PRINTLN(4, F("Already on latest version"));
      }
    } else {
      DEBUG_PRINT(F("Firmware version check failed, got HTTP response code ")); DEBUG_PRINTLN(httpCode);
      result = false;
    }
    httpClient.end();
    TASK_STAT_UPDATE(rtcmem_state.data.httpUpdateStat, result, TOC);
  }
}

Ticker led_ticker;

void led_flash(bool onoff)
{
  // digitalWrite(PIN_LED, !digitalRead(PIN_LED));
  if (onoff) {
    digitalWrite(PIN_LED, LOW);
    led_ticker.once_ms(50, led_flash, false);
  } else {
    digitalWrite(PIN_LED, HIGH);
    led_ticker.once_ms(950, led_flash, true);
  }
}

uint8_t SensorInterruptCnt;
//ICACHE_RAM_ATTR
void handleSensorInterrupt() {
  INCSAT(SensorInterruptCnt);
  blinker_set_onoff_num(5);
}

void handle_rfon() {
  buzzer_set_pin(PIN_BUZZER);
  blinker_set_pin(PIN_LED);

  // read eeprom config
  eeprommem_read_config();
  if ((rtcmem_token.state & STATE_TASK_CONFIG_PORTAL)||(eeprommem_config.data.WiFissid[0] == '\0')) {
    DEBUG_PRINTLN(F("Battery inserted.........."));
    DEBUG_BY_LEVEL_PRINT(3,F("ESP.getBootMode() = ")); DEBUG_BY_LEVEL_PRINTLN(3, ESP.getBootMode());
    DEBUG_BY_LEVEL_PRINT(3,F("ESP.getSdkVersion() = ")); DEBUG_BY_LEVEL_PRINTLN(3, ESP.getSdkVersion());
    DEBUG_BY_LEVEL_PRINT(3,F("ESP.getBootVersion() = ")); DEBUG_BY_LEVEL_PRINTLN(3, ESP.getBootVersion());
    DEBUG_BY_LEVEL_PRINT(3,F("ESP.getChipId() = ")); DEBUG_BY_LEVEL_PRINTLN(3, ESP.getChipId());
    DEBUG_BY_LEVEL_PRINT(3,F("ESP.getFlashChipSize() = ")); DEBUG_BY_LEVEL_PRINTLN(3, ESP.getFlashChipSize());
    DEBUG_BY_LEVEL_PRINT(3,F("ESP.getFlashChipRealSize() = ")); DEBUG_BY_LEVEL_PRINTLN(3, ESP.getFlashChipRealSize());
    DEBUG_BY_LEVEL_PRINT(3,F("ESP.getFlashChipSizeByChipId() = ")); DEBUG_BY_LEVEL_PRINTLN(3, ESP.getFlashChipSizeByChipId());
    DEBUG_BY_LEVEL_PRINT(3,F("ESP.getFlashChipId() = ")); DEBUG_BY_LEVEL_PRINTLN(3, ESP.getFlashChipId());

    // Connect Wifi w/ WifiManager
    rtcmem_token.state &= ~STATE_TASK_CONFIG_PORTAL;
    rtcmem_write_token(); // when WifiManager is interrupted by reset do not show up again
    blinker_set_add_onoff_num_per_wifi_station(1);
    blinker_start(1,50,0,1000);
    buzzer_music(tone_wifi_manager_welcome);
    // RTC state to default
    rtcmem_to_default_state();
    // show up WiFi Manager
    DEBUG_BY_LEVEL_PRINT(3,F("ESP.getFreeHeap(@line ")); DEBUG_BY_LEVEL_PRINT(3,__LINE__); DEBUG_BY_LEVEL_PRINT(3,F(") = ")); DEBUG_BY_LEVEL_PRINTLN(3, ESP.getFreeHeap());

    delay(500); // avoid noisy alert after battery insertion
    SensorInterruptCnt = 0;
    attachInterrupt(digitalPinToInterrupt(PIN_SENSOR_STATE), handleSensorInterrupt, CHANGE);
    callWiFiManager();
    detachInterrupt(digitalPinToInterrupt(PIN_SENSOR_STATE));
    DEBUG_PRINT(F("Sensor state changes during WiFi manager: ")); DEBUG_PRINTLN(SensorInterruptCnt);
    if (SensorInterruptCnt) rtcmem_token.state |= STATE_TASK_MQTT_RTCMEM_RESTORE_SPECIAL;
    buzzer_music(tone_wifi_manager_closed);
    blinker_stop();
    blinker_set_add_onoff_num_per_wifi_station(0);
  } else {
    rtcmem_read_state();
  }
  USUBSAT(rtcmem_state.data.WiFiDhcpDwnCntMinutes, rtcmem_token.sleep_cnt_wo_wifi * WIFI_DHCP_DWN_CNT_MINUTES_STEP);
  if (rtcmem_state.data.WiFiDhcpDwnCntMinutes==0) rtcmem_token.state |= STATE_TASK_DHCP;
  USUBSAT(rtcmem_state.data.mailUpdateDwnCntHours, rtcmem_token.sleep_cnt_wo_wifi);
  if (rtcmem_state.data.mailUpdateDwnCntHours==0) {
    rtcmem_token.state |= STATE_TASK_SEND_MAIL_STATUS_UPDATE;
    rtcmem_state.data.mailUpdateDwnCntHours = MAIL_UPDATE_HOURS;
  }
  USUBSAT(rtcmem_state.data.mailOnErrorDwnCntHours, rtcmem_token.sleep_cnt_wo_wifi);
  USUBSAT(rtcmem_state.data.httpOTAUpdateDwnCntHours, rtcmem_token.sleep_cnt_wo_wifi);
  USUBSAT(rtcmem_state.data.rtcmemSaveToMqttDwnCntHours, rtcmem_token.sleep_cnt_wo_wifi);

  rtcmem_token.sleep_cnt_wo_wifi = 0;
}

void callWiFiManager()
{
  WiFiManager wifiManager;
  char buf_port[6];

  #if DEBUG_LEVEL == 0
  wifiManager.setDebugOutput(false);
  #endif

  // timeout - this will quit WiFiManager if it's not configured in 1 minute
  wifiManager.setConfigPortalTimeout(60);

  // Custom Parameters
  String custom_fw_version_text_custom_str = F("<br/>Deep Sleep Sensor, FW version ");
  custom_fw_version_text_custom_str += fw_version_ul2char(fw_version,8);
  DEBUG_PRINTLN(custom_fw_version_text_custom_str);
  WiFiManagerParameter custom_fw_version_text(custom_fw_version_text_custom_str.c_str());
  wifiManager.addParameter(&custom_fw_version_text);

  String custom_hostname_id_str = String(F("Hostname"));
  String custom_hostname_ph_str = String(F("Client ID"));
  WiFiManagerParameter custom_hostname(custom_hostname_id_str.c_str(), custom_hostname_ph_str.c_str(), eeprommem_config.data.hostname, NUMEL(eeprommem_config.data.hostname));
  wifiManager.addParameter(&custom_hostname);

  String custom_mqtt_text_custom_str = String(F("<br/>MQTT config. <br/> No url to use smtp fallback."));
  WiFiManagerParameter custom_mqtt_text(custom_mqtt_text_custom_str.c_str());
  wifiManager.addParameter(&custom_mqtt_text);

  String custom_mqtt_hostname_id_str = String(F("mqtt-hostname"));
  String custom_mqtt_hostname_ph_str = String(F("Hostname"));
  WiFiManagerParameter custom_mqtt_hostname(custom_mqtt_hostname_id_str.c_str(), custom_mqtt_hostname_ph_str.c_str(), eeprommem_config.data.mqttHostname, NUMEL(eeprommem_config.data.mqttHostname));
  wifiManager.addParameter(&custom_mqtt_hostname);

  snprintf(buf_port, NUMEL(buf_port), "%d", eeprommem_config.data.mqttPort);
  String custom_mqtt_port_id_str = String(F("mqtt-port"));
  String custom_mqtt_port_ph_str = String(F("Port"));
  WiFiManagerParameter custom_mqtt_port(custom_mqtt_port_id_str.c_str(), custom_mqtt_port_ph_str.c_str(), buf_port, NUMEL(buf_port));
  wifiManager.addParameter(&custom_mqtt_port);

  String custom_mqtt_topic_id_str = String(F("mqtt-topic"));
  String custom_mqtt_topic_ph_str = String(F("Topic"));
  WiFiManagerParameter custom_mqtt_topic(custom_mqtt_topic_id_str.c_str(), custom_mqtt_topic_ph_str.c_str(), eeprommem_config.data.mqttTopic, NUMEL(eeprommem_config.data.mqttHostname));
  wifiManager.addParameter(&custom_mqtt_topic);

  String custom_mqtt_battery_topic_id_str = String(F("mqtt-battery-topic"));
  String custom_mqtt_battery_topic_ph_str = String(F("BatteryTopic"));
  WiFiManagerParameter custom_mqtt_battery_topic(custom_mqtt_battery_topic_id_str.c_str(), custom_mqtt_battery_topic_ph_str.c_str(), eeprommem_config.data.mqttBatteryTopic, NUMEL(eeprommem_config.data.mqttBatteryTopic));
  wifiManager.addParameter(&custom_mqtt_battery_topic);

  String custom_mqtt_restore_text_custom_str = String(F("<br/>Resore RTC memory from MQTT. <br/>Any input to restore. Empty to reset. <br/> Leave empty when new battery is inserted."));
  WiFiManagerParameter custom_mqtt_restore_text(custom_mqtt_restore_text_custom_str.c_str());
  wifiManager.addParameter(&custom_mqtt_restore_text);

  String custom_mqtt_restore_yn_id_str = String(F("restore-rtc"));
  String custom_mqtt_restore_yn_ph_str = String(F("RestoreStatisticFromMQTT"));
  WiFiManagerParameter custom_mqtt_restore_yn(custom_mqtt_restore_yn_id_str.c_str(), custom_mqtt_restore_yn_ph_str.c_str(), "", 2);
  wifiManager.addParameter(&custom_mqtt_restore_yn);

  String custom_smtp_text_custom_str = String(F("<br/>SMTP config. <br/> No url to disable."));
  WiFiManagerParameter custom_smtp_text(custom_smtp_text_custom_str.c_str());
  wifiManager.addParameter(&custom_smtp_text);

  String custom_smtp_server_id_str = String(F("smtp-server"));
  String custom_smtp_server_ph_str = String(F("Server"));
  WiFiManagerParameter custom_smtp_server(custom_smtp_server_id_str.c_str(), custom_smtp_server_ph_str.c_str(), eeprommem_config.data.smtpServer, NUMEL(eeprommem_config.data.smtpServer));
  wifiManager.addParameter(&custom_smtp_server);

  snprintf(buf_port, NUMEL(buf_port), "%d", eeprommem_config.data.smtpPort);
  String custom_smtp_port_id_str = String(F("smtp-port"));
  String custom_smtp_port_ph_str = String(F("Port"));
  WiFiManagerParameter custom_smtp_port(custom_smtp_port_id_str.c_str(), custom_smtp_port_ph_str.c_str(), buf_port, NUMEL(buf_port));
  wifiManager.addParameter(&custom_smtp_port);

  String custom_smtp_user_id_str = String(F("smtp-user"));
  String custom_smtp_user_ph_str = String(F("User"));
  WiFiManagerParameter custom_smtp_user(custom_smtp_user_id_str.c_str(), custom_smtp_user_ph_str.c_str(), eeprommem_config.data.smtpUser, NUMEL(eeprommem_config.data.smtpUser));
  wifiManager.addParameter(&custom_smtp_user);

  String custom_smtp_pass_id_str = String(F("smtp-pass"));
  String custom_smtp_pass_ph_str = String(F("Pass"));
  String custom_smtp_pass_default_str = (eeprommem_config.data.smtpPass[0]!='\0')?String(F("********")):String(F(""));
  WiFiManagerParameter custom_smtp_pass(custom_smtp_pass_id_str.c_str(), custom_smtp_pass_ph_str.c_str(), custom_smtp_pass_default_str.c_str(), NUMEL(eeprommem_config.data.smtpPass));
  wifiManager.addParameter(&custom_smtp_pass);

  String custom_smtp_to_id_str = String(F("smtp-to"));
  String custom_smtp_to_ph_str = String(F("Recipient"));
  WiFiManagerParameter custom_smtp_to(custom_smtp_to_id_str.c_str(), custom_smtp_to_ph_str.c_str(), eeprommem_config.data.smtpTo, NUMEL(eeprommem_config.data.smtpTo));
  wifiManager.addParameter(&custom_smtp_to);

  String custom_ntp_text_custom_str = String(F("<br/>NTP config. <br/> No url to disable."));
  WiFiManagerParameter custom_ntp_text(custom_ntp_text_custom_str.c_str());
  wifiManager.addParameter(&custom_ntp_text);

  String custom_ntp_server_id_str = String(F("ntp-server"));
  String custom_ntp_server_ph_str = String(F("Server"));
  WiFiManagerParameter custom_ntp_server(custom_ntp_server_id_str.c_str(), custom_ntp_server_ph_str.c_str(), eeprommem_config.data.ntpServer, NUMEL(eeprommem_config.data.ntpServer));
  wifiManager.addParameter(&custom_ntp_server);

  String custom_http_update_text_custom_str = String(F("<br/>HTTP update server config. <br/> No url to disable."));
  WiFiManagerParameter custom_http_update_text(custom_http_update_text_custom_str.c_str());
  wifiManager.addParameter(&custom_http_update_text);

  String custom_http_update_server_id_str = String(F("http-update-server"));
  String custom_http_update_server_ph_str = String(F("Server"));
  WiFiManagerParameter custom_http_update_server(custom_http_update_server_id_str.c_str(), custom_http_update_server_ph_str.c_str(), eeprommem_config.data.httpUpdateServer, NUMEL(eeprommem_config.data.httpUpdateServer));
  wifiManager.addParameter(&custom_http_update_server);

  // set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  // set reset callback
  wifiManager.setResetCallback(resetCallback);

  // sett SSID and PASS from already saved config
  wifiManager.setSSID(eeprommem_config.data.WiFissid);
  wifiManager.setPassword(eeprommem_config.data.WiFipass);
  wifiManager.setBreakAfterConfig(true); // if not true 'saveConfigCallback' is internally only called if WiFi.status() == WL_CONNECTED; set to true to allow saving of custom parameters even when no successful WiFi conection is established

  wifi_station_set_hostname(eeprommem_config.data.hostname);

  wifiManager.startConfigPortal(eeprommem_config.data.hostname); // return value is (WiFi.status() == WL_CONNECTED)
  DEBUG_PRINTLN(F("WifiManager config portal closed"));

  if (shouldSaveConfig) {
    DEBUG_PRINTLN(F("Saving config from WifiManager to EEPROM"));
    strcpy(eeprommem_config.data.hostname, custom_hostname.getValue());
    strcpy(eeprommem_config.data.mqttHostname, custom_mqtt_hostname.getValue());
    strcpy(eeprommem_config.data.mqttTopic, custom_mqtt_topic.getValue());
    eeprommem_config.data.mqttPort = atoi(custom_mqtt_port.getValue());
    strcpy(eeprommem_config.data.mqttBatteryTopic, custom_mqtt_battery_topic.getValue());
    if (custom_mqtt_restore_yn.getValue()[0]!='\0') rtcmem_token.state |= STATE_TASK_MQTT_RTCMEM_RESTORE_SPECIAL;
    strcpy(eeprommem_config.data.smtpServer, custom_smtp_server.getValue());
    eeprommem_config.data.smtpPort = atoi(custom_smtp_port.getValue());
    strcpy(eeprommem_config.data.smtpUser, custom_smtp_user.getValue());
    if (String(custom_smtp_pass.getValue())!=custom_smtp_pass_default_str) strcpy(eeprommem_config.data.smtpPass, custom_smtp_pass.getValue());
    strcpy(eeprommem_config.data.smtpTo, custom_smtp_to.getValue());
    strcpy(eeprommem_config.data.ntpServer, custom_ntp_server.getValue());
    strcpy(eeprommem_config.data.httpUpdateServer, custom_http_update_server.getValue());

    if (wifiManager.getSSID().c_str()[0]!='\0') {
      DEBUG_PRINTLN(F("WifiManager new SSID & PASS configured"));
      strncpy(eeprommem_config.data.WiFissid, wifiManager.getSSID().c_str(), sizeof(eeprommem_config.data.WiFissid));
      strncpy(eeprommem_config.data.WiFipass, wifiManager.getPassword().c_str(), sizeof(eeprommem_config.data.WiFipass));
    }
    eeprommem_write_config();
  }

  if (eeprommem_config.data.WiFissid[0] == '\0') {
     DEBUG_PRINTLN(F("No WiFi credentials known, going for WiFi Manager config portal after restart again"));
     TASK_STAT_UPDATE(rtcmem_state.data.EspWWifiStat, false, millis()+MILLIS_OFFSET_WRF);
     rtcmem_write_state();
     ESP.reset();
     delay(1000);
  }

  if (WiFi.status() == WL_CONNECTED) {
    WiFiSaveDhcpBssidToRTC();
    rtcmem_token.state &= ~STATE_TASK_DHCP;
  } else {
    rtcmem_token.state |= STATE_TASK_DHCP;
  }
}

void checkForErrors(uint16_t vcc) {
  if (vcc < rtcmem_state.data.vccMin) rtcmem_state.data.vccMin = vcc;
  if (vcc > rtcmem_state.data.vccMax) rtcmem_state.data.vccMax = vcc;

  // check for errors needing attention
  uint32_t state_error_tmp = 0;
  // check errors that are signalled by Mail
  if (vcc < VCC_ERROR_MIN_V) {
    state_error_tmp |= STATE_ERROR_VCC;
    DEBUG_PRINT(F("VCC is low: ")); DEBUG_PRINT((float)vcc/VCC_READ_1V); DEBUG_PRINTLN(F("V\n"));
  }
  if (rtcmem_state.data.EspWWifiStat.errorsSinceSuccess>=ERROR_TASK_STAT_CNT_ESPWWIFISTAT_SINCE_LAST_SUCCESS) {
    state_error_tmp |= STATE_ERROR_ESPWWIFI;
    DEBUG_PRINTLN(F("Too many general errors preventing RF sleep in a row!"));
  }
  if (rtcmem_state.data.WiFiStat.errorsSinceSuccess>=ERROR_TASK_STAT_CNT_WIFISTAT_SINCE_LAST_SUCCESS) {
    state_error_tmp |= STATE_ERROR_WIFI;
    DEBUG_PRINTLN(F("Too many WiFi errors in a row!"));
  }
  if (rtcmem_state.data.mqttStat.errorsSinceSuccess>=ERROR_TASK_STAT_CNT_MQTTSTAT_SINCE_LAST_SUCCESS) {
    state_error_tmp |= STATE_ERROR_MQTT;
    DEBUG_PRINTLN(F("Too many MQTT errors in a row!"));
  }
  if (rtcmem_state.data.httpUpdateStat.errorsSinceSuccess>=ERROR_TASK_STAT_CNT_HTTPUPDATESTAT_SINCE_LAST_SUCCESS) {
    state_error_tmp |= STATE_ERROR_HTTPUPDATE;
    DEBUG_PRINTLN(F("Too many HttpUpdate errors in a row!"));
  }
  if (rtcmem_state.data.mailStat.errorsSinceSuccess>=ERROR_TASK_STAT_CNT_MAILSTAT_SINCE_LAST_SUCCESS) {
    state_error_tmp |= STATE_ERROR_MAIL;
    DEBUG_PRINTLN(F("Too many Mail errors in a row!"));
  }
  if (rtcmem_state.data.ntpStat.errorsSinceSuccess>=ERROR_TASK_STAT_CNT_NTPSTAT_SINCE_LAST_SUCCESS) {
    state_error_tmp |= STATE_ERROR_NTP;
    DEBUG_PRINTLN(F("Too many NTP errors in a row!"));
  }

  if ((state_error_tmp & STATE_ERROR_NOTIFY_BY_MAIL) && ((rtcmem_state.data.mailOnErrorDwnCntHours==0)||((state_error_tmp & (~rtcmem_token.state) & STATE_ERROR_NOTIFY_BY_MAIL)))) {
    // errors need to be signalled by Mail
    rtcmem_state.data.mailOnErrorDwnCntHours = MAIL_ERROR_HOURS;
    rtcmem_token.state |= STATE_TASK_SEND_MAIL_STATUS_UPDATE;
  }
  if (state_error_tmp & STATE_ERROR_NOTIFY_BY_BUZZER) {
    // errors need to be signalled by Buzzer
    buzzer_music(tone_error);
    blinker_start(10,50,1,0);
    delay(700);
  }

  rtcmem_token.state &= ~STATE_ERROR_ALL;
  rtcmem_token.state |= state_error_tmp;
}

void WiFiSaveDhcpBssidToRTC() {
  struct netif *sta_netif = (struct netif *)eagle_lwip_getif(0x00);
  rtcmem_state.data.WiFiDhcpDwnCntMinutes = (sta_netif->dhcp->t1_timeout<=UMAXVALTYPE(rtcmem_state.data.WiFiDhcpDwnCntMinutes))?sta_netif->dhcp->t1_timeout:UMAXVALTYPE(rtcmem_state.data.WiFiDhcpDwnCntMinutes);
  rtcmem_state.data.WiFichannel = WiFi.channel();
  os_memcpy(rtcmem_state.data.WiFibssid, WiFi.BSSID(), sizeof(rtcmem_state.data.WiFibssid));
  rtcmem_state.data.ip = WiFi.localIP();
  rtcmem_state.data.gateway = WiFi.gatewayIP();
  rtcmem_state.data.subnet = WiFi.subnetMask();
}

void connectWiFi(){
  // Connect Wifi
  TIC;
  bool success = false;

  // Connect to last WiFi network
  WiFi.mode(WIFI_STA);
  DEBUG_PRINT(F("Connecting to \"")); DEBUG_PRINTF(eeprommem_config.data.WiFissid);
  if (rtcmem_token.state & STATE_TASK_DHCP) {
    DEBUG_PRINTLN(F("\" using DHCP/ChannelScan"));
    WiFi.config(0U, 0U, 0U); // use DHCP again if static before
    WiFi.begin(eeprommem_config.data.WiFissid, eeprommem_config.data.WiFipass);
  } else {
    DEBUG_PRINTLN(F("\" using last IP/Gateway/Subnet/Channel/Bssid"));
    WiFi.config(IPAddress(rtcmem_state.data.ip), IPAddress(rtcmem_state.data.gateway), IPAddress(rtcmem_state.data.subnet));
    WiFi.begin(eeprommem_config.data.WiFissid, eeprommem_config.data.WiFipass, rtcmem_state.data.WiFichannel, rtcmem_state.data.WiFibssid);
  }

  wifi_station_set_hostname(eeprommem_config.data.hostname);

  uint32_t time1 = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(10);
    DEBUG_PRINT(F("."));
    if (((millis() - time1)) > 10*1000U) { // wifi connection lasts too ling, retry
      DEBUG_PRINT(F("timeout"))
      break;
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    if (rtcmem_token.state & STATE_TASK_DHCP) {
      WiFiSaveDhcpBssidToRTC();
    }
    DEBUG_PRINT(F("\nWiFi connected @")); DEBUG_PRINT(millis()); DEBUG_PRINTLN(F("ms"));
    DEBUG_PRINT(F("SSID:    ")); DEBUG_PRINTLN(WiFi.SSID());
    DEBUG_PRINT(F("PASS:    ")); DEBUG_PRINTLN(WiFi.psk());
    DEBUG_PRINT(F("IP:      ")); DEBUG_PRINTLN(WiFi.localIP());
    DEBUG_PRINT(F("GATEWAY: ")); DEBUG_PRINTLN(WiFi.gatewayIP());
    DEBUG_PRINT(F("SUBNET:  ")); DEBUG_PRINTLN(WiFi.subnetMask());
    rtcmem_token.state &= ~STATE_TASK_DHCP;
    success = true;
  } else {
    WiFi.disconnect();
    delay(10);
    WiFi.forceSleepBegin();
    delay(10);
    WiFi.forceSleepWake();
    delay(10);
    rtcmem_token.state |= STATE_TASK_DHCP;
  }
  TASK_STAT_UPDATE(rtcmem_state.data.WiFiStat, success, TOC);
}

#define MQTT_RTCMEM_STORE_SIZE (sizeof(fw_version) + sizeof(time_t) + sizeof(rtcmem_token) + sizeof(rtcmem_state))
BUILD_BUG_ON(MQTT_RTCMEM_STORE_SIZE > MQTT_MAX_PACKET_SIZE);
void mqttCallback(char* ctopic, byte* bpayload, unsigned int length) {
  String mqttRtcMemTopic = String(F("rtcmem/")) + String(eeprommem_config.data.hostname);
  String topic(ctopic);
  DEBUG_PRINT(F("MQTT message received with topic '")); DEBUG_PRINT(ctopic); DEBUG_PRINT(F("' having ")); DEBUG_PRINT(length); DEBUG_PRINTLN(F("bytes"));
  if (topic == mqttRtcMemTopic) {
    DEBUG_PRINTLN(F("RTCMEM message found, trying to restore"));
    if (length==MQTT_RTCMEM_STORE_SIZE) {
      uint8_t *payload_buf_ptr = bpayload;
      bool rtcmem_match = true;
      unsigned long fw_version_buf;
      uint32_t unixTime_buf;
      RTCMEM_TOKEN_S rtcmem_token_buf;
      RTCMEM_STATE_S rtcmem_state_buf;
      os_memcpy(&fw_version_buf,   payload_buf_ptr, sizeof(fw_version));    payload_buf_ptr += sizeof(fw_version);
      os_memcpy(&unixTime_buf,     payload_buf_ptr, sizeof(unixTime_buf));  payload_buf_ptr += sizeof(unixTime_buf);
      os_memcpy(&rtcmem_token_buf, payload_buf_ptr, sizeof(rtcmem_token));  payload_buf_ptr += sizeof(rtcmem_token);
      os_memcpy(&rtcmem_state_buf, payload_buf_ptr, sizeof(rtcmem_state));  payload_buf_ptr += sizeof(rtcmem_state);
      DEBUG_PRINT(F("Received RTCMEM from FW version ")); DEBUG_PRINT(fw_version_ul2char(fw_version_buf,8)); DEBUG_PRINT(F(" saved @ ")); DEBUG_PRINTLN(time_to_localtime_str(unixTime_buf));
      if (rtcmem_token.state & STATE_TASK_MQTT_RTCMEM_RESTORE_SPECIAL) {
        DEBUG_PRINTLN(F("Restore of RTCMEM forced by sensor change during WifiManager"));
      } else if (fw_version_buf>=fw_version) {
        DEBUG_PRINT(F("Received FW version ")); DEBUG_PRINT(fw_version_ul2char(fw_version_buf,8)); DEBUG_PRINT(F(" from MQTT is newer or same as actual FW version ")); DEBUG_PRINTLN(fw_version_ul2char(fw_version,8));
        rtcmem_match = false;
      } else {
        uint32_t unixTime = sntp_get_unixTime();
        DEBUG_PRINT(F("FW update detected, checking for age against current date ")); DEBUG_PRINTLN(time_to_localtime_str(unixTime));
        if ((unixTime-unixTime_buf<(3600U*24U))||(unixTime_buf==UMAXVALTYPE(unixTime_buf))||(unixTime==UMAXVALTYPE(unixTime))) {
          DEBUG_PRINTLN(F("RTCMEM from MQTT found that was saved within last 24h"));
        } else {
          DEBUG_PRINTLN(F("RTCMEM from MQTT found but is too old or has unreasonable date"));
          rtcmem_match = false;
        }
      }
      if (RTCMEM_TOKEN_FINGERPRINT != rtcmem_token_buf.fingerprint) {
        DEBUG_PRINTLN(F("Unexpected received rtcmem_token, fingerprint missmatch"));
        rtcmem_match = false;
      }
      DEBUG_BY_LEVEL_PRINT(2, F("CRC32 read from MQTT ")); DEBUG_BY_LEVEL_PRINTLN(2,rtcmem_state_buf.crc32, HEX);
      uint32_t crc32_calc = crc32((byte*) &rtcmem_state_buf.data, sizeof(RTCMEM_STATE_DATA_S));
      DEBUG_BY_LEVEL_PRINT(2, F("CRC32 calculated to ")); DEBUG_BY_LEVEL_PRINTLN(2,crc32_calc, HEX);
      if (crc32_calc != rtcmem_state_buf.crc32) {
        DEBUG_PRINTLN(F("Unexpected received rtcmem_state, CRC missmatch"));
        rtcmem_match = false;
      }
      if (rtcmem_match) {
        DEBUG_PRINTLN(F("Restoring RTC memory from MQTT"));
        buzzer_music(tone_restore_rtcmem);
        rtcmem_token = rtcmem_token_buf;
        rtcmem_state = rtcmem_state_buf;
        rtcmem_token.state &= ~STATE_TASK_MQTT_RTCMEM_RESTORE;
        rtcmem_token.state |= STATE_TASK_SEND_MAIL_STATUS_UPDATE;
        rtcmem_token.state |= STATE_TASK_SENSOR_UPDATE;
        rtcmem_state.data.rtcmemSaveToMqttDwnCntHours = 0;
        rtcmem_state.data.httpOTAUpdateDwnCntHours = 0;
        TASK_STAT_UPDATE(rtcmem_state.data.EspWWifiStat, true, millis()+MILLIS_OFFSET_WRF);
        rtcmem_write_state();
        rtcmem_write_token();
        DEBUG_PRINTLN(F("Restart..."));
        delay(1000); // finish music
        ESP.reset();
        delay(1000);
      } else {
        DEBUG_PRINTLN(F("Conditions for restoring RTC memory from MQTT not met"));
      }
    } else {
      DEBUG_PRINT(F("RTCMEM message length does not fit expectation of ")); DEBUG_PRINT(MQTT_RTCMEM_STORE_SIZE); DEBUG_PRINTLN(F("bytes"));
    }
    rtcmem_token.state &= ~STATE_TASK_MQTT_RTCMEM_RESTORE;
  }
}

bool mqttSaveRestoreRtcMem(uint8_t save0_or_restore1) {
    bool success = false;
    if (eeprommem_config.data.mqttHostname[0]!='\0') {
      uint32_t unixTime = sntp_get_unixTime();
      if ((unixTime != UMAXVALTYPE(unixTime))||(rtcmem_state.data.ntpStat.errorsSinceSuccess>5)) {
        WiFiClient wclient;
        PubSubClient mqttClient(wclient);
        TIC;
        // MQTT_ENABLED
        mqttClient.setServer(eeprommem_config.data.mqttHostname, eeprommem_config.data.mqttPort);
        if (mqttClient.connect(eeprommem_config.data.hostname)) {
          String mqttRtcMemTopic = String(F("rtcmem/")) + String(eeprommem_config.data.hostname);
          if (save0_or_restore1==0) {
            // save
            uint8_t payload_buf[MQTT_RTCMEM_STORE_SIZE];
            uint8_t *payload_buf_ptr = payload_buf;
            uint16_t rtcmemSaveToMqttDwnCntHours_bak = rtcmem_state.data.rtcmemSaveToMqttDwnCntHours;
            rtcmem_state.data.rtcmemSaveToMqttDwnCntHours = RTCMEM_SAVE_TO_MQTT_HOURS; // set before publish and reset if publish is not successfull
            rtcmem_state.crc32 = crc32((byte*) &rtcmem_state.data, sizeof(RTCMEM_STATE_DATA_S));
            DEBUG_PRINT(F("Save RTCMEM of FW version ")); DEBUG_PRINT(fw_version_ul2char(fw_version,8));
            DEBUG_PRINT(F(" @ ")); DEBUG_PRINT(time_to_localtime_str(unixTime));
            DEBUG_PRINT(F(" having ")); DEBUG_PRINT(sizeof(payload_buf));
            DEBUG_PRINT(F("bytes to MQTT published as ")); DEBUG_PRINTLN(mqttRtcMemTopic);
            os_memcpy(payload_buf_ptr, &fw_version,   sizeof(fw_version));   payload_buf_ptr += sizeof(fw_version);
            os_memcpy(payload_buf_ptr, &unixTime,    sizeof(unixTime));    payload_buf_ptr += sizeof(unixTime);
            os_memcpy(payload_buf_ptr, &rtcmem_token, sizeof(rtcmem_token)); payload_buf_ptr += sizeof(rtcmem_token);
            os_memcpy(payload_buf_ptr, &rtcmem_state, sizeof(rtcmem_state)); payload_buf_ptr += sizeof(rtcmem_state);
            success = mqttClient.publish(mqttRtcMemTopic.c_str(), payload_buf, sizeof(payload_buf), true);
            if (!(success)) rtcmem_state.data.rtcmemSaveToMqttDwnCntHours = rtcmemSaveToMqttDwnCntHours_bak;
          } else {
            // restore
            DEBUG_PRINTLN(F("Looking for RTCMEM from MQTT"));
            mqttClient.setCallback(mqttCallback);
            success = mqttClient.subscribe(mqttRtcMemTopic.c_str()); mqttClient.loop();
            if (success) {
              DEBUG_PRINT(F("Wait 10s for MQTT retained message ")); DEBUG_PRINTLN(mqttRtcMemTopic);
              // if MQTT message is received module will restart and we never succeed here
              uint32_t time1 = millis();
              while ((mqttClient.connected())&&(rtcmem_token.state & STATE_TASK_MQTT_RTCMEM_RESTORE)) {
                mqttClient.loop();
                delay(10);
                DEBUG_PRINT(F("."));
                if (((millis() - time1)) > 10*1000U) { // wifi connection lasts too ling, retry
                  DEBUG_PRINT(F("timeout"))
                  break;
                }
              }
              DEBUG_PRINTLN(F("\nNo matching MQTT retained message received"));
              rtcmem_token.state &= ~STATE_TASK_MQTT_RTCMEM_RESTORE;
            }
          }
        } else {
            DEBUG_PRINTLN(F("MQTT connection error"));
        }
        TASK_STAT_UPDATE(rtcmem_state.data.mqttStat, success, TOC);
        if (mqttClient.connected()) mqttClient.disconnect();
        if (wclient.connected()) wclient.stopAll();
      } else {
        DEBUG_PRINTLN(F("SNTP error preventing RTCMEM save/restore"));
      }
    } else {
      DEBUG_PRINTLN(F("No MQTT server setup"));
    }
    return success;
}

void mqttUpdate(PubSubClient &mqttClient, uint16_t vcc) {
  TIC;
  bool success;
  if (eeprommem_config.data.mqttHostname[0]!='\0') {
    // MQTT_ENABLED
    mqttClient.setServer(eeprommem_config.data.mqttHostname, eeprommem_config.data.mqttPort);
    if (mqttClient.connect(eeprommem_config.data.hostname)) {
      DEBUG_PRINTLN(F("MQTT connected"));
      success = true;
      bool this_success;
      if (rtcmem_token.state & STATE_TASK_SENSOR_ALERT_OPEN) {
        DEBUG_PRINT(F("MQTT publish ")); DEBUG_PRINT(eeprommem_config.data.mqttTopic); DEBUG_PRINTLN(F(" open"));
        this_success = mqttClient.publish(eeprommem_config.data.mqttTopic, "open", true);
      } else {
        DEBUG_PRINT(F("MQTT publish ")); DEBUG_PRINT(eeprommem_config.data.mqttTopic); DEBUG_PRINTLN(F(" closed"));
        this_success = mqttClient.publish(eeprommem_config.data.mqttTopic, "closed", true);
      }
      success &= this_success;
      if (this_success) {DEBUG_BY_LEVEL_PRINTLN(4,F("MQTT publish success"));} else {DEBUG_PRINTLN(F("MQTT publish error"));}
      if (vcc < VCC_ERROR_MIN_V)  {
        DEBUG_PRINT(F("MQTT publish ")); DEBUG_PRINT(eeprommem_config.data.mqttBatteryTopic); DEBUG_PRINTLN(F(" nok"));
        this_success = mqttClient.publish(eeprommem_config.data.mqttBatteryTopic, "nok", false);
      } else {
        DEBUG_PRINT(F("MQTT publish ")); DEBUG_PRINT(eeprommem_config.data.mqttBatteryTopic); DEBUG_PRINTLN(F(" ok"));
        this_success = mqttClient.publish(eeprommem_config.data.mqttBatteryTopic, "ok", false);
      }
      success &= this_success;
      if (this_success) {DEBUG_BY_LEVEL_PRINTLN(4,F("MQTT publish success"));} else {DEBUG_PRINTLN(F("MQTT publish error"));}
    } else {
      DEBUG_PRINTLN(F("MQTT connection error"));
    }
  } else {
    DEBUG_PRINTLN(F("No MQTT server setup"));
    success = false;
    rtcmem_token.gpio_last = rtcmem_token.gpio_next;
    rtcmem_token.state &= ~STATE_TASK_SENSOR_ALERT;
  }
  if (success) {
    blinker_start(2,50,1,0);
    rtcmem_token.gpio_last = rtcmem_token.gpio_next;
    rtcmem_token.state &= ~STATE_TASK_SENSOR_ALERT;
  } else {
    DEBUG_PRINTLN(F("MQTT error"));
    blinker_start(10,50,1,0);
  }
  TASK_STAT_UPDATE(rtcmem_state.data.mqttStat, success, TOC);
}

String MailBody_add_to_string(String &body, uint16_t vcc) {
  DEBUG_BY_LEVEL_PRINTLN(4, F("Generating Mail body"));
  uint32_t uptime;
  body += String(F("Status report #"));
  body += (rtcmem_state.data.mailStatusCnt==UMAXVALTYPE(rtcmem_state.data.mailStatusCnt))?F("MAX"):String(rtcmem_state.data.mailStatusCnt);
  body += String(F(" of ")) + String(eeprommem_config.data.hostname);
  body += String(F(" @ FW version ")) + fw_version_ul2char(fw_version,8) + String(F("\n\n"));
   if ((rtcmem_token.state & STATE_ERROR_ALL) || (rtcmem_token.state & STATE_WARNING_ALL)) {
    DEBUG_BY_LEVEL_PRINTLN(4, F("Generating Mail body error message"));
    body += String(eeprommem_config.data.hostname) + String(F(" has error(s)/warning(s)"));
    if (rtcmem_token.state & STATE_ERROR_VCC) body += F(", Battery low");
    if (rtcmem_token.state & STATE_ERROR_ESPWWIFI) body += F(", Failures preventing long sleep w/o WiFi");
    if (rtcmem_token.state & STATE_ERROR_WIFI) body += F(", WiFi failures");
    if (rtcmem_token.state & STATE_ERROR_MQTT) body += F(", Mqtt failures");
    if (rtcmem_token.state & STATE_ERROR_HTTPUPDATE) body += F(", Http-update failures");
    if (rtcmem_token.state & STATE_ERROR_HTTPUPDATE_VCC) body += F(", Http-update VCC too low");
    if (rtcmem_token.state & STATE_ERROR_MAIL) body += F(", Mail failures");
    if (rtcmem_token.state & STATE_ERROR_NTP) body += F(", NTP failures");
    body += String(F("\n\n"));
  }
  body += String(F("Uptime: "));
  if ((rtcmem_state.data.BootTime) && (time(NULL)>=946684800U)) {
    uptime = time(NULL)-rtcmem_state.data.BootTime;
    body += timeDiffToString(uptime);
    body += F("\n");
  } else {
    uptime = 0;
    body += F("NaN\n");
  }
  body += F("Battery voltage: ");
  body += String((float)vcc/VCC_READ_1V);
  body += F("V\n");
  {
    float mAh_sleep = (float)uptime * CURRENT_SECONDS_TO_MAH_SLEEP;
    float mAh_wo_RF = (float)rtcmem_token.EspWoWifiStat_millis * CURRENT_MILLIS_TO_MAH_WORF;
    float mAh_w_RF = (float)rtcmem_state.data.EspWWifiStat.millis * CURRENT_MILLIS_TO_MAH_WRF;
    float mAh = mAh_sleep + mAh_wo_RF + mAh_w_RF;
    body += F("Estimated battery usage: ");
    body += String(mAh);
    body += F("mAh\n");
    body += F("-in sleep: ");
    if (rtcmem_state.data.BootTime) {
      body += String(mAh_sleep);
      body += F("mAh (");
      body += (mAh)?String(mAh_sleep/mAh*100.0):F("NaN");
      body += F("%)\n");
    } else
      body += F("NaN\n");
    body += F("-in no WiFi: ");
    body += String(mAh_wo_RF);
    body += F("mAh (");
    body += (mAh)?String(mAh_wo_RF/mAh*100.0):F("NaN");
    body += F("%)\n");
    body += F("-in active WiFi: ");
    body += String(mAh_w_RF);
    body += F("mAh (");
    body += (mAh)?String(mAh_w_RF/mAh*100.0):F("NaN");
    body += F("%)\n");
    body += F("Estimated remaining capacity: ");
    float mAh_rem = (float)BATTERY_MAH-mAh;
    if (mAh_rem<0) mAh_rem = 0;
    body += String(mAh_rem);
    body += F("mAh (");
    body += String(mAh_rem/((float)BATTERY_MAH)*100.0);
    body += F("%)\n");
    body += F("Estimated remaining battery lifetime: ");
    uint32_t remaining_time;
    if ((uptime)&&(mAh)) {
      remaining_time = (uint32_t)((float)uptime / mAh * mAh_rem);
      body += timeDiffToString(remaining_time) + F("\n");
    } else {
      body += F("NaN\n");
    }
    body += F("Estimated end of battery lifetime: ");
    if ((uptime)&&(mAh))
      body += time_to_localtime_str(rtcmem_state.data.BootTime + remaining_time) + F("\n");
    else
      body += F("NaN\n");
  }
  DEBUG_BY_LEVEL_PRINTLN(4, body);
  DEBUG_BY_LEVEL_PRINT(3, F("body.length: ")); DEBUG_BY_LEVEL_PRINTLN(3, body.length());
  INCSAT(rtcmem_state.data.mailStatusCnt);
  return body;
}

String MailSubject() {
  String subject;
  //subject.reserve(80);
  if (rtcmem_token.state & STATE_ERROR_ALL) {
    DEBUG_BY_LEVEL_PRINTLN(4, F("Generating Mail Subject for error case"));
    subject = String(eeprommem_config.data.hostname) + String(F(" has error(s)"));
  } else if (rtcmem_token.state & STATE_WARNING_ALL) {
    DEBUG_BY_LEVEL_PRINTLN(4, F("Generating Mail Subject for warning case"));
    subject = String(eeprommem_config.data.hostname) + String(F(" has warning(s)"));
  } else {
    DEBUG_BY_LEVEL_PRINTLN(4, F("Generating Mail Subject for healthy case"));
    subject = String(eeprommem_config.data.hostname) + String(F(" is healthy"));
  }
  DEBUG_BY_LEVEL_PRINTLN(4, subject);
  DEBUG_BY_LEVEL_PRINT(3, F("subject.length: ")); DEBUG_BY_LEVEL_PRINTLN(3, subject.length());
  return subject;
}

uint32_t sntp_get_unixTime() {
  uint32_t unixTime = UMAXVALTYPE(unixTime);
  if (eeprommem_config.data.ntpServer[0]!='\0') {
    SNTPtime SNTPserver(eeprommem_config.data.ntpServer);
    if (time(NULL)<946684800U) {
      TIC;
      bool sntpsuccess = SNTPserver.setSNTPtime();
      if (sntpsuccess) {
         unixTime = time(NULL);
         if (rtcmem_state.data.BootTime==0) rtcmem_state.data.BootTime = unixTime;
      }
      TASK_STAT_UPDATE(rtcmem_state.data.ntpStat, sntpsuccess, TOC);
    } else {
      unixTime = time(NULL);
    }
  } else {
    DEBUG_PRINTLN(F("No NTP server setup"));
    unixTime = 0;
  }
  return unixTime;
}

int mailUpdate(uint8_t mail_type_status0_sensor1, uint16_t vcc, uint16_t gpio_now, String *body_prefix=NULL) {
  // return 0:  mail send successsfully
  // return -1: no smtp setup
  // return 1:  mail error

  bool mailsuccess = false;
  uint32_t unixTime = UMAXVALTYPE(unixTime);

  if (mail_type_status0_sensor1 == 0) unixTime = sntp_get_unixTime();
  DEBUG_BY_LEVEL_PRINT(3,F("ESP.getFreeHeap(@line ")); DEBUG_BY_LEVEL_PRINT(3,__LINE__); DEBUG_BY_LEVEL_PRINT(3,F(") = ")); DEBUG_BY_LEVEL_PRINTLN(3, ESP.getFreeHeap());

  if ((eeprommem_config.data.smtpServer[0]!='\0') && (eeprommem_config.data.smtpUser[0]!='\0') && (eeprommem_config.data.smtpPass[0]!='\0') && (eeprommem_config.data.smtpTo[0]!='\0')) {
    if ((unixTime!=UMAXVALTYPE(unixTime)) || (rtcmem_state.data.ntpStat.errorsSinceSuccess>=MAIL_FORCE_SEND_AFTER_NTP_ERRORS) || (mail_type_status0_sensor1==1)) {
      TIC;

      String subject, body;
      switch (mail_type_status0_sensor1) {
        case 0: // status mail
          subject = MailSubject();
          body.reserve(2000);
          MailBody_add_to_string(body, vcc);
          body += String(F("\n"));
          rtcmem_token_add_to_string(body);
          body += String(F("\n"));
          rtcmem_state_add_to_string(body);
          break;
        case 1: // sensor mail
        default:
          subject = String(eeprommem_config.data.hostname) + String(F(" is "));
          subject += (gpio_now & (1<<PIN_SENSOR_STATE)) ? String(F("closed")) : String(F("open"));
          subject += String(F(", battery is "));
          subject += (vcc < VCC_ERROR_MIN_V)?String(F("empty")):String(F("ok"));
          break;
      }
      if (body_prefix) body+=(*body_prefix);
      DEBUG_BY_LEVEL_PRINT(3, F("subject.length: ")); DEBUG_BY_LEVEL_PRINTLN(3, subject.length());
      DEBUG_BY_LEVEL_PRINT(3, F("body.length: ")); DEBUG_BY_LEVEL_PRINTLN(3, body.length());

      DEBUG_BY_LEVEL_PRINT(3,F("ESP.getFreeHeap(@line ")); DEBUG_BY_LEVEL_PRINT(3,__LINE__); DEBUG_BY_LEVEL_PRINT(3,F(") = ")); DEBUG_BY_LEVEL_PRINTLN(3, ESP.getFreeHeap());
      SMTPClient smtp;
      smtp.setTimeout(2000);
      smtp.begin(eeprommem_config.data.smtpServer, eeprommem_config.data.smtpPort); // Server identity is not verified
      smtp.setAuthorization(eeprommem_config.data.smtpUser, eeprommem_config.data.smtpPass);
      smtp.addHeader(String(F("Content-Type")), String(F("text/plain; charset=UTF-8")));
      smtp.addHeader(String(F("Content-Transfer-Encoding")), String(F("8bit")));
      String mailer_str = String(F("DSS-Mailer"));
      smtp.setMailer(mailer_str.c_str());
      DEBUG_BY_LEVEL_PRINT(3,F("ESP.getFreeHeap(@line ")); DEBUG_BY_LEVEL_PRINT(3,__LINE__); DEBUG_BY_LEVEL_PRINT(3,F(") = ")); DEBUG_BY_LEVEL_PRINTLN(3, ESP.getFreeHeap());

      int result = 1;
      result = smtp.sendMessage(eeprommem_config.data.smtpUser, body.c_str(), body.length(), eeprommem_config.data.smtpTo, subject.c_str());
      if (result>0) {
        mailsuccess = true;
        DEBUG_PRINTLN(F("Mail sent sucessfully!"));
      } else {
        DEBUG_PRINTLN(F("Mail error!"));
        DEBUG_BY_LEVEL_PRINT(3,F("[SMTP] Message send result: ")); DEBUG_BY_LEVEL_PRINT(3,result); DEBUG_BY_LEVEL_PRINT(3,F(", errorToString(result): ")); DEBUG_BY_LEVEL_PRINTLN(3,smtp.errorToString(result)); DEBUG_BY_LEVEL_PRINT(3,F(", getErrorMessage: ")); DEBUG_BY_LEVEL_PRINT(3,smtp.getErrorMessage());
      }
      smtp.end();
      smtp.disconnect();
      DEBUG_BY_LEVEL_PRINT(3,F("ESP.getFreeHeap(@line ")); DEBUG_BY_LEVEL_PRINT(3,__LINE__); DEBUG_BY_LEVEL_PRINT(3,F(") = ")); DEBUG_BY_LEVEL_PRINTLN(3, ESP.getFreeHeap());

      TASK_STAT_UPDATE(rtcmem_state.data.mailStat, mailsuccess, TOC);
    }
    return (mailsuccess)?0:1;
  } else {
    DEBUG_PRINTLN(F("No SMTP server setup"));
    return -1;
  }
}

void setup() {
  // get GPIO state
  uint32 gpio_now = READ_PERI_REG(0x60000318);

  // avoid reset until NEXT DEEP SLEEP
  pinMode(PIN_SENSOR_RESET_DISABLE, OUTPUT); digitalWrite(PIN_SENSOR_RESET_DISABLE, LOW);                                                                                                                                                // measured that this line is executed approx. 200ms after reset
  pinMode(16, OUTPUT); digitalWrite(16, HIGH); // GPIO is RTC wake up but not tied high by default in non deep sleep operation
  WiFi.persistent( false );
  WiFi.mode(WIFI_OFF);

  // start DEBUG
  DEBUG_BY_LEVEL(0, Serial.begin(115200));
  DEBUG_BY_LEVEL(5, Serial.setDebugOutput(true));
  DEBUG_PRINT(F("\n\nStarted from reset, GPIO state: 0b")); DEBUG_PRINTLN(u20binstr(gpio_now));
  DEBUG_BY_LEVEL_PRINT(3,F("ESP.getFreeHeap(@line ")); DEBUG_BY_LEVEL_PRINT(3,__LINE__); DEBUG_BY_LEVEL_PRINT(3,F(") = ")); DEBUG_BY_LEVEL_PRINTLN(3, ESP.getFreeHeap());
  TIC;

  rtcmem_read_token();

  // look for reset reason
  {
    rst_info *rsti = ESP.getResetInfoPtr();
    bool had_exception = false;
    DEBUG_PRINT(F("ResetInfo.reason = ")); DEBUG_PRINTLN(rsti->reason);
    switch (rsti->reason) {
      case REASON_DEFAULT_RST: break;      // REASON_DEFAULT_RST		    = 0 /* normal startup by power on */
      case REASON_SOFT_RESTART: break;     // REASON_SOFT_RESTART 	    = 4 /* software restart ,system_restart , GPIO status won’t change */
      case REASON_DEEP_SLEEP_AWAKE: break; // REASON_DEEP_SLEEP_AWAKE	  = 5 /* wake up from deep-sleep */
      case REASON_EXT_SYS_RST: break;      // REASON_EXT_SYS_RST        = 6 /* external system reset */
      case REASON_EXCEPTION_RST:           // REASON_EXCEPTION_RST	    = 2 /* exception reset, GPIO status won’t change */
        INCSAT(rtcmem_token.EspResetExcept_cnt);
        had_exception = true;
        break;
      case REASON_WDT_RST:                 // REASON_WDT_RST			      = 1 /* hardware watch dog reset */
      case REASON_SOFT_WDT_RST:            // REASON_SOFT_WDT_RST       = 3 /* software watch dog reset, GPIO status won’t change */
        INCSAT(rtcmem_token.EspResetWdt_cnt);
        had_exception = true;
        break;
      default:
        DEBUG_PRINTLN(ESP.getResetInfo());
        break;
    }
    if (had_exception) {
      if (rtcmem_token.state & STATE_TASK_CONFIG_PORTAL) rtcmem_token.state |= STATE_TASK_MQTT_RTCMEM_RESTORE_SPECIAL;
      rtcmem_write_token();
    }
  }

  if (rtcmem_token.state & STATE_LONG_SLEEP) {
    INCSAT(rtcmem_token.sleep_cnt_wo_wifi);
    if (rtcmem_token.sleep_cnt_wo_wifi>=MQTT_SENSOR_UPDATE_HOURS) {
      rtcmem_token.state |= STATE_TASK_SENSOR_UPDATE;
    }
  }

  rtcmem_token.state |= STATE_TASKS_ALWAYS_ONCE;
  bool rfon = (rtcmem_token.state & STATE_RFON);

  DEBUG_BY_LEVEL_PRINT(3,F("ESP.getFreeHeap(@line ")); DEBUG_BY_LEVEL_PRINT(3,__LINE__); DEBUG_BY_LEVEL_PRINT(3,F(") = ")); DEBUG_BY_LEVEL_PRINTLN(3, ESP.getFreeHeap());
  uint16_t vcc=0;
  if (rfon) {
    handle_rfon();
    rtcmem_token.state |= STATE_TASK_SENSOR_UPDATE; // if RF is on send status update
    // get battery voltage
    vcc = ESP.getVcc();
    DEBUG_PRINT(F("VCC = ")); DEBUG_PRINT((float)vcc/VCC_READ_1V); DEBUG_PRINTLN(F("V"));
  }

  String no_mqtt_sensor_history_mail;
  DEBUG_BY_LEVEL_PRINT(3,F("ESP.getFreeHeap(@line ")); DEBUG_BY_LEVEL_PRINT(3,__LINE__); DEBUG_BY_LEVEL_PRINT(3,F(") = ")); DEBUG_BY_LEVEL_PRINTLN(3, ESP.getFreeHeap());
  {
    uint32_t task_watch = 0;
    uint8_t task_loop_wd = (rfon)?10:1;
    WiFiClient wclient;
    PubSubClient mqttClient(wclient);

    do { // loop until task vector is zero or stable
      TIC;
      DEBUG_PRINT(F("Task loop (task_loop_wd=")); DEBUG_PRINT(task_loop_wd); DEBUG_PRINT(F("): rtcmem_token.state = 0b")); DEBUG_PRINT(u20binstr(rtcmem_token.state)); DEBUG_PRINT(F(", task_watch = 0b")); DEBUG_PRINTLN(u20binstr(task_watch));
      task_watch = rtcmem_token.state;

      if (rfon) { // handle tasks that need RFON
        // if WiFi is not connected yet...
        DEBUG_BY_LEVEL_PRINT(3,F("ESP.getFreeHeap(@line ")); DEBUG_BY_LEVEL_PRINT(3,__LINE__); DEBUG_BY_LEVEL_PRINT(3,F(") = ")); DEBUG_BY_LEVEL_PRINTLN(3, ESP.getFreeHeap());
        if (WiFi.status() != WL_CONNECTED) connectWiFi();

        if (WiFi.status() == WL_CONNECTED) {
          // WiFi is connected now
          delay(0);
          //MQTT update
          DEBUG_BY_LEVEL_PRINT(3,F("ESP.getFreeHeap(@line ")); DEBUG_BY_LEVEL_PRINT(3,__LINE__); DEBUG_BY_LEVEL_PRINT(3,F(") = ")); DEBUG_BY_LEVEL_PRINTLN(3, ESP.getFreeHeap());
          if (rtcmem_token.state & STATE_TASK_SENSOR_ALERT) {
            rtcmem_token.state &= ~STATE_TASK_SENSOR_UPDATE;
            if (~(rtcmem_token.state & STATE_TASK_SENSOR_ALERT_NO_CHANGE)) INCSAT(rtcmem_state.data.sensorChangeCnt);
            if (eeprommem_config.data.mqttHostname[0]!='\0') {
              mqttUpdate(mqttClient, vcc);
            } else {
              rtcmem_token.gpio_last = rtcmem_token.gpio_next;
              if (no_mqtt_sensor_history_mail.length()) no_mqtt_sensor_history_mail += F(", ");
              if (rtcmem_token.state & STATE_TASK_SENSOR_ALERT_NO_CHANGE)
                no_mqtt_sensor_history_mail += F("update");
              else
                no_mqtt_sensor_history_mail += (rtcmem_token.state & STATE_TASK_SENSOR_ALERT_OPEN)?F("open"):F("closed");
              rtcmem_token.state &= ~STATE_TASK_SENSOR_ALERT;
              rtcmem_token.state |= STATE_TASK_SEND_MAIL_SENSOR_UPDATE;
            }
          }
        }
      }

      // check sensor status if not yet pending
      DEBUG_BY_LEVEL_PRINT(3,F("ESP.getFreeHeap(@line ")); DEBUG_BY_LEVEL_PRINT(3,__LINE__); DEBUG_BY_LEVEL_PRINT(3,F(") = ")); DEBUG_BY_LEVEL_PRINTLN(3, ESP.getFreeHeap());
      DEBUG_PRINTLN(F("GPIO check"));
      if (((rtcmem_token.gpio_last ^ (uint16_t) gpio_now) & (1<<PIN_SENSOR_STATE))||(rtcmem_token.state & STATE_TASK_SENSOR_UPDATE)) {
        rtcmem_token.state &= ~STATE_TASK_SENSOR_UPDATE;
        if (rtcmem_token.state & STATE_TASK_SENSOR_ALERT) {
          DEBUG_PRINTLN(F("Sensor state change/update detected but pending"));
        } else {
          if ((rtcmem_token.gpio_last ^ (uint16_t) gpio_now) & (1<<PIN_SENSOR_STATE)) {
            DEBUG_PRINTLN(F("Sensor state change detected"));
            rtcmem_token.state &= ~STATE_TASK_SENSOR_ALERT_NO_CHANGE;
          } else {
            DEBUG_PRINTLN(F("Sensor state update requested"));
            rtcmem_token.state |= STATE_TASK_SENSOR_ALERT_NO_CHANGE;
          }
          rtcmem_token.gpio_next = (uint16_t) gpio_now;
          rtcmem_token.state |= (rtcmem_token.gpio_next & (1<<PIN_SENSOR_STATE)) ? STATE_TASK_SENSOR_ALERT_CLOSED : STATE_TASK_SENSOR_ALERT_OPEN;
        }
      }
      gpio_now = READ_PERI_REG(0x60000318);
      task_loop_wd--;
      DEBUG_PRINTLN(F("Task loop iteration done"));
    } while ((task_loop_wd) && (rtcmem_token.state & (~task_watch) & STATE_TASKS));

    DEBUG_BY_LEVEL_PRINT(3,F("ESP.getFreeHeap(@line ")); DEBUG_BY_LEVEL_PRINT(3,__LINE__); DEBUG_BY_LEVEL_PRINT(3,F(") = ")); DEBUG_BY_LEVEL_PRINTLN(3, ESP.getFreeHeap());
    DEBUG_PRINT(F("Task loop finished (task_loop_wd=")); DEBUG_PRINT(task_loop_wd); DEBUG_PRINT(F("): rtcmem_token.state = 0b")); DEBUG_PRINTLN(u20binstr(rtcmem_token.state));
    if (mqttClient.connected()) mqttClient.disconnect();
    if (wclient.connected()) wclient.stopAll();
  }
  // single time tasks having WiFi connected
  if (WiFi.status() == WL_CONNECTED) {
    // restore RTC from MQTT
    DEBUG_BY_LEVEL_PRINT(3,F("ESP.getFreeHeap(@line ")); DEBUG_BY_LEVEL_PRINT(3,__LINE__); DEBUG_BY_LEVEL_PRINT(3,F(") = ")); DEBUG_BY_LEVEL_PRINTLN(3, ESP.getFreeHeap());
    if (rtcmem_token.state & STATE_TASK_MQTT_RTCMEM_RESTORE) mqttSaveRestoreRtcMem(1);

    // OTA update
    DEBUG_BY_LEVEL_PRINT(3,F("ESP.getFreeHeap(@line ")); DEBUG_BY_LEVEL_PRINT(3,__LINE__); DEBUG_BY_LEVEL_PRINT(3,F(") = ")); DEBUG_BY_LEVEL_PRINTLN(3, ESP.getFreeHeap());
    if (rtcmem_state.data.httpOTAUpdateDwnCntHours==0) httpUdateServerCheck(vcc);

    // save RTC to MQTT
    DEBUG_BY_LEVEL_PRINT(3,F("ESP.getFreeHeap(@line ")); DEBUG_BY_LEVEL_PRINT(3,__LINE__); DEBUG_BY_LEVEL_PRINT(3,F(") = ")); DEBUG_BY_LEVEL_PRINTLN(3, ESP.getFreeHeap());
    if ((!(rtcmem_token.state & STATE_TASK_MQTT_RTCMEM_RESTORE)) && (rtcmem_state.data.rtcmemSaveToMqttDwnCntHours==0)) mqttSaveRestoreRtcMem(0);

    // Mail sensor update
    DEBUG_BY_LEVEL_PRINT(3,F("ESP.getFreeHeap(@line ")); DEBUG_BY_LEVEL_PRINT(3,__LINE__); DEBUG_BY_LEVEL_PRINT(3,F(") = ")); DEBUG_BY_LEVEL_PRINTLN(3, ESP.getFreeHeap());
    if (rtcmem_token.state & STATE_TASK_SEND_MAIL_SENSOR_UPDATE) {
      int mailresult = mailUpdate(1, vcc, (uint16_t) gpio_now, &no_mqtt_sensor_history_mail);
      if (mailresult == 0) {
        rtcmem_token.state &= ~STATE_TASK_SEND_MAIL_SENSOR_UPDATE;
        blinker_start(2,50,1,0);
      } else {
        if (mailresult==-1) TASK_STAT_UPDATE(rtcmem_state.data.mailStat, false, 0);
        blinker_start(10,50,1,0);
        delay(1000);
      }
    }

    // Mail status update
    DEBUG_BY_LEVEL_PRINT(3,F("ESP.getFreeHeap(@line ")); DEBUG_BY_LEVEL_PRINT(3,__LINE__); DEBUG_BY_LEVEL_PRINT(3,F(") = ")); DEBUG_BY_LEVEL_PRINTLN(3, ESP.getFreeHeap());
    if ((rtcmem_token.state & STATE_TASK_SEND_MAIL_STATUS_UPDATE) && (mailUpdate(0, vcc, (uint16_t) gpio_now)!=1)) rtcmem_token.state &= ~STATE_TASK_SEND_MAIL_STATUS_UPDATE;

    // all WiFi done
    WiFi.disconnect( true );
  }

  if (rfon) {
    // check vcc and error counters for buzzer or mail attention
    DEBUG_BY_LEVEL_PRINT(3,F("ESP.getFreeHeap(@line ")); DEBUG_BY_LEVEL_PRINT(3,__LINE__); DEBUG_BY_LEVEL_PRINT(3,F(") = ")); DEBUG_BY_LEVEL_PRINTLN(3, ESP.getFreeHeap());
    checkForErrors(vcc);
  }

  TIC;
  DEBUG_BY_LEVEL_PRINT(3,F("ESP.getFreeHeap(@line ")); DEBUG_BY_LEVEL_PRINT(3,__LINE__); DEBUG_BY_LEVEL_PRINT(3,F(") = ")); DEBUG_BY_LEVEL_PRINTLN(3, ESP.getFreeHeap());

  uint32_t next_sleeptime;
  if (rfon || ((rtcmem_token.state & STATE_TASKS_ACQUIRE_RF)==0)) {
    next_sleeptime = DEEP_SLEEP_LONG_US_FF;
    rtcmem_token.state |= STATE_LONG_SLEEP;
  } else {
    next_sleeptime = DEEP_SLEEP_SHORT_US;
    rtcmem_token.state &= ~STATE_LONG_SLEEP;
  }
  DEBUG_PRINT(F("Next deep sleep time = ")); DEBUG_PRINT(next_sleeptime);

  RFMode next_rfon;
  if (rtcmem_token.state & STATE_TASKS_ACQUIRE_RF) {
    next_rfon = WAKE_RF_DEFAULT; //WAKE_RF_DEFAULT; // WAKE_RFCAL;
    rtcmem_token.state |= STATE_RFON;
  } else {
    next_rfon = WAKE_RF_DISABLED;
    rtcmem_token.state &= ~STATE_RFON;
  }
  DEBUG_PRINT(F(", NextRFON state = ")); DEBUG_PRINTLN(next_rfon);

  if (rfon) {
    TASK_STAT_UPDATE(rtcmem_state.data.EspWWifiStat, (next_rfon == WAKE_RF_DISABLED), millis()+MILLIS_OFFSET_WRF);
    rtcmem_write_state();
  } else {
    INCSAT(rtcmem_token.EspWoWifiStat_cnt);
    UADDSAT(rtcmem_token.EspWoWifiStat_millis,millis()+MILLIS_OFFSET_WORF);
  }
  rtcmem_write_token();

  DEBUG_BY_LEVEL_PRINT(0,F("Going for deep sleep @")); DEBUG_BY_LEVEL_PRINT(0,millis()); DEBUG_BY_LEVEL_PRINT(0,F(" ms: rtcmem_token.state = 0b")); DEBUG_BY_LEVEL_PRINTLN(0,u20binstr(rtcmem_token.state));

  // clear serial FIFOs for immediate sleep
  DEBUG_BY_LEVEL(0, delay(10)); // write out some more of my own logging
  DEBUG_BY_LEVEL(0, SET_PERI_REG_MASK(UART_CONF0(0), UART_TXFIFO_RST)); //RESET FIFO
  DEBUG_BY_LEVEL(0, CLEAR_PERI_REG_MASK(UART_CONF0(0), UART_TXFIFO_RST)); //RESET FIFO

  ESP.deepSleep(next_sleeptime, next_rfon);
}

void loop() {}
