#!/bin/bash

BLOATY=~/.platformio/bloaty/bloaty

if [ -z "$BLOATY" ]; then
	echo "Please set BLOATY"
	exit 1
fi

bloaty=$BLOATY

app_out=~/Work/ESP8266/deep_sleep_sensor/.pioenvs/sensor/firmware.elf

echo $bloaty
bloaty_cmd="$bloaty $app_out -d segments,sections,compileunits,symbols"
$bloaty_cmd
