#include "parse.h"
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

int response_get(int sock, int client_sock, Request* request)
{
    if (strcmp(request->http_version, "HTTP/1.1") != 0) {
        if (send(client_sock, RESPONSE_505, strlen(RESPONSE_505), 0) != strlen(RESPONSE_505)) {
            close_socket(client_sock);
            fprintf(stderr, "Error sending to client.\n");
            return EXIT_FAILURE;
        }
        write_ip2log();
        fprintf(log_file, "\"%s %s %s\" %s %ld\n", request->http_method, request->http_uri, request->http_version, "505", strlen(RESPONSE_505));
        fflush(log_file);
        return 0;
    }

    int is_short_connection = 0;
    for (int i = 0; i < request->header_count; ++i) {
        if (strcmp(request->headers[i].header_name, "Accept") == 0) {
            if (strcmp(request->headers[i].header_value, "close") == 0) {
                is_short_connection = 1;
            }
        }
    }

    char file_name[64]; // file path is limited to 64
    memset(file_name, 0, 64);

    for (int i = 0; i < strlen(request->http_uri); ++i) {
        if (request->http_uri[i] == '?')
            return cgi_get(sock, client_sock, request);
    }

    if (strcmp(request->http_uri, "/") == 0) {
        strcpy(file_name, "static_site/index.html");
    } else {
        strcat(file_name, "static_site");
        strcat(file_name, request->http_uri);
    }

    FILE* fp = NULL;
    fp = fopen(file_name, "r");
    if (NULL == fp) {
        if (send(client_sock, RESPONSE_404, strlen(RESPONSE_404), 0) != strlen(RESPONSE_404)) {
            close_socket(client_sock);
            fprintf(stderr, "Error sending to client.\n");
            return EXIT_FAILURE;
        }
        write_ip2log();
        fprintf(log_file, "\"%s %s %s\" %s %ld\n", request->http_method, request->http_uri, request->http_version, "404", strlen(RESPONSE_404));
        fflush(log_file);
        return 0;
    }

    // memset(content, 0, CONTENT_SIZE);
    // memset(content, 0, MSG_SIZE);

    char c;
    long file_len = 0;
    long msg_len = 0;
    while (!feof(fp)) {
        c = fgetc(fp);
        // putchar(c);
        content[file_len++] = c;
    }
    file_len -= 1;
    //本句可有可无，因为传送的时候并不按照字符串(并不是遇\0结束) 而是按长度传输
    //否则在遇到传输二进制文件的时候遇\0会错误的终止传输过程
    char con_len[64];
    sprintf(con_len, "Content-Length: %ld\r\n", file_len);
    strcat(msg, RESPONSE_200);
    strcat(msg, "Server: liso-dcb\r\n");
    // strcat(msg, "Content-Type: text/html; charset=utf-8\r\n");
    strcat(msg, con_len);
    strcat(msg, "Connection: keep-alive\r\n");
    strcat(msg, "\r\n"); // 不应该是\r\n\r\n 因为有正文，只需要一个\r\n
    msg_len = strlen(msg);
    // strncat(msg, content, file_len);错误，不可使用strncat，因为number>strlen(source)时并不会拼接number个，而是拼接strlen(source)个
    for (int i = 0; i < file_len; ++i) {
        msg[msg_len + i] = content[i];
    }
    msg_len = msg_len + file_len;
    // printf("%p\n", content);

    // for (int i = 0; i < msg_len; ++i)
    // {
    //   putchar(msg[i]); // 这里会出现服务端显示的数据比客户端少，因为服务的遇到\n才会在终端回显，所以需要加上putchar('\n);
    // }
    // putchar('\n');

    if (send(client_sock, msg, msg_len, 0) != msg_len) {
        close_socket(client_sock);
        fprintf(stderr, "Error sending to client.\n");
        return EXIT_FAILURE;
    }
    write_ip2log();
    fprintf(log_file, "\"%s %s %s\" %s %ld\n", request->http_method, request->http_uri, request->http_version, "200", msg_len);
    fflush(log_file);

    if (is_short_connection) {
        return 2;
    }
    return 0;
}