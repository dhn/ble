/*
 * Copyright (c) 2014, Dennis 'dhn' Herrmann <dhn@4bit.ws>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the names of the company nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY <Dennis 'dhn' Herrmann> ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <Dennis 'dhn' Herrmann> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id: ble.c,v 1.7 2014/11/23 00:21:29 dhn Exp $
 *
*/
#define _XOPEN_SOURCE 500

#include <math.h>
#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#define DEV_ID  "hci1"
#define BDADDR  "00:07:80:7F:59:9E"

/* typedefs */
typedef struct {
    int dd;
    int err;
} hci_typ;

typedef struct {
    int screen;
    Window root, win;
    Pixmap pmap;
} Lock;

/* function declarations */
static void die(const char*, ...);
static void unlockscreen(Display*, Lock*);
static Lock *lockscreen(Display*, int);
static void add_to_white_list(int dev_id);
static uint16_t connect_to_device(int dev_id);
static void disconnect_from_device(int dev_id, uint16_t handle);
static void encryption(int dev_id, uint16_t handle);
static void check_version(int dev_id);
static int read_rssi(int dev_id, uint16_t handle);
static double calculate_distance(int rssi);

/* variables */
static Lock **locks;
static int nscreens;
static Bool running = True;
static hci_typ typ;
static bdaddr_t bdaddr;
static uint8_t bdaddr_type = LE_PUBLIC_ADDRESS;

void
add_to_white_list(int dev_id)
{
    if (dev_id < 0)
        dev_id = hci_get_route(NULL);

    if ((typ.dd = hci_open_dev(dev_id)) < 0) {
        perror("Could not open device");
        exit(1);
    }

    if (BDADDR)
        str2ba(BDADDR, &bdaddr);

    typ.err = hci_le_add_white_list(typ.dd, &bdaddr, bdaddr_type, 1000);
    hci_close_dev(typ.dd);

    if (typ.err < 0) {
        typ.err = -errno;
        printf("Can't add to white list!\n");
        exit(1);
    }
}

uint16_t
connect_to_device(int dev_id)
{
    uint16_t interval, latency, max_ce_length, max_interval, min_ce_length;
    uint16_t min_interval, supervision_timeout, window, handle;
    uint8_t initiator_filter, own_bdaddr_type;

    initiator_filter = 0x01; /* Use white list */

    if (dev_id < 0)
        dev_id = hci_get_route(NULL);

    if ((typ.dd = hci_open_dev(dev_id)) < 0) {
        perror("Could not open device");
        exit(1);
    }

    memset(&bdaddr, 0, sizeof(bdaddr_t));
    if (BDADDR)
        str2ba(BDADDR, &bdaddr);

    interval = htobs(0x0004);
    window = htobs(0x0004);
    own_bdaddr_type = 0x00;
    min_interval = htobs(0x000F);
    max_interval = htobs(0x000F);
    latency = htobs(0x0000);
    supervision_timeout = htobs(0x0C80);
    min_ce_length = htobs(0x0001);
    max_ce_length = htobs(0x0001);

    typ.err = hci_le_create_conn(typ.dd, interval, window, initiator_filter,
            bdaddr_type, bdaddr, own_bdaddr_type, min_interval,
            max_interval, latency, supervision_timeout,
            min_ce_length, max_ce_length, &handle, 25000);

    if (typ.err < 0) {
        perror("Could not create connection");
        exit(1);
    }

    hci_close_dev(typ.dd);

    return handle;
}

void
disconnect_from_device(int dev_id, uint16_t handle)
{
    if (dev_id < 0)
        dev_id = hci_get_route(NULL);

    if ((typ.dd = hci_open_dev(dev_id)) < 0) {
        perror("Could not open device");
        exit(1);
    }

    typ.err = hci_disconnect(typ.dd, handle, HCI_OE_USER_ENDED_CONNECTION, 10000);
    if (typ.err < 0) {
        perror("Could not disconnect");
        exit(1);
    }

    hci_close_dev(typ.dd);
}

