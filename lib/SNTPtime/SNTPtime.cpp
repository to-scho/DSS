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

#include <Arduino.h>
#include "SNTPtime.h"
#include "debug_by_level.h"
#include <time.h>

#define LEAP_YEAR(Y) ( ((1970+Y)>0) && !((1970+Y)%4) && ( ((1970+Y)%100) || !((1970+Y)%400) ) )

const int NTP_PACKET_SIZE = 48;
byte _packetBuffer[ NTP_PACKET_SIZE];
static const uint8_t  _monthDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

char *_SNTPserver=(char *)"";

// NTPserver is the name of the NTPserver

SNTPtime::SNTPtime(char *SNTPserver) {
  _SNTPserver = SNTPserver;
}

// Converts a unix time stamp to a strDateTime structure
strDateTime SNTPtime::ConvertUnixTimestamp( unsigned long _tempTimeStamp) {
  strDateTime _tempDateTime;
  uint8_t _year, _month, _monthLength;
  uint32_t _time;
  unsigned long _days;

  _time = (uint32_t)_tempTimeStamp;
  _tempDateTime.second = _time % 60;
  _time /= 60; // now it is minutes
  _tempDateTime.minute = _time % 60;
  _time /= 60; // now it is hours
  _tempDateTime.hour = _time % 24;
  _time /= 24; // now it is _days
  _tempDateTime.dayofWeek = ((_time + 4) % 7) + 1;  // Sunday is day 1

  _year = 0;
  _days = 0;
  while ((unsigned)(_days += (LEAP_YEAR(_year) ? 366 : 365)) <= _time) {
    _year++;
  }
  _tempDateTime.year = _year; // year is offset from 1970

  _days -= LEAP_YEAR(_year) ? 366 : 365;
  _time  -= _days; // now it is days in this year, starting at 0

  _days = 0;
  _month = 0;
  _monthLength = 0;
  for (_month = 0; _month < 12; _month++) {
    if (_month == 1) { // february
      if (LEAP_YEAR(_year)) {
        _monthLength = 29;
      } else {
        _monthLength = 28;
      }
    } else {
      _monthLength = _monthDays[_month];
    }

    if (_time >= _monthLength) {
      _time -= _monthLength;
    } else {
      break;
    }
  }
  _tempDateTime.month = _month + 1;  // jan is month 1
  _tempDateTime.day = _time + 1;     // day of month
  _tempDateTime.year += 1970;

  return _tempDateTime;
}


//
// Summertime calculates the daylight saving time for middle Europe. Input: Unixtime in UTC
//
bool SNTPtime::summerTime(unsigned long _timeStamp ) {

  strDateTime  _tempDateTime;
  _tempDateTime = ConvertUnixTimestamp(_timeStamp);
  // printTime("Innerhalb ", _tempDateTime);

  if (_tempDateTime.month < 3 || _tempDateTime.month > 10) return false; // keine Sommerzeit in Jan, Feb, Nov, Dez
  if (_tempDateTime.month > 3 && _tempDateTime.month < 10) return true; // Sommerzeit in Apr, Mai, Jun, Jul, Aug, Sep
  if (_tempDateTime.month == 3 && (_tempDateTime.hour + 24 * _tempDateTime.day) >= (3 +  24 * (31 - (5 * _tempDateTime.year / 4 + 4) % 7)) || _tempDateTime.month == 10 && (_tempDateTime.hour + 24 * _tempDateTime.day) < (3 +  24 * (31 - (5 * _tempDateTime.year / 4 + 1) % 7)))
    return true;
  else
    return false;
}

