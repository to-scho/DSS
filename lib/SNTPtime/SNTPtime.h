/*
SNTP library for ESP8266


This routine gets the unixtime from a NTP server and adjusts it to the time zone and the
Middle European summer time if requested

Copyright (c) 2016 Andreas Spiess

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

V1.0 2016-8-3

*/

#ifndef SNTPtime_h
#define SNTPtime_h

#include <Arduino.h>

struct strDateTime
{
  byte hour;
  byte minute;
  byte second;
  int year;
  byte month;
  byte day;
  byte dayofWeek;
  bool valid;
};

class SNTPtime {
  public:
    SNTPtime(char *SNTPServer);
    strDateTime getTime(float _timeZone, bool _DayLightSaving);
    unsigned long getLocalTime(float _timeZone, bool _DayLightSaving);
    bool setSNTPtime();
    unsigned long adjustTimeZone(unsigned long _timeStamp, float _timeZone, byte _DayLightSavingSaving);

  private:
    strDateTime ConvertUnixTimestamp( unsigned long _tempTimeStamp);
    bool summerTime(unsigned long _timeStamp );
    bool daylightSavingTime(unsigned long _timeStamp);
};
#endif
String time_to_localtime_str(time_t t);
String timeDiffToString(uint32_t td);