void
encryption(int dev_id, uint16_t handle)
{
    if (BDADDR)
        str2ba(BDADDR, &bdaddr);
    
    if ((typ.dd = hci_open_dev(dev_id)) < 0) {
        perror("Could not open device");
        exit(1);
    }

    if (hci_encrypt_link(typ.dd, handle, 1, 25000) < 0) {
        perror("HCI set encryption request failed");
        exit(1);
    }

    hci_close_dev(typ.dd);
}

void
check_version(int dev_id)
{
    struct hci_version ver;
    char *lmpver;

    if ((typ.dd = hci_open_dev(dev_id)) < 0) {
        perror("Could not open device");
        exit(1);
    }

    if (hci_read_local_version(typ.dd, &ver, 1000) < 0) {
        perror("Can't read version info hci0");
        exit(1);
    }

    lmpver = lmp_vertostr(ver.lmp_ver);

    if (strcmp(lmpver, "4.0")) {
        printf("You need a Bluetooth 4.0 LE device\n");
        bt_free(lmpver);
        exit(1);
    } else {
        bt_free(lmpver);
    }

    hci_close_dev(typ.dd);
}

int
read_rssi(int dev_id, uint16_t handle)
{
    int8_t rssi;

    if (BDADDR)
        str2ba(BDADDR, &bdaddr);

    if ((typ.dd = hci_open_dev(dev_id)) < 0) {
        perror("Could not open device");
        exit(1);
    }

    if (hci_read_rssi(typ.dd, handle, &rssi, 1000) < 0) {
        perror("Read RSSI failed");
        exit(1);
    }

    hci_close_dev(typ.dd);
    
    return rssi;
}

/*
 *  Calculated the estimated distance in meters to the beacon based on a
 *  reference rssi at 1m and the known actual rssi at the current location 
*/
double
calculate_distance(int rssi)
{
    double ratio = rssi*1.0/-58.0; /* txPower: -58 for blueIOT */

    if (rssi == 0)
        return -1.0;

    if (ratio < 1.0) {
        return pow(ratio, 10);
    } else {
        /*
         * FIXME: value1 = 0.42093, value2 = 6.9476, value3 = 0.54992
         *        value1 = 0.89976, value2 = 7.7095, value3 = 0.111
         *        value1 = 0.42093, value2 = 6.9476, value3 = 0.34992
        */
        return (0.42093)*pow(ratio, 6.9476) + 0.54992;
        /* return (0.22093)*pow(ratio, 6.9476) + 0.2344; */
    }
}

void
die(const char *errstr, ...) {
	va_list ap;

	va_start(ap, errstr);
	vfprintf(stderr, errstr, ap);
	va_end(ap);
	exit(EXIT_FAILURE);
}

void
unlockscreen(Display *dpy, Lock *lock) {
	if(dpy == NULL || lock == NULL)
		return;

	XUngrabPointer(dpy, CurrentTime);
	XFreePixmap(dpy, lock->pmap);
	XDestroyWindow(dpy, lock->win);

	free(lock);
}

