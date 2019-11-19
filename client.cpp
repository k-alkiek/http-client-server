/*
** showip.c -- show IP addresses for a host given on the command line
*/
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <zconf.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include "client_utils.h"
#include "utils.h"

#define COMMANDS_FILE_PATH "./client_requests"
#define COMMAND_MAX_LENGTH 256

#define BUFFER_SIZE 1024*1024

using namespace std;

int main(int argc, char *argv[])
{
    char *server_ip = "localhost";
    char *server_port = "80";

    if (argc == 2) {
            server_ip = argv[1];
    }
    else if (argc == 3) {
        server_ip = argv[1];
        server_port = argv[2];
    }
    else if (argc > 3){
        fprintf(stderr,"usage: [client server_ip] [port_number]\n");
        return 1;
    }

    char recv_buffer[BUFFER_SIZE];
    char send_buffer[BUFFER_SIZE];
    int server_socket_fd;

    /* Get address info */
    struct addrinfo hints, *res, *p;
    int status;
    char ipstr[INET6_ADDRSTRLEN];
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // AF_INET or AF_INET6 to force version
    hints.ai_socktype = SOCK_STREAM;
    if ((status = getaddrinfo(server_ip, server_port, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 1;
    }

    /* loop on address info and attempt connection */
    for(p = res;p != NULL; p = p->ai_next) {
        server_socket_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (server_socket_fd == -1) {
            perror("socket");
            continue;
        }

        if (connect(server_socket_fd, p->ai_addr, p->ai_addrlen) == -1) {
            close(server_socket_fd);
            perror("connect");
            continue;
        }
        else {
            break;
        }
    }

    if (p == NULL) {
        fprintf(stderr, "Connection failed");
        return 2;
    }

    printf("Connected to server\n");
    freeaddrinfo(res); // free the linked list

    /* read commands file */
    vector<pair<string, string>> commands = read_commands(COMMANDS_FILE_PATH);


    for(auto it = commands.begin(); it != commands.end(); ++it) {
        bzero(send_buffer, BUFFER_SIZE);
        bzero(recv_buffer, BUFFER_SIZE);
        int buffer_actual_size = 0, read_bytes = 0;
        int received_bytes, sent_bytes;
        if (it->first.compare("GET") == 0) {    // GET request
            buffer_actual_size += load_get_request(send_buffer, it->second.c_str());

            sent_bytes = send(server_socket_fd, (const void *) send_buffer, buffer_actual_size, 0);
            printf("\n-----------------------------\nsent %d/%d bytes:\n%s\n",sent_bytes, buffer_actual_size, send_buffer);

            bool request_received = false, headers_received = false;
            unordered_map<string, string> headers;
            vector<pair<char*, int>> body_vector;
            int remaining_content = 0, content_length = 0, headers_length = 0;
            while(!headers_received) {
                received_bytes = recv(server_socket_fd, recv_buffer, BUFFER_SIZE - 1, 0);
                printf("received %d bytes\n", received_bytes);
                char *p = strstr(recv_buffer, "\r\n\r\n");
                if (p != NULL) {
                    headers_received = true;
                    headers_length = extract_headers(recv_buffer, &headers);
                    remaining_content = content_length = stoi(headers.find("Content-Length")->second);
                    p += 4;
                    int body_bytes = received_bytes - headers_length;
                    remaining_content -= body_bytes;
                    char *body_frag = (char*) malloc(body_bytes);
                    memcpy(body_frag, p, body_bytes);
                    body_vector.push_back(make_pair(body_frag, body_bytes));
                }
            }
            while (remaining_content > 0) {
                received_bytes = recv(server_socket_fd, recv_buffer, BUFFER_SIZE - 1, 0);
                printf("received %d bytes\n", received_bytes);

                if (remaining_content < received_bytes) {      // last receive might contain another request;
                    char *body_frag = recv_buffer;
                    body_frag = (char*) malloc(remaining_content);
                    memcpy(body_frag, recv_buffer, remaining_content);
                    body_vector.push_back(make_pair(body_frag, remaining_content));
                    remaining_content = 0;
                }
                else {
                    remaining_content -= received_bytes;
                    char *body_frag = recv_buffer;
                    body_frag = (char*) malloc(received_bytes);
                    memcpy(body_frag, recv_buffer, received_bytes);
                    body_vector.push_back(make_pair(body_frag, received_bytes));
                }
            }

            char* full_body = construct_body(body_vector, content_length);
            handle_get_response_body(full_body, content_length, it->second);

            if (headers.find("Content-Type")->second.substr(0, 4).compare("text") == 0) {
                log_body(full_body, content_length);
            }
            free(full_body);
//            read_bytes += handle_get_response(recv_buffer, it->second, read_bytes);
        }
        else {  // POST request
//            buffer_actual_size += load_post_request(send_buffer, it->second.c_str());
            char *request_buffer, *file_buffer;
            string actual_path = get_actual_path(it->second.c_str(), CLIENT_ROOT);
            int content_length = read_file(actual_path.c_str(), &file_buffer);
            int req_headers_length =  load_post_request_headers(send_buffer, it->second, content_length);

            int request_length = req_headers_length + content_length;
            request_buffer = (char *)malloc(request_length);
            memcpy(request_buffer, send_buffer, req_headers_length);
            memcpy(request_buffer+req_headers_length, file_buffer, content_length);

            printf("\n-----------------------------\n");
            sent_bytes = send_all(server_socket_fd, (const void *) request_buffer, request_length);
            printf("sent %d/%d bytes:\n",sent_bytes, request_length);
//            printf("body: \n%s\n", request_buffer);
            printf("body: \n");
            print_chars(request_buffer, req_headers_length);
//            printf("sent:\n%s\n", send_buffer);

            received_bytes = recv(server_socket_fd, recv_buffer, BUFFER_SIZE - 1, 0);
            read_bytes = 0;
            printf("received %d bytes\n", received_bytes);
            printf("received: %s ", recv_buffer);
        }

    }

    printf("\nFinished all requests\n");
//    recv(server_socket_fd, recv_buffer, BUFFER_SIZE - 1, 0);
    close(server_socket_fd);

    return 0;
}