/*
 * $Id: ble.c,v 1.3 2014/11/07 23:47:10 dhn Exp $
 * gcc -std=c99 -o ble ble.c -lbluetooth -lm
*/
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#define DEV_ID  "hci0"
#define BDADDR   "00:07:80:7F:59:9E"

static void add_to_white_list(int dev_id)
{
    int err, dd;
    bdaddr_t bdaddr;
    uint8_t bdaddr_type = LE_PUBLIC_ADDRESS;

    if (dev_id < 0)
        dev_id = hci_get_route(NULL);

    dd = hci_open_dev(dev_id);
    if (dd < 0) {
        perror("Could not open device");
        exit(1);
    }

    if (BDADDR)
        str2ba(BDADDR, &bdaddr);

    err = hci_le_add_white_list(dd, &bdaddr, bdaddr_type, 1000);
    hci_close_dev(dd);

    if (err < 0) {
        err = -errno;
        fprintf(stderr, "Can't add to white list: %s(%d)\n",
                strerror(-err), -err);
        exit(1);
    }
}

static uint16_t connect_to_device(int dev_id)
{
    int err, dd;
    bdaddr_t bdaddr;
    uint16_t interval, latency, max_ce_length, max_interval, min_ce_length;
    uint16_t min_interval, supervision_timeout, window, handle;
    uint8_t initiator_filter, own_bdaddr_type, peer_bdaddr_type;

    peer_bdaddr_type = LE_PUBLIC_ADDRESS;
    initiator_filter = 0x01; /* Use white list */

    if (dev_id <0)
        dev_id = hci_get_route(NULL);

    dd = hci_open_dev(dev_id);
    if (dd < 0) {
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

    err = hci_le_create_conn(dd, interval, window, initiator_filter,
            peer_bdaddr_type, bdaddr, own_bdaddr_type, min_interval,
            max_interval, latency, supervision_timeout,
            min_ce_length, max_ce_length, &handle, 25000);

    if (err < 0) {
        perror("Could not create connection");
        exit(1);
    }

    printf("Connect to %s - handle %d\n", BDADDR, handle);
    hci_close_dev(dd);

    return handle;
}

static void disconnect_from_device(int dev_id, uint16_t handle)
{
    int err, dd;
    uint8_t reason;

    if (dev_id < 0)
        dev_id = hci_get_route(NULL);

    dd = hci_open_dev(dev_id);
    if (dd < 0) {
        perror("Could not open device");
        exit(1);
    }

    err = hci_disconnect(dd, handle, HCI_OE_USER_ENDED_CONNECTION, 10000);
    if (err < 0) {
        perror("Could not disconnect");
        exit(1);
    }

    hci_close_dev(dd);
}

static int read_rssi(int dev_id, uint16_t handle)
{
    bdaddr_t bdaddr;
    int8_t rssi;
    int dd;

    if (BDADDR)
        str2ba(BDADDR, &bdaddr);

    dd = hci_open_dev(dev_id);
    if (dd < 0) {
        perror("Could not open device");
        exit(1);
    }

    if (hci_read_rssi(dd, handle, &rssi, 1000) < 0) {
        perror("Read RSSI failed");
        exit(1);
    }

    printf("RSSI return value: %d\n", rssi);

    hci_close_dev(dd);
    
    return rssi;
}

/*
 *  Calculated the estimated distance in meters to the beacon based on a
 *  reference rssi at 1m and the known actual rssi at the current location 
*/
static double calculate_distance(int rssi)
{
    double ratio = rssi*1.0/-58.0; /* txPower: -58 for blueIOT */

    if (rssi == 0) {
        return -1.0;
    }

    if (ratio < 1.0) {
        return pow(ratio, 10);
    } else {
        /*
         * FIXME: value1 = 0.42093, value2 = 6.9476, value3 = 0.54992
         *        value1 = 0.89976, value2 = 7.7095, value3 = 0.111
         *        value1 = 0.42093, value2 = 6.9476, value3 = 0.34992
        */
        return (0.42093)*pow(ratio, 6.9476) + 0.34992;
    }
}

int main(void)
{
    int rssi, dev_id = -1;
    uint16_t handle;

    dev_id = hci_devid(DEV_ID);
    if (dev_id < 0) {
        perror("Invalid device");
        exit(1);
    } else {
        add_to_white_list(dev_id);
        handle = connect_to_device(dev_id);

        /* DEBUG */
        for(int i=0; i<10; i++) {
            sleep(1);
            rssi = read_rssi(dev_id, handle);
            printf("Distance: approximate %f\n", calculate_distance(rssi));
            sleep(1);
        }

        disconnect_from_device(dev_id, handle);
    }
}
