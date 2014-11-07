/*
 * $Id: ble.c,v 1.1 2014/11/07 17:34:59 dhn Exp $
 * gcc -std=c99 -o ble ble.c -lbluetooth
*/
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#define DEV_ID  "hci0"
#define BADDR   "00:07:80:7F:59:9E"

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

    if (BADDR)
        str2ba(BADDR, &bdaddr);

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
    if (BADDR)
        str2ba(BADDR, &bdaddr);

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

    printf("Connect to %s - handle %d\n", BADDR, handle);
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

int main(void)
{
    int dev_id = -1;
    uint16_t handle;

    dev_id = hci_devid(DEV_ID);
    if (dev_id < 0) {
        perror("Invalid device");
        exit(1);
    } else {
        add_to_white_list(dev_id);
        handle = connect_to_device(dev_id);
        sleep(10); /* DEBUG */
        disconnect_from_device(dev_id, handle);
    }
}
