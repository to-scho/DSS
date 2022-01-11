#!/bin/sh
if [ $# -eq 0 ]
then
  inputfile="StackTrace.txt"
  echo "No argument supplied for exception input text file, stack Trace decoding $inputfile"
else
  inputfile=$1
  echo "Stack Trace decoding $inputfile"
fi
java -jar ~/.platformio/EspStackTraceDecoder.jar ~/.platformio/packages/toolchain-xtensa/bin/xtensa-lx106-elf-addr2line ~/Work/ESP8266/deep_sleep_sensor/.pioenvs/sensor/firmware.elf $inputfile
