//
// Created by khaled on 11/2/19.
//

#ifndef SOCKET_PROG_SERVER_UTIL_H
#define SOCKET_PROG_SERVER_UTIL_H

struct http_request {
    enum Method {GET, POST} method;
    enum FileType {NONE, HTML, TXT, JPG} filetype;
    char* path;
    char* body;
};
const char *get_http_request_method(http_request::Method method);
const char *get_http_request_filetype(http_request::FileType fileType);

void print_client(struct sockaddr *);
struct http_request *parse_request(char buffer[], int buffer_size);
void handle_sigchld(int sig);

#endif //SOCKET_PROG_SERVER_UTIL_H
