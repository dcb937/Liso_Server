#include "parse.h"

extern FILE* yyin;

int pipeline_buf_pos;
int pipeline_isend;

/**
 * Given a char buffer returns the parsed request headers
 */
Request* parse(char* buffer, int size, int socketFd)
{
    // Differant states in the state machine
    enum {
        STATE_START = 0,
        STATE_CR,
        STATE_CRLF,
        STATE_CRLFCR,
        STATE_CRLFCRLF
    };
    // to implement pipeline, so add
    buffer += pipeline_buf_pos;
    // size -= pipeline_buf_pos;
    // add end

    int i = 0, state;
    size_t offset = 0;
    char ch;
    char buf[8192];
    memset(buf, 0, 8192);

    state = STATE_START;
    while (state != STATE_CRLFCRLF) {
        char expected = 0;

        if (i == size)
            break;

        ch = buffer[i++];
        buf[offset++] = ch;

        switch (state) {
        case STATE_START:
        case STATE_CRLF:
            expected = '\r';
            break;
        case STATE_CR:
        case STATE_CRLFCR:
            expected = '\n';
            break;
        default:
            state = STATE_START;
            continue;
        }

        if (ch == expected)
            state++;
        else
            state = STATE_START;

    } // 包头以CRLFCRLF结尾，这里相当于是获取了包头 从buffer提取到buf

    // to implement pipeline, so add
    pipeline_buf_pos += offset;
    // printf("pipeline_buf_pos: %d  size: %d\n", pipeline_buf_pos, size);
    if (pipeline_buf_pos == size - 3 || pipeline_buf_pos == size - 1 || pipeline_buf_pos >= size) // 这里为什么要有-3和-1，一般的除了pipeline以外的samples都是以\r\n\r\n\r\n结束，而pipeline却是以\r\n\r\n结束
        pipeline_isend = 1;
    // printf("%d", pipeline_isend);
    //  add end

    // Valid End State
    if (state == STATE_CRLFCRLF) {
        Request* request = (Request*)malloc(sizeof(Request));
        request->header_count = 0;
        // TODO You will need to handle resizing this in parser.y
        request->headers = (Request_header*)malloc(sizeof(Request_header));
        set_parsing_options(buf, i, request);

        if (yyparse() == SUCCESS) {
            return request;
        } else {
            yyrestart(yyin);
            free(request->headers);
            free(request);
        }
    }
    // TODO Handle Malformed Requests
    printf("Parsing Failed\n");
    return NULL;
}
