#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/input.h>
#include <linux/uinput.h>

#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/select.h>
#include <sys/time.h>
#include <termios.h>
#include <signal.h>


static void die (const char * format, ...) {
    va_list vargs;
    va_start (vargs, format);
    vfprintf (stderr, format, vargs);
    if (errno != 0) //Is there no constant for ERROR SUCCESS?
        fprintf (stderr, ": %s",strerror(errno));
    
    fprintf (stderr, "\n");
    exit(1);
}


//Lists that we will register so users know what events we emit.
int evbits[] = {EV_SYN,EV_MSC,EV_LED,EV_ABS,EV_KEY};
int keybits[] = {BTN_START,BTN_SELECT,BTN_SOUTH,BTN_EAST,BTN_NORTH,BTN_WEST,BTN_TL,BTN_TR,BTN_MODE,BTN_THUMBL,BTN_THUMBR};
int absbits[] = {ABS_X,ABS_Y,ABS_Z,ABS_RZ,ABS_GAS,ABS_BRAKE,ABS_HAT0X,ABS_HAT0Y,ABS_MISC,41,42};
int ledbits[] = {LED_NUML,LED_CAPSL,LED_SCROLLL,LED_COMPOSE};

//Loop through array and call ioctl for each item
void register_bits(int fd,int arr[],int len,int request) {
    int i;
    for(i = 0; i < len; i++)
        if(ioctl(fd, request, arr[i]) < 0) die("error: ioctl");
}
//Simply sets all the data for the device, found in evtest initial output.
void prepare_uinput_device(int fd_out,char*name) {
    int i;
    struct uinput_user_dev uidev;
    memset(&uidev, 0, sizeof(uidev));
    
    register_bits(fd_out,evbits,sizeof(evbits)/sizeof(int),UI_SET_EVBIT);
    register_bits(fd_out,keybits,sizeof(keybits)/sizeof(int),UI_SET_KEYBIT);
    register_bits(fd_out,ledbits,sizeof(ledbits)/sizeof(int),UI_SET_LEDBIT);
    register_bits(fd_out,absbits,sizeof(absbits)/sizeof(int),UI_SET_ABSBIT);
    
    if(ioctl(fd_out, UI_SET_MSCBIT, MSC_SCAN) < 0) die("error: ioctl"); //Register lone MSC_ event

    //Fill extra absdata, currently only ABS_HAT0[XY] differ, so we do it this way
    for(i = 0; i < sizeof(absbits) / sizeof(int); i++) {
        if (absbits[i] == ABS_HAT0X || absbits[i] == ABS_HAT0Y) {
            uidev.absmax[absbits[i]] = 1;
            uidev.absmin[absbits[i]] = -1;
            uidev.absflat[absbits[i]] = 1;
            
        } else {
            uidev.absmax[absbits[i]] = 255;
            uidev.absmin[absbits[i]] = 0;
            uidev.absflat[absbits[i]] = 15;
        }
    }
    
    //Set device name
    snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "%s",name);
    
    //Set device data
    uidev.id.bustype = BUS_USB;
    uidev.id.vendor  = 0x1;
    uidev.id.product = 0x1;
    uidev.id.version = 1;

    if(write(fd_out, &uidev, sizeof(uidev)) < 0) die("error: write");
    
    //Create device
    if(ioctl(fd_out, UI_DEV_CREATE) < 0) die("error: ioctl");
}

int main (int argc, char *argv[])
{
    int fd_in,fd_out,i,size = sizeof (struct input_event);
    struct input_event ev;
    char in_name[256] = "NULL";
    char new_name[UINPUT_MAX_NAME_SIZE];
    char *device = NULL;
    
    
    /*if (argc == 3 && !strcmp("-d",argv[1])) {
        device = argv[2];
        daemon(0,0);
    } else */
    if (argc == 2) {
        device = argv[1];
    } else {
        die("USAGE: ./%s <event device>",argv[0]);
    }
    
    fd_out = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if(fd_out < 0) die("error: open");


    //Open Device
    if ((fd_in = open (device, O_RDONLY)) == -1)
        die ("Failed to open device %s", device); 

    //Get exclusive access so nobody uses the original device...
    ioctl(fd_in, EVIOCGRAB, 1);

    //Load device name
    ioctl (fd_in, EVIOCGNAME (sizeof (in_name)), in_name);
    snprintf(new_name, UINPUT_MAX_NAME_SIZE, "Wrapped(%s)",in_name);
    
    prepare_uinput_device(fd_out,new_name);

 
  while (1){
      //Read original device event
      if (read (fd_in, &ev, size) < size)
          die ("read()");
        
        //Translate KEY_ events. ( KEY_BACK -> BTN_START, KEY_HOMEPAGE -> BTN_SELECT )
        if (ev.type == EV_KEY && ev.code == KEY_BACK)
            ev.code = BTN_START;
            
        if (ev.type == EV_KEY && ev.code == KEY_HOMEPAGE)
            ev.code = BTN_SELECT;
        //Write device event
      if(write(fd_out, &ev, sizeof(struct input_event)) < 0)
        die("write()");
    }

    if(ioctl(fd_out, UI_DEV_DESTROY) < 0)
        die("error: ioctl");
        
    close(fd_in);
    close(fd_out);

    return 0;
}
