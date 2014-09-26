#!/bin/bash
#
# ARM System Profiler usage example
#
# Copyright (C) 2014 ARM Limited
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed "as is" WITHOUT ANY WARRANTY of any
# kind, whether express or implied; without even the implied warranty
# of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

#
# This script demonstrates the usage of ARM System Profiler platform to capture
# and extract various performance-related events from the System Profiler
# hardware.
#
# The ARM System Profiler driver exposes System Profiler registers as debugfs
# files through which user space can access them. The user programs necessary
# filters, and configure counters, and later triggers capture event.  Upon a
# capture event, System Profiler exposes the collected metrics though its shadow
# registers
#
# This script collects some unfiltered events from AXI bus monitors connected to
# A57 and A53 clusters, and operates the Profiler in software stream mode. This
# however is only a example and doesn't of capabilities of System Profiler. For
# more information, see ARM DDI0520 System Profiler Technical Reference Manual
#
# ARM System Profiler driver is located at drivers/bus/arm-system-profiler.c
#

# System Profiler root under debugfs
sp_root="/sys/kernel/debug/arm-system-profiler"

# The command to execute, after which we capture metrics. The command itself has
# no significance, except that it exercises a decent number of data transactions
# through the system
command="tar cz /usr/bin 2>/dev/null | md5sum &>/dev/null"

# Check if debugfs is mounted
if [ ! -d "/sys/kernel/debug" ]; then
	echo "Debugfs not mounted"
	exit 1
fi

# Check if System Profiler driver is enabled
if [ ! -d "$sp_root" ]; then
	echo "Unable to locate System Profiler root dir '$sp_root'"
	echo "Is ARM System Profiler driver enabled?"
	exit 1
fi

# Obtain System Profiler device root. We pick the first device listed.
pushd "$sp_root" &>/dev/null
if [ -z "$sp_device" ]; then
	sp_device="$(find -maxdepth 1 -type d | sed -n '2{s/^\.\///;p;q}')"
	if [ -z "$sp_device" ]; then
		echo "No System Profiler instance found under '$sp_root'"
		exit 1
	fi
elif [ ! -d "$sp_device" ]; then
	echo "No System Profiler instance '$sp_device' found under '$sp_root'"
	exit 1
fi
popd &>/dev/null

# System Profiler directory
system_profiler="$sp_root/$sp_device"

# Verify the monitors what we need have been implemented
im="$(cat "$system_profiler/MTR_IM")"
for p in 0 1; do
	let "imp = im & (1 << $p)"
	if [ "$imp" == 0 ]; then
		echo "$sp_device: Monitor $p not implemented"
		exit 1
	fi

	if [ ! -d "$system_profiler/port$p" ]; then
		echo "$sp_device: No port $p"
		exit 1
	fi
done

#
# Now programm actual registers
#
function regw() {
	[ "$debug" ] && printf " write %-20s <- %s\n" $1 $2
	echo "$2" > "$1"
}

cd "$system_profiler"

# Disable System Profiler
regw CTRL 0x0

# Configure in software stream mode
regw CFG 0x0

# Enable Monitors 0 and 1
regw MTR_EN 0x3

# Disable interrupts
regw INT_EN 0x0

# Program both AXI monitors to accumulation mode
regw port0/ABM_MODE 0x0
regw port1/ABM_MODE 0x0

# Disable all filters
regw port0/FLTEN 0x0
regw port1/FLTEN 0x0

# Since we aren't using any filters, configure counters to count unfiltered
# transactions
c_cfg=(0 1 2 3)
for p in 0 1; do
	for c in 1 2 3 4; do
		for rw in R W; do
			regw "port$p/C${rw}${c}SEL" "${c_cfg[$c - 1]}"
		done
	done
done

# Enable System Profiler
regw CTRL 0x5

# Run 6 different instances of our command which hopefully will spawn on all
# different cores, and wait for them finish
echo "Spawning commands:"
for i in {1..6}; do
	eval "{ $command; } &"
	echo "  command: [$!] $command"
done

echo "Waiting for commands to finish..."
wait

# Commands have finished running. Generate a capture event so that the metrics
# will be transferred to shadow registers
regw CTRL 0x2

# Wait until System Profiler has finished capturing from all its monitors. This
# will be reflected in SP_STATUS.CAP_ACK
while :; do
	ack=$(cat SP_STATUS)
	let "ack &= 1"
	[ "$ack" == 1 ] && break
done

# Now that System Profiler has captured the metrics, all data must be available
# in the shadow registers to collect. Print it out
echo
echo "Shadow registers post capture:"
for p in 0 1; do
	echo "  Port $p:"
	for i in {0..11}; do
		printf "    %-4s: %s\n" "SR$i" "$(cat port$p/SR$i)"
	done
	echo
done

# Disable System Profiler (flip bit 0)
regw CTRL 0x4

exit 0

