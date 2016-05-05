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

#define BITS_PER_LONG (sizeof(long) * 8)
#define LONGS_TO_FIT_BITS(x) ((((x)-1)/BITS_PER_LONG)+1)
#define testbit(in, b)	((in[(b)/BITS_PER_LONG] & (((unsigned long)1) << ((b)%BITS_PER_LONG))))


/*
 * Print exit message, append error (if there is one)
 * and exit with code 1
 */
static void die (const char * format, ...) {
    va_list vargs;
    va_start (vargs, format);
    vfprintf (stderr, format, vargs);
    if (errno != 0) //Is there no constant for ERROR SUCCESS?
        fprintf (stderr, ": %s",strerror(errno));
    
    fprintf (stderr, "\n");
    exit(1);
}


struct user_dev_events {
    __s32 events[EV_MAX];

//KEY_MAX is the biggest of the different events 
//    [at least for now - lets hope it remains that way]
    __s32 bits[EV_MAX][KEY_MAX];
};

/*
 * Read info from opened /dev/input/event* device and fill the passed user_dev_events and uinput_user_dev.
 * 
 * Kills program if it fails to read.
 */ 
void read_dev_info(int fd_event, struct user_dev_events *dev, struct uinput_user_dev *uidev) {
    struct input_absinfo absinfo;
    unsigned long bits[EV_MAX][LONGS_TO_FIT_BITS(KEY_MAX)];
    unsigned long event_types[LONGS_TO_FIT_BITS(EV_MAX)];
    
    int allow_eviocgbit[EV_MAX];
    memset(allow_eviocgbit, 0, sizeof(allow_eviocgbit));
    allow_eviocgbit[EV_KEY] = allow_eviocgbit[EV_REL] = allow_eviocgbit[EV_ABS] =
    allow_eviocgbit[EV_MSC] = allow_eviocgbit[EV_LED] = allow_eviocgbit[EV_SND] =  
    allow_eviocgbit[EV_FF] = allow_eviocgbit[EV_SW] = 1;
    
    int i,j,retlen;
    //TODO: ff_effects_max ?? 
    
    //Load device name
    if (ioctl (fd_event, EVIOCGNAME (UINPUT_MAX_NAME_SIZE), uidev->name) == -1) die("Line %d, ioctl",__LINE__);
    
    //Load id
    if (ioctl(fd_event, EVIOCGID, &(uidev->id)) == -1) die("Line %d, ioctl",__LINE__);
    
    if (ioctl(fd_event, EVIOCGBIT(0, EV_MAX), event_types) == -1) die("Line %d, ioctl",__LINE__);
    for (i = 0; i < EV_MAX; ++i) {
		if (!testbit(event_types, i)) continue;
        dev->events[i] = 1;
        if (!allow_eviocgbit[i]) continue;
        //Get more bits 
        retlen = ioctl(fd_event, EVIOCGBIT(i, KEY_MAX), bits[i]);
        if (retlen == -1) die("Line %d, ioctl (i: %d)",__LINE__,i);
        //Go over all bits although many are outside of the scope
        for(j=0; j < KEY_MAX; j++) {
            if (!testbit(bits[i], j)) continue;
            dev->bits[i][j] = 1;
        }
	}
    
    if (dev->events[EV_ABS]) {
        for (i = 0; i < ABS_MAX; i++) {
            if (!dev->bits[EV_ABS][i]) continue;
            if (ioctl(fd_event, EVIOCGABS(i), &absinfo) == -1) die("Line %d, ioctl",__LINE__);
            uidev->absmax[i] = absinfo.maximum;
            uidev->absmin[i] = absinfo.minimum;
            uidev->absflat[i] = absinfo.flat;
            uidev->absfuzz[i] = absinfo.fuzz;
        }
    }
}


/*
 * Write info in passed user_dev_events and uinput_user_dev to opened uinput file (fd_uinput).
 * Also creates the device!
 * 
 * Kills program if it fails to write.
 */
