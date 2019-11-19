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


