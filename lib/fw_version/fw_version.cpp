#include "fw_version.h"
#include "fw_version_def.h"
#include <stdio.h>
#include <string.h>
#include "debug_by_level.h"

const unsigned long fw_version = MY_CONCAT(FW_VERSION);

char* fw_version_ul2char(unsigned long fw_version, int len) {
  static char buf[14] = ""; // 10 Digits + 3 seperators + NULL
  char *char_str = &buf[14];
  int intert_each_3 = 3;

  snprintf(buf,14,"%10lu",fw_version);
  *char_str = '\0';
  for (int i=9; i>=0; i--) {
    if ((len-->0) | (buf[i]!=' ')) {
      *--char_str = (buf[i]!=' ') ? buf[i] : '0';
      if (--intert_each_3==0) {
        intert_each_3 = 3;
        *--char_str = ',';
      }
    } else
      break;
  }
  return char_str;
}

unsigned long fw_version_char2ul(const char* fw_version_str) {
  char buf[14];
  char *token;
  unsigned long fw_version = 0;

  strncpy(buf, fw_version_str, 14);
  token = strtok(buf, ",.");
  while(token != NULL)
  {
     fw_version = fw_version*1000U + (unsigned long)atoi(token);
     token = strtok(NULL, ",.");
  }
  return fw_version;
}
