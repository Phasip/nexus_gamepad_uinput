# uinput driver for the nexus gamepad.

Passes through all events except KEY_BACK and KEY_HOMEPAGE, which are translated to
BTN_SELECT and BTN_START

### Why?
Because Retropie didn't recognize those keys as it thought they were from the keyboard.

### How to use?
Make sure you have at installed and that the uinput module is loaded.

Modify the 99-nexus-gamepad-uinput.rules to match the location of the repo and move to /etc/udev/rules.d/

Compile the program using "make" and be happy.

### Comments
The udev stuff in this is largely based on https://github.com/adolson/NexusGamepad - which uses xboxdrv instead of individual uinput program. As I had problems with xboxdrv I decided to make this.

### TODO
Automatically parse evdev buttons and report instead of have them hardcoded

Support handling l2 and r2 as buttons instead of axises

