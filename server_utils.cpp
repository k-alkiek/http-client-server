//
// Created by khaled on 11/2/19.
//

#include <netinet/in.h>
#include <libnet.h>
#include <wait.h>
#include <string>
#include <unordered_map>
#include <sstream>
#include <iostream>
#include <fstream>
#include <algorithm>
#include "utils.h"
#include "server_utils.h"



using namespace std;

void free_http_request(http_request *request) {
    free(request->path);
    free(request->body);
    free(request);
}

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
        case http_request::FileType::GIF: return "GIF";
    }
}

void print_client(struct sockaddr * client_sockaddr) {
    // Log client address and port
    char client_ip4[INET6_ADDRSTRLEN];
    char client_ip6[INET6_ADDRSTRLEN];

    if (client_sockaddr->sa_family == AF_INET) { // IPv4
        struct sockaddr_in *sock_addr = (struct sockaddr_in *)client_sockaddr;
        inet_ntop(AF_INET, &sock_addr->sin_addr, client_ip4, INET_ADDRSTRLEN);
        printf("\nClient connected from IPv4: %s port %d\n", client_ip4, ntohs(sock_addr->sin_port));
    } else { // IPv6
        struct sockaddr_in6 *sock_addr = (struct sockaddr_in6 *)client_sockaddr;
        inet_ntop(AF_INET6, &sock_addr->sin6_addr, client_ip6, INET6_ADDRSTRLEN);
        printf("\nClient connected from IPv6: %s port %d\n", client_ip6, ntohs(sock_addr->sin6_port));
    }
}

int extract_headers(char* buffer, unordered_map<string, string> *headers) {
    char *headers_end = strstr(buffer, "\r\n\r\n");
    *headers_end = '\0';
    int headers_length = strlen(buffer)+4;
    char* token = strtok(buffer, "\r\n");
    headers->insert(make_pair("Request", string(token)));
    token = strtok(NULL, "\r\n");

    while (token != NULL) {
        string token_str(token);
        int sep_pos = token_str.find(':');
        string field_name = token_str.substr(0, sep_pos);
        string field_value = token_str.substr(sep_pos+1);
        trim((string &) field_value);
        headers->insert(make_pair(field_name, field_value));
        token = strtok(NULL, "\r\n");
    }
    return headers_length;
}

/*
 * Extracts info from the request string to a struct http_request
 */
int parse_request(char buffer[], int buffer_size, struct http_request **request_ptr) {
    struct http_request *request = (struct http_request *)malloc(sizeof(http_request));

    int headers_length = 0;
    unordered_map<string, string> headers;
    headers_length += extract_headers(buffer, &headers);

    string request_line = headers.find("Request")->second;
    stringstream stringstream1(request_line);
    string method, path;
    stringstream1 >>  method >> path;

    int content_length = 0;

    request->connection = http_request::Connection::CLOSED;
    if (headers.find("Connection") != headers.end() && headers.find("Connection")->second.compare("keep-alive")==0) {
        request->connection = http_request::Connection::KEEP_ALIVE;
    }
    request->path = (char *)(malloc(path.length()+1));
    strcpy(request->path, path.c_str());

    if(method.compare("GET") == 0) {
        request->method = http_request::HTTPMethod::GET;
    }
    else {
        request->method = http_request::HTTPMethod::POST;
        content_length = stoi(headers.find("Content-Length")->second);
        request->body = (char *)malloc(buffer_size - headers_length);
        memcpy(request->body, buffer+headers_length, buffer_size - headers_length);
    }

    string ext = get_file_extension(path);
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return std::tolower(c); });

    if (ext.compare("html") == 0) {
        request->filetype = http_request::FileType::HTML;
    }
    else if (ext.compare("txt") == 0) {
        request->filetype = http_request::FileType::TXT;
    }
    else if (ext.compare("jpg") == 0) {
        request->filetype = http_request::FileType::JPG;
    }
    else if (ext.compare("png") == 0) {
        request->filetype = http_request::FileType::PNG;
    }
    else if (ext.compare("gif") == 0) {
        request->filetype = http_request::FileType::GIF;
    }
    else {
        request->filetype = http_request::FileType::NONE;
    }

    request->content_length = content_length;

    *request_ptr = request;
    return headers_length + content_length;
}