void set_dev_info(int fd_uinput, struct user_dev_events *dev, struct uinput_user_dev *uidev) {
    
    int translate[EV_MAX];
    int i,j;
    
    memset (translate, -1, sizeof(translate));
    
    translate[EV_KEY] = UI_SET_KEYBIT;
    translate[EV_LED] = UI_SET_LEDBIT;
    translate[EV_ABS] = UI_SET_ABSBIT;
    translate[EV_MSC] = UI_SET_MSCBIT;
    translate[EV_REL] = UI_SET_RELBIT;
    translate[EV_SND] = UI_SET_SNDBIT;
    translate[EV_FF] = UI_SET_FFBIT;
    translate[EV_SW] = UI_SET_SWBIT;
    //EV_REP?
    //EV_PWR?
    //UI_SET_PHYS?
    //UI_SET_PROPBIT?

    
    for (i = 0; i < EV_MAX; ++i) {
		if (!dev->events[i]) continue;
        
        if(ioctl(fd_uinput, UI_SET_EVBIT, i) < 0) die("Line %d, ioctl",__LINE__);
        //Go over all bits although many are outside of the scope
        for(j=0; j < KEY_MAX; j++) {
            if (!dev->bits[i][j] || translate[i] == -1) continue;
            if (ioctl(fd_uinput, translate[i], j) == -1) die("Line %d, ioctl",__LINE__);
        }        
	}
    
    //Write uidev data
    if(write(fd_uinput, uidev, sizeof(struct uinput_user_dev)) < 0) die("Line %d, write",__LINE__);
    
    //Create device
    if(ioctl(fd_uinput, UI_DEV_CREATE) < 0) die("Line %d, ioctl",__LINE__);
}


int main (int argc, char *argv[])
{
    int fd_in,fd_out,i,size = sizeof (struct input_event);
    struct input_event ev;
    char in_name[256] = "NULL";
    char new_name[UINPUT_MAX_NAME_SIZE];
    char *device = NULL;
    
    struct user_dev_events de;
    struct uinput_user_dev uinput;
    
    if (argc == 2) {
        device = argv[1];
    } else {
        die("USAGE: ./%s <event device>",argv[0]);
    }
    
    fd_out = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if(fd_out < 0) die("Line %d, open",__LINE__);


    //Open Device
    if ((fd_in = open (device, O_RDONLY)) == -1)
        die ("Failed to open device %s", device); 

    //Get exclusive access so nobody uses the original device...
    ioctl(fd_in, EVIOCGRAB, 1);

    
    //Read device info into de and uinput
    read_dev_info(fd_in,&de,&uinput);
    
    //Check that the device we are wrapping does not use BTN_START and BTN_SELECT
    if (de.bits[EV_KEY][BTN_START] || de.bits[EV_KEY][BTN_SELECT]) {
        die("Cannot translate to BTN_START and BTN_SELECT as it seems at least one of these keys are already used by the device\n");
    }
    
    //Unset keys to be replaced
    de.bits[EV_KEY][KEY_BACK] = 0;
    de.bits[EV_KEY][KEY_HOMEPAGE] = 0;
    
    //Set keys that will be translated to
    de.bits[EV_KEY][BTN_START] = 1;
    de.bits[EV_KEY][BTN_SELECT] = 1;
    
    snprintf(new_name,UINPUT_MAX_NAME_SIZE,"Wrapped(%s)",uinput.name);
    strcpy(uinput.name,new_name);
    
    set_dev_info(fd_out,&de,&uinput);
 
  while (1){
      //Read original device event
      if (read (fd_in, &ev, size) < size)
          die ("Line %d, read",__LINE__);
          
        //Translate KEY_ events. ( KEY_BACK -> BTN_START, KEY_HOMEPAGE -> BTN_SELECT )
        if (ev.type == EV_KEY && ev.code == KEY_BACK)
            ev.code = BTN_START;
            
        if (ev.type == EV_KEY && ev.code == KEY_HOMEPAGE)
            ev.code = BTN_SELECT;
            
        //Write device event
      if(write(fd_out, &ev, sizeof(struct input_event)) < 0)
        die("Line %d, write",__LINE__);
    }

    if(ioctl(fd_out, UI_DEV_DESTROY) < 0)
        die("Line %d, ioctl",__LINE__);
        
    close(fd_in);
    close(fd_out);

    return 0;
}
