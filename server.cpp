#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <iostream>
#include <cstring>
#include <zconf.h>
#include <libnet.h>
#include <unordered_map>
#include <sstream>

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

    char buffer[BUFFER_SIZE];
    char send_buffer[BUFFER_SIZE];
    int received_bytes = -1;
    int bytes_sent = -1;

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
            while(true) {
                bzero(send_buffer, BUFFER_SIZE);

                // Receive request header
                received_bytes = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
                printf("received %d bytes\n", received_bytes);
                if (received_bytes == -1) {
                    perror("recv");
                    exit(1);
                }
                else if (received_bytes == 0) {
                    close(client_fd);
                    printf("***CLIENT CLOSED CONNECTION***\n");
                    exit(0);
                }
                string req1(buffer);

                // Parse request
                struct http_request *request;
                int extracted_bytes = 0;

                extracted_bytes += parse_request(buffer+extracted_bytes, received_bytes, &request);



                int headers_length = req1.find("\r\n\r\n") + 4;
                int cur_body_length = received_bytes - headers_length;
                printf("headers length: %d, received: %d, body: %d\n", headers_length, received_bytes, cur_body_length);
                int remaining_content = request->content_length - cur_body_length;
                int content_length = request->content_length;

                while (remaining_content > 0) {
                    received_bytes = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);

                    if (remaining_content < received_bytes) {      // last receive might contain another request;
                        realloc(request->body, cur_body_length + remaining_content);
                        memcpy(request->path + cur_body_length, buffer, remaining_content);
                        remaining_content = 0;
                        cur_body_length += remaining_content;
                    }
                    else {
                        remaining_content -= received_bytes;
                        char* new_stuff = (char*)malloc(cur_body_length + received_bytes);
                        memcpy(new_stuff, request->body, cur_body_length);
                        memcpy(new_stuff+cur_body_length, buffer, received_bytes);
                        free(request->body);
                        request->body = new_stuff;
//                        realloc(request->body, cur_body_length + received_bytes);
//                        memcpy(request->body + cur_body_length, buffer, received_bytes);
                        cur_body_length += received_bytes;
                    }
                    printf("received %d bytes, remaining %d\n", received_bytes, remaining_content);
                }
                printf("body length %d\n", cur_body_length);



                printf("received %s on %s\n", get_http_request_method(request->method), request->path);
                printf("filetype:%s, content-length:%d\n", get_http_request_filetype(request->filetype), request->content_length);
                printf("connection:%s, body:\n", get_http_request_connection(request->connection));

                if (request->filetype == http_request::FileType::HTML || request->filetype ==http_request::FileType::TXT) {
                    print_chars(request->body, content_length);
                }

                // Process request and prepare response
                int response_size;
                string actual_path = get_actual_path(request->path, SERVER_ROOT);

                if (request->method == http_request::HTTPMethod::GET) {     // GET request
                    char *file_buffer;
                    int file_size = read_file(actual_path.c_str(), &file_buffer);

                    printf("file size %d *****\n", file_size);
                    if (file_size == -1) {
                        response_size = load_response_not_found(send_buffer);

                        // Send response
                        bytes_sent = send(client_fd, (const void *) send_buffer, response_size, 0);
                        printf("sent bytes: %d/%d \n\n", bytes_sent, response_size);
                    }
                    else {
                        int response_headers_length = load_response_file_headers(send_buffer, file_size, request->filetype, "200 OK");
                        response_size = response_headers_length + file_size;
                        char* response = (char *)malloc(response_size);
                        memcpy(response, send_buffer, response_headers_length);
                        memcpy(response+response_headers_length, file_buffer, file_size);

                        // Send response
                        bytes_sent = send_all(client_fd, (const void *) response, response_size);
                        printf("sent bytes: %d/%d \n\n", bytes_sent, response_size);
                    }

                }
                else if (request->method == http_request::HTTPMethod::POST) {       // POST request
                    extract_body_to_file(request->body, request->content_length, actual_path);
                    response_size = load_response_success(send_buffer);

                    // Send response
                    bytes_sent = send(client_fd, (const void *) send_buffer, response_size, 0);
                    printf("sent bytes: %d/%d \n\n", bytes_sent, response_size);
                }


                fd_set rfds;
                struct timeval tv;
                int retval;

                /* Watch stdin (fd 0) to see when it has input. */
                FD_ZERO(&rfds);
                FD_SET(client_fd, &rfds);

                /* Wait up to five seconds. */
                tv.tv_sec = 5;
                tv.tv_usec = 0;

                cout << "***WAIT***" << endl;
                retval = select(client_fd+1, &rfds, NULL, NULL, &tv);
                /* Don't rely on the value of tv now! */
                if (retval == -1)
                    perror("select()");
                else if (retval == 0){
                    cout << "***TIMEOUT***" << endl;
                    close(client_fd);
                    exit(0);
                }

//                printf("parent: %d, self: %d\n", getppid(), getpid);
//                char cmd[100];
//                sprintf(cmd, "pgrep -P %d | wc -l\0", getppid());

            }

            printf("byee");
            close(client_fd);
            exit(0);
        }
        else { // This is the parent process

        }

    }
#pragma clang diagnostic pop

    return 0;
}