void handle_sigchld(int sig) {
    int saved_errno = errno;
    while (waitpid((pid_t)(-1), 0, WNOHANG) > 0) {}
    errno = saved_errno;
}

int read_file(const char* path, char** file_buffer) {
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

//        printf("\n %s\n length=%d\n", buffer, length);
        *file_buffer = buffer;
        return length;
    }
    else {
        return -1;
    }
}

int load_response_success(char *buffer) {
    strcat(buffer, "HTTP/1.1 200 OK\r\n\r\n");
    return strlen("HTTP/1.1 200 OK\r\n\r\n");
}

int load_response_not_found(char *buffer) {
    char *file_buffer;
    int file_size;
    string actual_path = get_actual_path("/not_found.html", SERVER_ROOT);
    file_size = read_file(actual_path.c_str(), &file_buffer);

    return load_response_file(buffer, file_buffer, file_size, http_request::FileType::HTML, "404 Not Found");
}

// Returns total size of response
int load_response_file(char *buffer, char* file_buffer, int file_size, http_request::FileType type, char* status) {
    int header_length, body_length;

    strcat(buffer, "HTTP/1.1 ");
    strcat(buffer, status);
    strcat(buffer, "\r\n");
    char content_length[100];
    sprintf(content_length, "%d", file_size);
    strcat(buffer, "Content-Length: ");
    strcat(buffer, content_length);
    strcat(buffer, "\r\n");
    strcat(buffer, "Connection: keep-alive\r\n");
    strcat(buffer, "Keep-Alive: timeout=5, max=100\r\n");
    load_content_type(buffer, type);
    strcat(buffer, "\r\n");
    header_length = strlen(buffer);

    memcpy(((char*)buffer) + strlen(buffer), file_buffer, file_size);
    free(file_buffer);

    body_length = file_size;
    printf("headers size: %d, body size: %d\n", header_length, file_size);
    return header_length + body_length;
}

int load_response_file_headers(char *buffer, int file_size, http_request::FileType type, char* status) {
    int header_length;

    strcat(buffer, "HTTP/1.1 ");
    strcat(buffer, status);
    strcat(buffer, "\r\n");
    char content_length[100];
    sprintf(content_length, "%d", file_size);
    strcat(buffer, "Content-Length: ");
    strcat(buffer, content_length);
    strcat(buffer, "\r\n");
    strcat(buffer, "Connection: keep-alive\r\n");
    strcat(buffer, "Keep-Alive: timeout=5, max=100\r\n");
    load_content_type(buffer, type);
    strcat(buffer, "\r\n");
    header_length = strlen(buffer);


    return header_length;
}

void load_content_type(char *buffer, http_request::FileType type) {
    strcat(buffer, "Content-Type: ");
    switch(type) {
        case http_request::FileType::HTML:
            strcat(buffer, "text/html");
            break;
        case http_request::FileType::JPG:
            strcat(buffer, "image/jpg");
            break;
        case http_request::FileType::PNG:
            strcat(buffer, "image/png");
            break;
        case http_request::FileType::GIF:
            strcat(buffer, "image/gif");
            break;
        case http_request::FileType::TXT:
        case http_request::FileType::NONE:
            strcat(buffer, "text/plain");
            break;
    }
    strcat(buffer, "\r\n");
}

std::string exec(const char* cmd) {
    FILE* pipe = popen(cmd, "r");
    if (!pipe) return "ERROR";
    char buffer[128];
    std::string result = "";
    while(!feof(pipe)) {
        if(fgets(buffer, 128, pipe) != NULL)
            result += buffer;
    }
    pclose(pipe);
    return result;
}