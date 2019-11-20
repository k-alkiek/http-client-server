#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <iostream>
#include <cstring>
#include <zconf.h>
#include <libnet.h>
#include <unordered_map>
#include <sstream>
#include <vector>
#include <map>
#include <fstream>

#include "server_utils.h"
#include "utils.h"


#define BACKLOG 10
#define BUFFER_SIZE 1024*1024

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr,"usage: server port\n");
        return 1;
    }

    char *port = argv[1];

    int yes = 1;
    int socket_fd, client_fd;
    struct sockaddr_storage client_sockaddr;
    socklen_t sin_size = sizeof(struct sockaddr_storage);

    /* Initialize address info */
    int status;
    struct addrinfo hints;
    struct addrinfo *servinfo, *res; // will point to the results
    memset(&hints, 0, sizeof hints); // make sure the struct is empty

    hints.ai_family = AF_UNSPEC; // don't care IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
    hints.ai_flags = AI_PASSIVE; // fill in my IP for me
    if ((status = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(1);
    }

    /* Create and bind socket*/

    // Loop on results and bind on the first socket possible
    int bind_status = -1;
    for (res = servinfo; res != NULL && bind_status == -1; res = res->ai_next) {
        socket_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (socket_fd == -1) {
            continue;
        }

        if (setsockopt(socket_fd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        bind_status = bind(socket_fd, res->ai_addr, res->ai_addrlen);
        if (bind_status == -1) {
            close(socket_fd);
            perror("bind");
            continue;
        }
    }

    // Could not bind on any of the results
    if (res == NULL) {
        perror("No port to bind");
        exit(1);
    }

    freeaddrinfo(servinfo); // free the linked-list

    /* Start listening */

    if (listen(socket_fd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    // Register sigchld handler and reap any previous zombie child processes
    struct sigaction sa;
    sa.sa_handler = &handle_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sa, 0) == -1) {
        perror(0);
        exit(1);
    }

    printf("server: Listening to port %s. Waiting for connections...\n", port);

    /* Main loop */

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
    while (true) {
        client_fd = accept(socket_fd, (struct sockaddr *) &client_sockaddr, &sin_size);
        if (client_fd == -1) {
            perror("accept");
            continue;
        }
        print_client((struct sockaddr *) &client_sockaddr);

        if (fork() == 0) { // This is the child process

            // Initialize connection related variables
            bool timeout = false;
            char receive_buffer[BUFFER_SIZE];
            int receive_buffer_length = 0;


            vector<char> headers_buffer, body_buffer, leftover, send_buffer;

            while(!timeout) {
                // Request related variables
                cout << "____________________________________________" << endl;
                int headers_length = 0, content_length = 0;
                string method, path, connection, content_type;

                headers_buffer.clear();
                send_buffer.clear();
                body_buffer.clear();

                // Initialize headers_buffer from leftover
                if(!leftover.empty()) {
                    extract_headers_from_leftover(&headers_buffer, &leftover);
                    cout << "extracted some headers: " << headers_buffer.size() << "bytes" << endl;
                }

                // If headers are not complete, continue receiving
                while(!headers_complete(&headers_buffer)) {
                    receive_buffer_length = recv(client_fd, receive_buffer, BUFFER_SIZE, 0);
                    if (receive_buffer_length == 0 ) {
                        cout << "*** Connection closed from client ***" << endl;
                        close(client_fd);
                        exit(1);
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

                // Prepare response
                struct Request *request = create_request(headers_map);
                struct Response *response;
                char *file_buffer = NULL;

                if (request->method.compare("GET") == 0) {
                    int file_size = read_file(request->path, &file_buffer);

                    if (file_size == -1) {
                        file_size = read_file("/not_found.html", &file_buffer);
                        response = create_get_response("404 Not Found", request->connection, "/not_found.html", file_buffer, file_size);
                    }
                    else {
                        response = create_get_response("200 OK", request->connection, request->path, file_buffer, file_size);
                    }
                }
                else if (request->method.compare("POST") == 0) {
                    int remaining_length = request->content_length;
                    remaining_length -= extract_body_from_leftover(&body_buffer, &leftover, remaining_length);
                    while (remaining_length > 0) {
                        receive_buffer_length = recv(client_fd, receive_buffer, BUFFER_SIZE, 0);
                        remaining_length -= append_body(&body_buffer, receive_buffer, receive_buffer_length, &leftover, remaining_length);
                    }

                    write_file(&body_buffer, request->path);
                    response = create_post_response("200 OK", request->connection);
                }

                // Send response
                populate_send_buffer(&send_buffer, request->method, *response);
                send_all(client_fd, &send_buffer);

                delete file_buffer;
                delete request;
//                delete response;

                // Persistent connection
                bool enable_persistent_connection = false;
                if (enable_persistent_connection) {
                    double wait_time = wait_time_heuristic(BACKLOG);
                    cout << "***WAIT FOR " << wait_time << " seconds" << "***" << endl;
                    int status = persist_connection(client_fd, wait_time);
                    if (status == -1) {
                        perror("select()");
                    }
                    else if (status == 0) {
                        cout << "***TIMEOUT***" << endl;
                        timeout = true;
                    }
                }
            }

            cout << "***Closing connection***" << endl;
            close(client_fd);
            exit(0);
        }
        else { // This is the parent process

        }

    }
#pragma clang diagnostic pop

    return 0;
}


