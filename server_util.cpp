//
// Created by khaled on 11/2/19.
//

#include <netinet/in.h>
#include <libnet.h>
#include <wait.h>
#include "server_util.h"

const char *get_http_request_connection(http_request::Connection connection) {
    switch(connection) {
        case http_request::Connection::CLOSED: return "closed";
        case http_request::Connection::KEEP_ALIVE: return "keep-alive";
    }
}

const char *get_http_request_method(http_request::HTTPMethod method) {
    switch(method) {
        case http_request::HTTPMethod::POST: return "POST";
        case http_request::HTTPMethod::GET: return "GET";
    }
}

const char *get_http_request_filetype(http_request::FileType fileType) {
    switch(fileType) {
        case http_request::FileType::NONE: return "NONE";
        case http_request::FileType::HTML: return "HTML";
        case http_request::FileType::TXT: return "TXT";
        case http_request::FileType::JPG: return "JPG";
        case http_request::FileType::PNG: return "PNG";
    }
}

void print_client(struct sockaddr * client_sockaddr) {
    // Log client address and port
    char client_ip4[INET6_ADDRSTRLEN];
    char client_ip6[INET6_ADDRSTRLEN];

    if (client_sockaddr->sa_family == AF_INET) { // IPv4
        struct sockaddr_in *sock_addr = (struct sockaddr_in *)client_sockaddr;
        inet_ntop(AF_INET, &sock_addr->sin_addr, client_ip4, INET_ADDRSTRLEN);
        printf("Client connected from IPv4: %s port %d\n", client_ip4, ntohs(sock_addr->sin_port));
    } else { // IPv6
        struct sockaddr_in6 *sock_addr = (struct sockaddr_in6 *)client_sockaddr;
        inet_ntop(AF_INET6, &sock_addr->sin6_addr, client_ip6, INET6_ADDRSTRLEN);
        printf("Client connected from IPv6: %s port %d\n", client_ip6, ntohs(sock_addr->sin6_port));
    }
}

/*
 * Extracts info from the request string to a struct http_request
 */
struct http_request *parse_request(char buffer[], int buffer_size) {
//    printf("\n %s \n", buffer);
    struct http_request *request = (struct http_request *)malloc(sizeof(http_request));

    char *req_head, *req_body;
    char *body_delim = "\r\n\r\n";

    // Extracting the body
    req_body = strstr(buffer, "\r\n\r\n")+4;
    request->body = (char *)malloc(strlen(req_body)+1);
    strcpy(request->body, req_body);

    // Extracting method
    if (buffer[0] == 'P') {
        request->method = http_request::HTTPMethod::POST;
    }
    else if (buffer[0] == 'G'){
        request->method = http_request::HTTPMethod::GET;
    }

    // Extracting connection
    request->connection = http_request::Connection::CLOSED;
    char * connection = strstr(buffer, "Connection: ");
    if (connection != NULL) {
        connection += strlen("Connection: ");
        char *tmp = (char *)malloc(strlen(connection)+1);
        strcpy(tmp, connection);
        tmp = strtok(tmp, "\r\n");
        if (!strcmp(tmp, "keep-alive")) {
            request->connection = http_request::Connection::KEEP_ALIVE;
        }
        free(tmp);
    }


    // Extracting content length
    request->content_length = -1;
    char * content_length = strstr(buffer, "Content-Length: ");

    if (content_length != NULL) {
        content_length += strlen("Content-Length: ");

        content_length = strtok(content_length, "\r\n");
        request->content_length = atoi(content_length);
    }

    // Extracting path
    req_head = strtok(buffer, "\r\n");
    req_head = strtok(req_head, " ");
    req_head = strtok(NULL, " ");
    request->path = (char *)malloc(strlen(req_head)+2);
    strcpy(request->path+1, req_head);
    request->path[0] = '.';

    // Extracting file type
    char *ext = strchr(request->path+1, '.');
    if (ext != NULL) {
        char *p = ext;
        for(; *p; ++p) *p = tolower(*p);
        ext++;
        if (!strcmp(ext, "html")) request->filetype = http_request::FileType::HTML;
        else if (!strcmp(ext, "txt")) request->filetype = http_request::FileType::TXT;
        else if (!strcmp(ext, "jpg")) request->filetype = http_request::FileType::JPG;
        else if (!strcmp(ext, "png")) request->filetype = http_request::FileType::PNG;
    }
    else {
        request->filetype = http_request::FileType::NONE;
    }


    return request;
}

void handle_sigchld(int sig) {
    int saved_errno = errno;
    while (waitpid((pid_t)(-1), 0, WNOHANG) > 0) {}
    errno = saved_errno;
}

char *read_file(char* path) {
    char *buffer = 0;
    long length;
    FILE * f = fopen (path, "rb");

    if (f) {
        fseek (f, 0, SEEK_END);
        length = ftell (f);
        fseek (f, 0, SEEK_SET);
        buffer = (char *)malloc(length);
        if (buffer) {
            fread (buffer, 1, length, f);
        }
        fclose (f);

//        printf("\n %s \n", buffer);
        return buffer;
    }
    else {
        return NULL;
    }
}

void load_response_success(char *buffer) {
    strcat(buffer, "HTTP/1.1 200 OK\r\n\r\n");
}

void load_response_not_found(char *buffer) {
    char *file_buffer = read_file("./not_found.html");
    char content_length[100];
    sprintf(content_length, "%d", strlen(file_buffer));
    strcpy(buffer, "HTTP/1.1 404 Not Found\r\n");
    strcat(buffer, "Content-Length: ");
    strcat(buffer, content_length);
    strcat(buffer, "\r\n");
    strcat(buffer, "Content-Type: text/html");
    strcat(buffer, "\r\n\r\n");
    strcat(buffer, file_buffer);

//    strcpy(buffer, "HTTP/1.1 404 Not Found\r\n\r\n");
}

void load_response_text_file(char *buffer, char* file_buffer, http_request::FileType type) {
    char content_length[100];
    sprintf(content_length, "%d", strlen(file_buffer));
    strcpy(buffer, "HTTP/1.1 200 OK\r\n");
    strcat(buffer, "Content-Length: ");
    strcat(buffer, content_length);
    strcat(buffer, "\r\n");
    strcat(buffer, "Content-Type: ");
    strcat(buffer, "text/");
    if (type == http_request::FileType::HTML) {
        strcat(buffer, "html");
    }
    else {
        strcat(buffer, "plain");
    }
    strcat(buffer, "\r\n\r\n");
    strcat(buffer, file_buffer);
}