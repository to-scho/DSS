#!/bin/bash
# Print data, rodata, bss sizes in bytes
#
# Usage:
# OBJDUMP=../xtensa-lx106-elf/bin/xtensa-lx106-elf-objdump ./mem_usage.sh app.out [total_mem_size]
#
# If you specify total_mem_size, free heap size will also be printed
# For esp8266, total_mem_size is 81920
#

OBJDUMP=~/.platformio/packages/toolchain-xtensa/bin/xtensa-lx106-elf-objdump
SIZE=/usr/bin/size
NM=~/.platformio/packages/toolchain-xtensa/bin/xtensa-lx106-elf-nm

if [ -z "$OBJDUMP" ]; then
	echo "Please set OBJDUMP"
	exit 1
fi
if [ -z "$SIZE" ]; then
	echo "Please set SIZE"
	exit 1
fi
if [ -z "$NM" ]; then
	echo "Please set NM"
	exit 1
fi

objdump=$OBJDUMP
sizecmd=$SIZE
nmcmd=$NM

app_out=~/Work/ESP8266/deep_sleep_sensor/.pioenvs/sensor/firmware.elf
total_size=81920
used=0

function print_usage {
	name=$1
	start_sym="_${name}_start"
	end_sym="_${name}_end"

	objdump_cmd="$objdump -t $app_out"
	addr_start=`$objdump_cmd | grep " $start_sym" | cut -d " " -f1 | tr 'a-f' 'A-F'`
	addr_end=`$objdump_cmd | grep " $end_sym" | cut -d " " -f1 | tr 'a-f' 'A-F'`
	size=`printf "ibase=16\n$addr_end - $addr_start\n" | bc`
	used=$(($used+$size))
	echo $name: $size
}
echo $objdump
print_usage data
print_usage rodata
print_usage bss
echo total: $used
if [[ ! -z "$total_size" ]]; then
	echo free: $(($total_size-$used))
fi

echo " "
echo $nmcmd
nm_cmd="$nmcmd --print-size --size-sort --radix=x --line-numbers $app_out"
$nm_cmd | grep 3ff | grep tobias/Work
# | grep -E " B | b | d | D | R | r | V | v "

echo " "
echo $sizecmd
size_cmd="$sizecmd -A -x $app_out"
$size_cmd | grep -E '\.data|\.rodata|\.bss'
