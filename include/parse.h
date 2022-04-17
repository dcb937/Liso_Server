#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SUCCESS 0

// Header field
typedef struct
{
    char header_name[4096];
    char header_value[4096];
} Request_header;

// HTTP Request Header
typedef struct
{
    char http_version[50];
    char http_method[50];
    char http_uri[4096];
    Request_header* headers;
    int header_count;
} Request;

Request* parse(char* buffer, int size, int socketFd);

// functions decalred in parser.y
int yyparse();
void yyrestart(FILE* input_file);
void set_parsing_options(char* buf, size_t i, Request* request);

extern const char RESPONSE_200[];
extern const char RESPONSE_302[];
extern const char RESPONSE_400[];
extern const char RESPONSE_404[];
extern const char RESPONSE_501[];
extern const char RESPONSE_505[];
extern const char RESPONSE_500[];
extern int close_socket(int sock);
extern FILE* log_file;
extern int CONTENT_SIZE;
extern int MSG_SIZE;
extern char* content;
extern char* msg;
extern int pipeline_buf_pos;
extern int pipeline_isend;
extern struct sockaddr_in cli_addr;

int response_get(int sock, int client_sock, Request* request);
int cgi_get(int sock, int client_sock, Request* request);
void write_ip2log();