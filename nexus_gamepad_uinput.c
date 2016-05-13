#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h> 
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
#define DEV_INPUT "/dev/input/"

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
void help(char *progname) {
    printf("USAGE: ./%s [<device>|-n <device name>]\n",progname);
    printf("\t<device>\tTranslate input from device and exit when device is disconnected\n");
    printf("\t-n <device name>\tHook device with matching name, search for new device on disconnect \n\t\t- keeping the same translated uinput device. This may help with controls stopping working in some emulators\n");
    exit(0);
}

int open_and_lock_evdev(char * evdev_device) {
    int fd_in;
    
    if ((fd_in = open (evdev_device, O_RDONLY)) == -1)
       return -1;
    
    //Get exclusive access so nobody uses the original device...
    //Maybe should check for -EBUSY specifically?
    if (ioctl(fd_in, EVIOCGRAB, 1) < 0)
        return -2;
        
    
    return fd_in;
}
int close_and_unlock_evdev(int fd) {
    //TODO: Should we handle if we fail to ungrab the device?
    ioctl(fd, EVIOCGRAB, 0);
    close(fd);
}
int create_uinput_device(int fd_in) {
    struct user_dev_events de;
    struct uinput_user_dev uinput;
    char in_name[256] = "NULL";
    char new_name[UINPUT_MAX_NAME_SIZE];
    int fd_out;
    
    fd_out = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if(fd_out < 0) die("Line %d, open",__LINE__);


    //Open Device
    
    
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
    return fd_out;
    
}
int kill_uinput_device(int fd_out) {
    //TODO: Do something useful if we fail...
    ioctl(fd_out, UI_DEV_DESTROY); 
    close(fd_out);
}
int open_matching_device(char * device_name) {
    DIR           *dir;
    struct dirent *dir_entry;
    char read_name[UINPUT_MAX_NAME_SIZE]; //Wonder what the const for the real max name is
    char device_file[PATH_MAX];
    int fd_in;
    
    strcpy(device_file,DEV_INPUT);
    
    //TODO: This could probably be made more advanced, 
    //    ie first look at existing and then simply monitor for changes...
    while (1) {
        dir = opendir(DEV_INPUT);
        if (!dir)
            return -1;
            
        while ((dir_entry = readdir(dir)) != NULL)
        {
          //Do not hog the computer!
          usleep(20*1000);
          
          if (dir_entry->d_type != DT_CHR)
             continue;
             
          if(strncmp("event",dir_entry->d_name, strlen("event")))
            continue;
          
          strcpy(device_file+strlen(DEV_INPUT),dir_entry->d_name);
          //printf("Is event device: %s!\n",device_file);   
          
          fd_in = open_and_lock_evdev(device_file);
          if (fd_in < 0) {
              //printf("Fail to lock device!\n");
              continue;
          }
          if (ioctl (fd_in, EVIOCGNAME (UINPUT_MAX_NAME_SIZE), read_name) == -1) {
              //printf("Fail to get name!\n");
              close_and_unlock_evdev(fd_in);
              continue;
          }
          
          if (strcmp(read_name,device_name)) {
              close_and_unlock_evdev(fd_in);
              continue;
          }
          
          printf("Device: %s name: %s!\n",device_file,read_name);
          
          closedir(dir);
          return fd_in;
        }

        closedir(dir);
    }
    
}

int wrap_device(int fd_out, int fd_in) {    
      struct input_event ev;
      int size = sizeof (struct input_event);
      while (1){
      //Read original device event
      if (read (fd_in, &ev, size) < size)
          return -2;
          
        //Translate KEY_ events. ( KEY_BACK -> BTN_START, KEY_HOMEPAGE -> BTN_SELECT )
        if (ev.type == EV_KEY && ev.code == KEY_BACK)
            ev.code = BTN_START;
            
        if (ev.type == EV_KEY && ev.code == KEY_HOMEPAGE)
            ev.code = BTN_SELECT;
            
        //Write device event
      if(write(fd_out, &ev, sizeof(struct input_event)) < 0)
        return -1;
    }
    close(fd_in);
    return 0;
}
int main (int argc, char *argv[])
{
    int fd_in,fd_out,i;
    
    
    char *device = NULL;
    
    if (argc == 2) {
        device = argv[1];
        fd_in = open_and_lock_evdev(device);
        if (fd_in < 0)
            die("Line %d, open_and_lock_evdev(%s)",__LINE__,device);
            
        fd_out = create_uinput_device(fd_in);
        if (wrap_device(fd_out,fd_in))
            die("Line %d, wrap_device",__LINE__);
        
        kill_uinput_device(fd_out);
        close_and_unlock_evdev(fd_in);
            
    } else if (argc == 3 && !strcmp("-n",argv[1])) {
        do {
            fd_in = open_matching_device(argv[2]);
        } while (fd_in < 0);
        
        fd_out = create_uinput_device(fd_in);
        
        while(1) {
            wrap_device(fd_out,fd_in);
            close_and_unlock_evdev(fd_in);
            
            do {
                fd_in = open_matching_device(argv[2]);
            } while (fd_in < 0);
            
        }
        kill_uinput_device(fd_out);
        
    } else {
        help(argv[0]);
        
    }
    
    return 0;
}