Lock *
lockscreen(Display *dpy, int screen) {
	char curs[] = {0, 0, 0, 0, 0, 0, 0, 0};
	unsigned int len;
	Lock *lock;
	XColor black, dummy;
	XSetWindowAttributes wa;
	Cursor invisible;

	if(dpy == NULL || screen < 0)
		return NULL;

	lock = malloc(sizeof(Lock));
	if(lock == NULL)
		return NULL;

	lock->screen = screen;

	lock->root = RootWindow(dpy, lock->screen);

	/* init */
	wa.override_redirect = 1;
	wa.background_pixel = BlackPixel(dpy, lock->screen);
	lock->win = XCreateWindow(dpy, lock->root, 0, 0, DisplayWidth(dpy, lock->screen), DisplayHeight(dpy, lock->screen),
			0, DefaultDepth(dpy, lock->screen), CopyFromParent,
			DefaultVisual(dpy, lock->screen), CWOverrideRedirect | CWBackPixel, &wa);
	XAllocNamedColor(dpy, DefaultColormap(dpy, lock->screen), "black", &black, &dummy);
	lock->pmap = XCreateBitmapFromData(dpy, lock->win, curs, 8, 8);
	invisible = XCreatePixmapCursor(dpy, lock->pmap, lock->pmap, &black, &black, 0, 0);
	XDefineCursor(dpy, lock->win, invisible);
	XMapRaised(dpy, lock->win);
	for(len = 1000; len; len--) {
		if(XGrabPointer(dpy, lock->root, False, ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
			GrabModeAsync, GrabModeAsync, None, invisible, CurrentTime) == GrabSuccess)
			break;
		usleep(1000);
	}
	if(running && (len > 0)) {
		for(len = 1000; len; len--) {
			if(XGrabKeyboard(dpy, lock->root, True, GrabModeAsync, GrabModeAsync, CurrentTime)
				== GrabSuccess)
				break;
			usleep(1000);
		}
		running = (len > 0);
	}

	if(!running) {
		unlockscreen(dpy, lock);
		lock = NULL;
	}
	else 
		XSelectInput(dpy, lock->root, SubstructureNotifyMask);

	return lock;
}

int
main(void)
{
    Display *dpy;
    int screen, rssi, dev_id = -1;
    uint16_t handle;
    
    running = True;

    dev_id = hci_devid(DEV_ID);
    if (dev_id < 0) {
        perror("Invalid device");
        exit(1);
    } else {
        if (!(dpy = XOpenDisplay(0)))
            die("ble: cannot open display");

        check_version(dev_id);
        add_to_white_list(dev_id);
        handle = connect_to_device(dev_id);
        sleep(1);
        /* encryption(dev_id, handle); */

        /* Get the number of screens in display "dpy" and blank them all. */
        nscreens = ScreenCount(dpy);
        locks = malloc(sizeof(Lock *) * nscreens);
        if (locks == NULL)
            die("ble: malloc: %s", strerror(errno));

        /* while(running) { */
        /*     sleep(1); */
        /*     rssi = read_rssi(dev_id, handle); */
        /*     printf("\t%d\t%f\n", rssi, calculate_distance(rssi)); */
        /*     if ((calculate_distance(rssi) >= 2.1) && (rssi <= -71 && rssi >= -75)) { */
        /*     #<{(| if (rssi <= -71 && rssi >= -75) { |)}># */
        /*         printf("%d\t%f\n", rssi, calculate_distance(rssi)); */
        /*         sleep(1); */
        /*     }       */
        /* } */
        while(running) {
            rssi = read_rssi(dev_id, handle);
            sleep(2);
            if ((calculate_distance(rssi) >= 2.0) && (rssi <= -71 && rssi >= -75)) {
                if (locks != NULL) {
                    nscreens = ScreenCount(dpy);
                    for (screen = 0; screen < nscreens; screen++)
                        locks[screen] = lockscreen(dpy, screen);
                    XSync(dpy, False);
                }
                running = False;
            }
        }

        printf("SLEEP\n");
        sleep(10);

        while(! running) {
            rssi = read_rssi(dev_id, handle);
            if ((calculate_distance(rssi) <= 2.0) && (rssi <= -30 && rssi >= -70)) {
                if (locks != NULL) {
                    /* Distance ok, unlock everything and quit. */
                    for (screen = 0; screen < nscreens; screen++)
                        unlockscreen(dpy, locks[screen]);
                }
                running = True;
            }
        }
        disconnect_from_device(dev_id, handle);

        if (locks != NULL)
            free(locks);
        XCloseDisplay(dpy);
    }

    return EXIT_SUCCESS;
}
