#!/bin/sh

# rename to avoid programs using non-wrapped input
mv /dev/input/event$1 /dev/input/nexus$1

#Launch wrapper - using at so udev doesn't kill it
echo "/home/pi/nexus_gamepad_uinput/nexus_gamepad_uinput /dev/input/nexus$1 &" | at now
