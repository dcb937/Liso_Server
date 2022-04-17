/******************************************************************************
 * echo_client.c                                                               *
 *                                                                             *
 * Description: This file contains the C source code for an echo client.  The  *
 *              client connects to an arbitrary <host,port> and sends input    *
 *              from stdin.                                                    *
 *                                                                             *
 * Authors: Athula Balachandran <abalacha@cs.cmu.edu>,                         *
 *          Wolf Richter <wolf@cs.cmu.edu>                                     *
 *                                                                             *
 *******************************************************************************/

#include "parse.h"
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define ECHO_PORT 9999
#define BUF_SIZE 41000

int main(int argc, char *argv[])
{
    if (argc != 4) {
        fprintf(stderr, "usage: %s <server-ip> <port> <target-file>", argv[0]);
        return EXIT_FAILURE;
    }

    char *buf = (char *)malloc(BUF_SIZE);
    // char buf[4096];
    int status, sock;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    struct addrinfo *servinfo;       // will point to the results
    hints.ai_family = AF_INET;       // IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
    hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

    if ((status = getaddrinfo(argv[1], argv[2], &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s \n", gai_strerror(status));
        return EXIT_FAILURE;
    }

    if ((sock = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol)) == -1) {
        fprintf(stderr, "Socket failed");
        return EXIT_FAILURE;
    }

    if (connect(sock, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
        fprintf(stderr, "Connect");
        return EXIT_FAILURE;
    }

    char msg[BUF_SIZE];

    FILE *fp = NULL;
    fp = fopen(argv[3], "r");
    if (NULL == fp) {
        printf("Cannot open target-file\n");
        return EXIT_FAILURE;
    }
    char c;
    int len = 0;
    while (!feof(fp)) {
        c = fgetc(fp);
        msg[len++] = c;
    }
    // int request_count = 0; // this value is set for count pipelines
    // for (int i = 0; i < len - 1; i++)
    // {
    // 	if (strncmp(msg + i, "\r\n\r\n", 4) == 0)
    // 	{
    // 		request_count++;
    // 		i += 3;
    // 	}
    // }
    // printf("%d\n", request_count);
    int bytes_received;
    fprintf(stdout, "Sending:\n%s", msg);

    send(sock, msg, len, 0);
    printf("\n");
    fprintf(stdout, "Received:\n");
    while (1) // 这里为什么要死循环，这是因为似乎一次recv会接受到服务器端多次的send
    {
        if ((bytes_received = recv(sock, buf, BUF_SIZE, 0)) > 1) {
            buf[bytes_received] = '\0';
            // fprintf(stdout, "Received:\n%s", buf);
            //  printf("%d\n", bytes_received);
            for (int i = 0; i < bytes_received; ++i) {
                putchar(buf[i]);
            }
            // printf("%d\n", bytes_received);
        } else
            break;
    }
    printf("\nrecv end\n");
    free(buf);
    freeaddrinfo(servinfo);
    close(sock);
    fclose(fp);
    fp = NULL;
    return EXIT_SUCCESS;
}
