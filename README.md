# uinput driver for the nexus gamepad.

Passes through all events except KEY_BACK and KEY_HOMEPAGE, which are translated to
BTN_SELECT and BTN_START

### Why?
Because Retropie didn't recognize those keys as it thought they were from the keyboard.

### How to use?
Compile the program using "make"

Make sure you have at installed and that the uinput module is loaded.

Modify the 99-nexus-gamepad-uinput.rules to match the location of the repo and move to /etc/udev/rules.d/

##### Using with the ```-n <device name>``` argument
This option sets the program to monitor for new /dev/input/nexusNN files
and check that their device name match the argument given (ie "ASUS Gamepad")

If the device matches then it grabs the device and starts passing events through
to the uinput device. When the device dissapears/disconnects the uinput device
is kept open and the next time a device with a matching name is connected
its inputs will be transferred to the same uinput device.
[May solve problems with emulator that can't handle controller reconnects/timeouts]

To do this you should modify the 99-nexus-gamepad-uinput.rules to only rename the device
and by some method start the nexus_Gamepad_uinput program with the -n argument. 
[ie using @reboot in crontab]

### Comments
The udev stuff in this is largely based on https://github.com/adolson/NexusGamepad - which uses xboxdrv instead of individual uinput program. As I had problems with xboxdrv I decided to make this.

### TODO
Automatically parse evdev buttons and report instead of have them hardcoded

Support handling l2 and r2 as buttons instead of axises

Some logging and more/better error-handling

