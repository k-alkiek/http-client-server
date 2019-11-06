//
// Created by khaled on 11/2/19.
//

#include <netinet/in.h>
#include <libnet.h>
#include <wait.h>
#include "server_util.h"

const char *get_http_request_method(http_request::Method method) {
    switch(method) {
        case http_request::Method::POST: return "POST";
        case http_request::Method::GET: return "GET";
    }
}

const char *get_http_request_filetype(http_request::FileType fileType) {
    switch(fileType) {
        case http_request::FileType::NONE: return "NONE";
        case http_request::FileType::HTML: return "HTML";
        case http_request::FileType::TXT: return "TXT";
        case http_request::FileType::JPG: return "JPG";
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
    struct http_request *request = (struct http_request *)malloc(sizeof(http_request));
    char *req_head, *req_body;
    char *body_delim = "\r\n\r\n";

    // Extracting the body
    req_body = strstr(buffer, "\r\n\r\n")+4;
    request->body = (char *)malloc(strlen(req_body)+1);
    strcpy(request->body, req_body);

    // Extracting method
    req_head = strtok(buffer, "\r\n");

    if (req_head[0] == 'P') {
        request->method = http_request::Method::POST;
    }
    else {
        request->method = http_request::Method::GET;
    }

    // Extracting path
    req_head = strtok(req_head, " ");
    req_head = strtok(NULL, " ");
    request->path = (char *)malloc(strlen(req_head)+1);
    strcpy(request->path, req_head);

    // Extracting file type
    char *ext = strchr(request->path, '.');
    if (ext != NULL) {
        char *p = ext;
        for(; *p; ++p) *p = tolower(*p);
        ext++;
        printf("ext=%s\n", ext);
        if (!strcmp(ext, "html")) request->filetype = http_request::FileType::HTML;
        else if (!strcmp(ext, "txt")) request->filetype = http_request::FileType::TXT;
        else if (!strcmp(ext, "jpg")) request->filetype = http_request::FileType::JPG;
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
