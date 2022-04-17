#include "parse.h"
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

char CONTEN_LENGTH[8];
char CONTENT_TYPE[32];
char GATEWAY_INTERFACE[] = "CGI/1.1";
char PATH_INFO[64];
char QUERY_STRING[64];
char REMOTE_ADDR[64];
char REQUEST_METHOD[64];
char REQUEST_URI[64];
char SCRIPT_NAME[64];
char SERVER_PORT[64];
char SERVER_PROTOCOL[] = "HTTP/1.1";
char SERVER_SOFTWARE[] = "Liso/1.0";
char HTTP_ACCEPT[156];
char HTTP_REFERER[64];
char HTTP_ACCEPT_ENCODING[64];
char HTTP_ACCEPT_LANGUAGE[64];
char HTTP_ACCEPT_CHARSET[64];
char HTTP_HOST[64];
char HTTP_COOKIE[64];
char HTTP_USER_AGENT[256];
char HTTP_CONNECTION[64];

char username[64];
char passwd[64];

int cgi_get(int sock, int client_sock, Request* request)
{
    int pos;
    for (int i = 0; i < strlen(request->http_uri); i++) {
        if (request->http_uri[i] == '?') {
            pos = i;
            break;
        }
    }
    char* p = strstr(request->http_uri + pos, "username") + strlen("username") + 1;
    char* q = strstr(request->http_uri + pos, "passwd") + strlen("passwd") + 1;
    if (p == NULL || q == NULL) {
        if (send(client_sock, RESPONSE_500, strlen(RESPONSE_500), 0) != strlen(RESPONSE_505)) {
            close_socket(client_sock);
            fprintf(stderr, "Error sending to client.\n");
            return EXIT_FAILURE;
        }
    }
    int offset = 0;
    int len_u = 0;
    int len_p = 0;
    while (!(p[offset] == '&' || p[offset] == 0)) {
        username[len_u++] = p[offset++];
    }
    offset = 0;
    while (!(q[offset] == '&' || q[offset] == 0)) {
        passwd[len_p++] = q[offset++];
    }

    strncpy(PATH_INFO, request->http_uri, pos);
    strcpy(QUERY_STRING, request->http_uri + pos + 1);
    char cIP[16];
    memset(cIP, 0x00, sizeof(cIP));
    inet_ntop(AF_INET, &cli_addr.sin_addr.s_addr, cIP, sizeof(cIP));
    if (cIP != NULL)
        sprintf(REMOTE_ADDR, "%s", cIP);
    strcpy(REQUEST_METHOD, "CGI_GET");
    sprintf(SERVER_PORT, "%hd", ntohs(cli_addr.sin_port));

    for (int i = 0; i < request->header_count; ++i) {
        if (strcmp(request->headers[i].header_name, "Accept") == 0) {
            strcpy(HTTP_ACCEPT, request->headers[i].header_value);
        }
        if (strcmp(request->headers[i].header_name, "Referer") == 0) {
            strcpy(HTTP_REFERER, request->headers[i].header_value);
        }
        if (strcmp(request->headers[i].header_name, "Accept-Encoding") == 0) {
            strcpy(HTTP_ACCEPT_ENCODING, request->headers[i].header_value);
        }
        if (strcmp(request->headers[i].header_name, "Accept-Language") == 0) {
            strcpy(HTTP_ACCEPT_LANGUAGE, request->headers[i].header_value);
        }
        if (strcmp(request->headers[i].header_name, "Accept-Charset") == 0) {
            strcpy(HTTP_ACCEPT_CHARSET, request->headers[i].header_value);
        }
        if (strcmp(request->headers[i].header_name, "Host") == 0) {
            strcpy(HTTP_HOST, request->headers[i].header_value);
        }
        if (strcmp(request->headers[i].header_name, "Cookie") == 0) {
            strcpy(HTTP_COOKIE, request->headers[i].header_value);
        }
        if (strcmp(request->headers[i].header_name, "User-Agent") == 0) {
            strcpy(HTTP_USER_AGENT, request->headers[i].header_value);
        }
        if (strcmp(request->headers[i].header_name, "Connection") == 0) {
            strcpy(HTTP_CONNECTION, request->headers[i].header_value);
        }
    }

    printf("CONTEN_LENGTH: %s\n CONTENT_TYPE: %s\n GATEWAY_INTERFACE : %s\n PATH_INFO: %s\n QUERY_STRING: %s\n REMOTE_ADDR: %s\n REQUEST_METHOD: %s\n REQUEST_URI: %s\n SCRIPT_NAME: %s\n SERVER_PORT: %s\n SERVER_PROTOCOL : %s\n SERVER_SOFTWARE : %s\n HTTP_ACCEPT: %s\n HTTP_REFERER: %s\n HTTP_ACCEPT_ENCODING: %s\n HTTP_ACCEPT_LANGUAGE: %s\n HTTP_ACCEPT_CHARSET: %s\n HTTP_HOST: %s\n HTTP_COOKIE: %s\n HTTP_USER_AGENT: %s\n HTTP_CONNECTION: %s\n\n", CONTEN_LENGTH, CONTENT_TYPE, GATEWAY_INTERFACE, PATH_INFO, QUERY_STRING, REMOTE_ADDR, REQUEST_METHOD, REQUEST_URI, SCRIPT_NAME, SERVER_PORT, SERVER_PROTOCOL, SERVER_SOFTWARE, HTTP_ACCEPT, HTTP_REFERER, HTTP_ACCEPT_ENCODING, HTTP_ACCEPT_LANGUAGE, HTTP_ACCEPT_CHARSET, HTTP_HOST, HTTP_COOKIE, HTTP_USER_AGENT, HTTP_CONNECTION);

    FILE* fp = fopen("static_site/welcome.html", "w+");
    char display[128];
    fprintf(fp, "<h1>Your username is %s<h1>\n<h1>Your password is %s<h1>", username, passwd);
    fclose(fp);

    strcat(msg, RESPONSE_302);
    strcat(msg, "Server: liso-dcb\r\n");
    strcat(msg, "Content-Length: 0");
    strcat(msg, "Connection: keep-alive\r\n");
    strcat(msg, "Location: welcome.html\r\n");
    strcat(msg, "\r\n");
    long msg_len = strlen(msg);

    for (int i = 0; i < strlen(display); ++i) {
        msg[msg_len + i] = content[i];
    }
    msg_len = msg_len + strlen(display);
    if (send(client_sock, msg, msg_len, 0) != msg_len) {
        close_socket(client_sock);
        fprintf(stderr, "Error sending to client.\n");
        return EXIT_FAILURE;
    }
    write_ip2log();
    fprintf(log_file, "\"%s %s %s\" %s %ld\n", request->http_method, request->http_uri, request->http_version, "302", msg_len);
    fflush(log_file);
    return 0;
}