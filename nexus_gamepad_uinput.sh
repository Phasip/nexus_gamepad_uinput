#!/bin/sh

# rename to avoid programs using non-wrapped input
mv /dev/input/event$1 /dev/input/nexus$1

#If you start nexus_gamepad_uinput with crontab and option -n, then remove the code below


#Launch wrapper - using at so udev doesn't kill it
echo "/home/pi/nexus_gamepad_uinput/nexus_gamepad_uinput -d /dev/input/nexus$1 &" | at now
