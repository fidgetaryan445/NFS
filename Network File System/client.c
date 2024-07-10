#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "udp.h"

#define BUFFER_SIZE (1000)

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <command>\n", argv[0]);
        exit(1);
    }

    struct sockaddr_in addrSnd, addrRcv;
    int sd = UDP_Open(20000);
    int rc = UDP_FillSockAddr(&addrSnd, "localhost", 10000);

    char message[BUFFER_SIZE];
    sprintf(message, "%s", argv[1]);
    for (int i = 2; i < argc; i++) {
        strcat(message, " ");
        strcat(message, argv[i]);
    }

    printf("client:: send message [%s]\n", message);
    rc = UDP_Write(sd, &addrSnd, message, BUFFER_SIZE);
    if (rc < 0) {
        printf("client:: failed to send\n");
        exit(1);
    }

    printf("client:: wait for reply...\n");
    rc = UDP_Read(sd, &addrRcv, message, BUFFER_SIZE);
    printf("client:: got reply [size:%d contents:(%s)]\n", rc, message);
    return 0;
}
