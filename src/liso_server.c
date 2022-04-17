/******************************************************************************
 * echo_server.c                                                               *
 *                                                                             *
 * Description: This file contains the C source code for an echo server.  The  *
 *              server runs on a hard-coded port and simply write back anything*
 *              sent to it by connected clients.  It does not support          *
 *              concurrent clients.                                            *
 *                                                                             *
 * Authors: Athula Balachandran <abalacha@cs.cmu.edu>,                         *
 *          Wolf Richter <wolf@cs.cmu.edu>                                     *
 *                                                                             *
 *******************************************************************************/

#include "parse.h"
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#define ECHO_PORT 9999
#define BUF_SIZE 16384

// char *http_method[] = {"GET", "OPTIONS", "HEAD", "POST", "PUT", "DELETE",
// "TRACE", "CONNECT"}; char* implemented_method[] = {"GET", "POST", "HEAD"};
const char RESPONSE_200[] = "HTTP/1.1 200 OK\r\n";
const char RESPONSE_302[] = "HTTP/1.1 302 Found\r\n";
const char RESPONSE_400[] = "HTTP/1.1 400 Bad request\r\n\r\n";
const char RESPONSE_404[] = "HTTP/1.1 404 Not Found\r\n\r\n";
const char RESPONSE_501[] = "HTTP/1.1 501 Not Implemented\r\n\r\n";
const char RESPONSE_505[] = "HTTP/1.1 505 HTTP Version not supported\r\n\r\n";
const char RESPONSE_500[] = "HTTP/1.1 500 Internal Server Error\r\n\r\n";

int CONTENT_SIZE = 32768;
int MSG_SIZE = 41000;
char* content;
char* msg;

struct sockaddr_in addr, cli_addr;

FILE* log_file; // apache log file

int close_socket(int sock)
{
    if (close(sock)) {
        fprintf(stderr, "Failed closing socket.\n");
        return 1;
    }
    return 0;
}

void print_parsed_request(Request* request) // for debug
{
    printf("Http Method: %s\n", request->http_method);
    printf("Http Version: %s\n", request->http_version);
    printf("Http Uri: %s\n", request->http_uri);
    for (int index = 0; index < request->header_count; index++) {
        printf("Request Header\n");
        printf("Header name: %s Header Value: %s\n",
               request->headers[index].header_name,
               request->headers[index].header_value);
    }
}

void write_ip2log()
{
    //打印客户端的IP和PORT
    // char cIP[16];
    // memset(cIP, 0x00, sizeof(cIP));
    // inet_ntop(AF_INET, &cli_addr.sin_addr.s_addr, cIP, sizeof(cIP));
    // if (cIP)
    //     printf("IP: %s, PORT: %d\n", cIP, ntohs(cli_addr.sin_port));
    char cIP[16];
    memset(cIP, 0x00, sizeof(cIP));
    inet_ntop(AF_INET, &cli_addr.sin_addr.s_addr, cIP, sizeof(cIP));
    if (!cIP) {
        printf("Fetch IP Error\n");
        return;
    }
    fprintf(log_file, "%s:%d - - ",
            cIP,
            ntohs(cli_addr.sin_port));
}