bool SNTPtime::daylightSavingTime(unsigned long _timeStamp) {

  strDateTime  _tempDateTime;
  _tempDateTime = ConvertUnixTimestamp(_timeStamp);

  // here the US code
  // see http://stackoverflow.com/questions/5590429/calculating-daylight-saving-time-from-only-date
  // since 2007 DST begins on second Sunday of March and ends on first Sunday of November.
  // Time change occurs at 2AM locally
  if (_tempDateTime.month < 3 || _tempDateTime.month > 11) return false;  //January, february, and december are out.
  if (_tempDateTime.month > 3 && _tempDateTime.month < 11) return true;   //April to October are in
  int previousSunday = _tempDateTime.day - (_tempDateTime.dayofWeek - 1);  // dow Sunday input was 1,
  // need it to be Sunday = 0. If 1st of month = Sunday, previousSunday=1-0=1
  //int previousSunday = day - (dow-1);
  // -------------------- March ---------------------------------------
  //In march, we are DST if our previous Sunday was = to or after the 8th.
  if (_tempDateTime.month == 3 ) {  // in march, if previous Sunday is after the 8th, is DST
    // unless Sunday and hour < 2am
    if ( previousSunday >= 8 ) { // Sunday = 1
      // return true if day > 14 or (dow == 1 and hour >= 2)
      return ((_tempDateTime.day > 14) ||
      ((_tempDateTime.dayofWeek == 1 && _tempDateTime.hour >= 2) || _tempDateTime.dayofWeek > 1));
    } // end if ( previousSunday >= 8 && _dateTime.dayofWeek > 0 )
    else
    {
      // previousSunday has to be < 8 to get here
      //return (previousSunday < 8 && (_tempDateTime.dayofWeek - 1) = 0 && _tempDateTime.hour >= 2)
      return false;
    } // end else

    // ------------------------------- November -------------------------------

    // gets here only if month = November
    //In november we must be before the first Sunday to be dst.
    //That means the previous Sunday must be before the 2nd.
    if (previousSunday < 1)
    {
      // is not true for Sunday after 2am or any day after 1st Sunday any time
      return ((_tempDateTime.dayofWeek == 1 && _tempDateTime.hour < 2) || (_tempDateTime.dayofWeek > 1));
      //return true;
    } // end if (previousSunday < 1)
    else
    {
      // return false unless after first wk and dow = Sunday and hour < 2
      return (_tempDateTime.day <8 && _tempDateTime.dayofWeek == 1 && _tempDateTime.hour < 2);
    }  // end else
  }
}

unsigned long SNTPtime::adjustTimeZone(unsigned long _timeStamp, float _timeZone, byte _DayLightSaving) {
  _timeStamp += (unsigned long)(_timeZone *  3600.0); // adjust timezone
  if (_DayLightSaving ==1 && summerTime(_timeStamp)) _timeStamp += 3600; // European Summer time
  if (_DayLightSaving ==2 && daylightSavingTime(_timeStamp)) _timeStamp += 3600; // US daylight time
  return _timeStamp;
}

bool SNTPtime::setSNTPtime() {
  unsigned long entry=millis();
  int count = 0;
	do {
    if (count==0)	configTime(0,0,_SNTPserver);
    count++; if (count==50) count = 0;
  	delay(10);
	} while (millis()-entry<5000 && time(NULL)<946684800U); // 1.1.2000
	if (time(NULL)>100) return true;
	else return false;
}

// time zone is the difference to UTC in hours
// if _isDayLightSaving is 0, time in timezone is returned
// if _isDayLightSaving is 1, time will be adjusted to European Summer time


strDateTime SNTPtime::getTime(float _timeZone, bool _DayLightSaving)
{
	unsigned long  _currentTimeStamp = getLocalTime(_timeZone, _DayLightSaving);
	strDateTime _dateTime = ConvertUnixTimestamp(_currentTimeStamp);
	return _dateTime;
}

unsigned long SNTPtime::getLocalTime(float _timeZone, bool _DayLightSaving)
{
	unsigned long  _unixTime = time(NULL);
	DEBUG_BY_LEVEL_PRINT(2, F("unixTime: ")); DEBUG_BY_LEVEL_PRINTLN(2,_unixTime);
	unsigned long  _currentTimeStamp = adjustTimeZone(_unixTime, _timeZone, _DayLightSaving);
	return _currentTimeStamp;
}

String time_to_localtime_str(time_t t) {
  String localtime_str;
  SNTPtime dummy('\0');
  time_t localtime = dummy.adjustTimeZone(t, 1.0, 1);
  localtime_str = ctime(&localtime);
  return localtime_str. substring(0, localtime_str.length()-1);
}

String timeDiffToString(uint32_t td) {
  uint32_t tmp_time;
  String timeDiff;
  //timeDiff.reserve(80);
  tmp_time = td / 31536000U;
  timeDiff += String(tmp_time);
  timeDiff += F("years, ");
  td -= tmp_time*31536000U;
  tmp_time = td / 86400U;
  timeDiff += String(tmp_time);
  timeDiff += F("days, ");
  td -= tmp_time*86400U;
  tmp_time = td / 3600;
  timeDiff += String(tmp_time);
  timeDiff += F("hours, ");
  td -= tmp_time*3600;
  tmp_time = td / 60;
  timeDiff += String(tmp_time);
  timeDiff += F("minutes, ");
  td -= tmp_time*60;
  timeDiff += String(td);
  timeDiff += F("seconds");
  DEBUG_BY_LEVEL_PRINT(3, F("timeDiff.length: ")); DEBUG_BY_LEVEL_PRINTLN(3, timeDiff.length());
  return timeDiff;
}
