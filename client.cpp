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
#include <queue>
#include "client_utils.h"
#include "utils.h"

#define COMMANDS_FILE_PATH "./client_requests"
#define COMMAND_MAX_LENGTH 256
#define PIPELINE_SIZE 3

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


    // Initialize buffers and variables
    char receive_buffer[BUFFER_SIZE];
    int receive_buffer_length = 0;
    vector<char> headers_buffer, body_buffer, leftover, send_buffer;
    queue<pair<string, string>> pipeline;

    /* read commands file */
    vector<pair<string, string>> commands = read_commands(COMMANDS_FILE_PATH);
    auto command = commands.begin();

    while (command != commands.end()) {
        leftover.clear();
        send_buffer.clear();

        cout << "------------pipelining " << PIPELINE_SIZE << " requests---------------" << endl;
        // Pipeline and enqueue requests
        for (int i = 0; i < PIPELINE_SIZE && command != commands.end(); ++i, ++command) {
            struct Request *request = create_request(command->first, command->second);
            append_send_buffer(&send_buffer, request);
            pipeline.push(*command);
        }

        // Send pipelined requests
        send_all(server_socket_fd, &send_buffer);

        cout << "------------waiting for pipelined requests responses ---------------" << endl;
        // Receive responses
        while (!pipeline.empty()) {
            headers_buffer.clear();
            body_buffer.clear();
            int headers_length = 0, content_length = 0;


            // Initialize headers_buffer from leftover
            if(!leftover.empty()) {
                extract_headers_from_leftover(&headers_buffer, &leftover);
                cout << "extracted some headers: " << headers_buffer.size() << "bytes" << endl;
            }

            // If headers are not complete, continue receiving
            while(!headers_complete(&headers_buffer)) {
                receive_buffer_length = recv(server_socket_fd, receive_buffer, BUFFER_SIZE, 0);
                if (receive_buffer_length == 0 ) {
                    close(server_socket_fd);
                    cout << "*** Connection closed from client ***" << endl;
                }
                cout << "received " << receive_buffer_length << " bytes" << endl;
                append_headers(&headers_buffer, receive_buffer, receive_buffer_length, &leftover);
            }
            remove_headers_leftover(&headers_buffer, &leftover);     // Remove extra bytes from header to leftover
            headers_length = headers_buffer.size();
            cout << "headers received: " << headers_length << " bytes" << endl;
            log_vector(&headers_buffer);

            // Process headers
            map<string, string> headers_map = process_headers(&headers_buffer);

            if (pipeline.front().first.compare("GET") == 0) {
                content_length = stoi(headers_map.find("Content-Length")->second);
                int remaining_length = content_length;

                remaining_length -= extract_body_from_leftover(&body_buffer, &leftover, remaining_length);
                while (remaining_length > 0) {
                    receive_buffer_length = recv(server_socket_fd, receive_buffer, BUFFER_SIZE, 0);
                    remaining_length -= append_body(&body_buffer, receive_buffer, receive_buffer_length, &leftover, remaining_length);
                }

                if (headers_map.find("Content-Type")->second.substr(0, 4).compare("text") == 0)
                    log_vector(&body_buffer);
                write_file(&body_buffer, get_file_name(pipeline.front().second));
            }
//            else if (pipeline.front().first.compare("POST") == 0) {
//
//            }

            pipeline.pop();
        }
    }

    printf("\nFinished all requests\n");
    close(server_socket_fd);

    return 0;
}