int main(int argc, char* argv[])
{
    int sock, client_sock;
    ssize_t readret;
    socklen_t cli_size;
    // struct sockaddr_in addr, cli_addr;
    char* buf = malloc(BUF_SIZE);

    log_file = fopen("access_log", "w+");

    fprintf(stdout, "----- Echo Server -----\n");

    /* all networked programs must create a socket */
    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
        fprintf(stderr, "Failed creating socket.\n");
        return EXIT_FAILURE;
    }

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int)); //允许端口复用

    addr.sin_family = AF_INET;
    addr.sin_port = htons(ECHO_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    /* servers bind sockets to ports---notify the OS they accept connections */
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr))) {
        close_socket(sock);
        fprintf(stderr, "Failed binding socket.\n");
        return EXIT_FAILURE;
    }

    if (listen(sock, 5)) {
        close_socket(sock);
        fprintf(stderr, "Error listening on socket.\n");
        return EXIT_FAILURE;
    }

    int maxfd;              //最大的文件描述符
    int maxi;               //有效的文件描述符最大值
    int connfd[FD_SETSIZE]; //有效文件描述符数组
    int nready;
    fd_set tmpfds, rdfds; //要监控的文件描述符集

    //文件描述符集初始化
    FD_ZERO(&tmpfds);
    FD_ZERO(&rdfds);

    //将监听文件描述符加入到监控的读集合中
    FD_SET(sock, &rdfds);

    //初始化有效的文件描述符集，为-1表示可用，该数组不保存lfd
    for (int i = 0; i < FD_SETSIZE; i++) {
        connfd[i] = -1;
    }
    maxfd = sock;
    content = (char*)malloc(CONTENT_SIZE);
    msg = (char*)malloc(MSG_SIZE);
    /* finally, loop waiting for input and then write it back */
    while (1) {

        // select为阻塞函数，若没有变化的文件描述符，就一直阻塞，若有事件发生则解除阻塞，函数返回
        // select的第二个参数tmpfds为输入输出参数，调用select完毕后这个节后中保留的是发生变化的文件描述符
        cli_size = sizeof(cli_addr);
        tmpfds = rdfds;
        nready = select(maxfd + 1, &tmpfds, NULL, NULL, NULL);
        if (nready > 0) { //文件描述符集有变化
            //发生变化的文件描述符有两类，一类是监听类的，一类是用于数据通信的。
            //监听文件描述符有变化，有新的连接到来，则accept新的连接
            if (FD_ISSET(sock, &tmpfds)) {
                if ((client_sock = accept(sock, (struct sockaddr*)&cli_addr, &cli_size)) == -1) {
                    close(sock);
                    fprintf(stderr, "Error accepting connection.\n");
                    return EXIT_FAILURE;
                }

                //先找到位置，然后将新的链接的文件描述符保存到connfd数组中
                int i;
                for (i = 0; i < FD_SETSIZE; i++) {
                    if (connfd[i] == -1) {
                        connfd[i] = client_sock;
                        break;
                    }
                }
                //若连接总数达到的最大值
                if (i == FD_SETSIZE) {
                    close(client_sock);
                    printf("too many clients,i==[%d]\n", i);
                    continue;
                }
                //确保connfd中maxi保存的是最后一个文件描述符的下标
                if (i > maxi) {
                    maxi = i;
                }

                //将新的文件描述符加入到select监控的文件描述符中
                FD_SET(client_sock, &rdfds);
                if (maxfd < client_sock) {
                    maxfd = client_sock;
                }
                //如果没有变化的文件描述符，则无需执行后续代码
                if (--nready <= 0) {
                    continue;
                }
            }
            //下面是通信文件描述符有变化的情况
            //只需要循环connfd数组中有效的文件描述符即可，这样可以减少循环次数
            int i;
            for (i = 0; i <= maxi; i++) {
                int sockfd = connfd[i];
                //数组内的文件描述符如果被释放，有可能变为-1
                if (sockfd == -1) {
                    continue;
                }
                if (FD_ISSET(sockfd, &tmpfds)) {

                    readret = 0;
                    printf("sockfd: %d\n", sockfd);
                    readret = recv(sockfd, buf, BUF_SIZE, 0);
                    if (readret >= 1) {
                        // printf("%s\n", buf);
                        pipeline_buf_pos = 0;
                        pipeline_isend = 0;
                        while (!pipeline_isend) {
                            Request* request = parse(buf, readret, sockfd);
                            // debug
                            if (request != NULL)
                                print_parsed_request(request);
                            printf("\n----------------------------------\n\n");

                            memset(content, 0, CONTENT_SIZE);
                            memset(msg, 0, MSG_SIZE);

                            if (request == NULL) // case: parse fail
                            {
                                fprintf(log_file, "Parse failed.\n");
                                if (send(sockfd, RESPONSE_400, strlen(RESPONSE_400), 0) != strlen(RESPONSE_400)) {
                                    close_socket(sockfd);
                                    fprintf(stderr, "Error sending to client.\n");
                                    return EXIT_FAILURE;
                                }
                            }

                            else if (strcmp(request->http_method, "POST") == 0) {
                                write_ip2log();
                                fprintf(log_file, "\"%s %s %s\" %s %ld\n",
                                        request->http_method, request->http_uri,
                                        request->http_version, "echo_back",
                                        readret);
                                fflush(log_file);
                                if (send(sockfd, buf, readret, 0) != readret) {
                                    close_socket(sockfd);
                                    fprintf(stderr, "Error sending to client.\n");
                                    return EXIT_FAILURE;
                                }
                            }

                            else if (strcmp(request->http_method, "GET") == 0) {
                                int res = response_get(sock, sockfd, request);
                                if (res == EXIT_FAILURE)
                                    return EXIT_FAILURE;
                                else if (res == 2) {
                                    goto xxxxxxxxx;
                                }
                            }

                            else if (strcmp(request->http_method, "HEAD") == 0) {
                                if (strcmp(request->http_version, "HTTP/1.1") != 0) {
                                    write_ip2log();
                                    fprintf(log_file, "\"%s %s %s\" %s %ld\n",
                                            request->http_method, request->http_uri,
                                            request->http_version, "505",
                                            strlen(RESPONSE_505));
                                    fflush(log_file);
                                    if (send(sockfd, RESPONSE_505, strlen(RESPONSE_505), 0) != strlen(RESPONSE_505)) {
                                        close_socket(sockfd);
                                        fprintf(stderr, "Error sending to client.\n");
                                        return EXIT_FAILURE;
                                    }
                                } else {
                                    char tmp[20];
                                    strcpy(tmp, RESPONSE_200);
                                    strcat(tmp, "\r\n");
                                    write_ip2log();
                                    fprintf(log_file, "\"%s %s %s\" %s %ld\n",
                                            request->http_method, request->http_uri,
                                            request->http_version, "200",
                                            strlen(RESPONSE_200) + 2);
                                    fflush(log_file);
                                    if (send(sockfd, tmp, strlen(tmp), 0) != strlen(tmp)) {
                                        close_socket(sockfd);
                                        fprintf(stderr, "Error sending to client.\n");
                                        return EXIT_FAILURE;
                                    }
                                }
                            }

                            else {
                                write_ip2log();
                                fprintf(log_file, "\"%s %s %s\" %s %ld\n",
                                        request->http_method, request->http_uri,
                                        request->http_version, "501",
                                        strlen(RESPONSE_501));
                                fflush(log_file);
                                if (send(sockfd, RESPONSE_501, strlen(RESPONSE_501), 0) != strlen(RESPONSE_501)) {
                                    close_socket(sockfd);
                                    fprintf(stderr, "Error sending to client.\n");
                                    return EXIT_FAILURE;
                                }
                            }
                        }
                        memset(buf, 0, BUF_SIZE);
                    } else if (readret == -1) {
                        close_socket(sockfd);
                        fprintf(stderr, "Error reading from client socket.\n");
                        FD_CLR(sockfd, &rdfds);
                        connfd[i] = -1; //将connfd[0]置为-1，表示位置可用
                    } else {
                    xxxxxxxxx:
                        close_socket(sockfd);
                        FD_CLR(sockfd, &rdfds);
                        connfd[i] = -1; //将connfd[0]置为-1，表示位置可用
                    }
                    if (--nready <= 0) {
                        break; //注意这里是break，而不是continue，应该是从最外层的while继续循环
                    }
                }
            }
        }
    }

    free(msg);
    free(content);
    free(buf);
    close_socket(sock);
    fclose(log_file);
    return EXIT_SUCCESS;
}
