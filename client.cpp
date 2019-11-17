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

#define COMMANDS_FILE_PATH "./client_requests"
# define COMMAND_MAX_LENGTH 256

#define BUFFER_SIZE 1048576

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
        int buffer_actual_size = 0, read_bytes = 0;
        int rec_bytes;
        if (it->first.compare("GET") == 0) {    // GET request
            buffer_actual_size += load_get_request(send_buffer, it->second.c_str());

            send(server_socket_fd, (const void *) send_buffer, buffer_actual_size, 0);
            printf("sent:\n%s\n", send_buffer);

            rec_bytes = recv(server_socket_fd, recv_buffer, BUFFER_SIZE - 1, 0);
            read_bytes = 0;
            read_bytes = handle_get_response(recv_buffer, it->second, read_bytes);
            printf("received: %d read: %d", rec_bytes, read_bytes);
        }
        else {  // POST request
            buffer_actual_size += load_post_request(send_buffer, it->second.c_str());
            send(server_socket_fd, (const void *) send_buffer, buffer_actual_size, 0);
            printf("sent:\n%s\n", send_buffer);

            rec_bytes = recv(server_socket_fd, recv_buffer, BUFFER_SIZE - 1, 0);
            read_bytes = 0;
            printf("received: %d read: %d", rec_bytes, read_bytes);
        }

    }

    return 0;
}