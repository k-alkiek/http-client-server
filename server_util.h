//
// Created by khaled on 11/2/19.
//

#ifndef SOCKET_PROG_SERVER_UTIL_H
#define SOCKET_PROG_SERVER_UTIL_H

struct http_request {
    enum HTTPMethod {GET, POST} method;
    enum FileType {NONE, HTML, TXT, JPG, PNG, GIF} filetype;
    enum Connection {CLOSED, KEEP_ALIVE} connection;
    char* path;

    // Specific to POST requests
    int content_length;
    char* body;
};

void free_http_request(http_request *);
const char *get_http_request_connection(http_request::Connection connection);
const char *get_http_request_method(http_request::HTTPMethod method);
const char *get_http_request_filetype(http_request::FileType fileType);

void print_client(struct sockaddr *);
struct http_request *parse_request(char buffer[], int buffer_size);
void handle_sigchld(int sig);
int read_file(char* path, char** file_buffer);

void load_response_success(char *buffer);
int load_response_not_found(char *buffer);
int load_response_file(char *buffer, char *file_buffer, int file_size, http_request::FileType type, char* status);
void load_content_type(char *buffer, http_request::FileType type);

#endif //SOCKET_PROG_SERVER_UTIL_H
