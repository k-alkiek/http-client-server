//
// Created by khaled on 11/2/19.
//


#ifndef SOCKET_PROG_SERVER_UTILS_H
#define SOCKET_PROG_SERVER_UTILS_H

#include <unordered_map>

#define SERVER_ROOT_DIR "./server-root"

using namespace std;




struct Request *create_request(map<string, string> headers_map);
struct Response *create_get_response(string status, string connection, string path, char* file_buffer, int file_size);
struct Response *create_post_response(string status, string connection);


void populate_send_buffer(vector<char> *send_buffer, string method, struct Response response);

string get_actual_path(string path);
int read_file(string path, char **buffer);
void write_file(vector<char> *buffer, string path);

double wait_time_heuristic(int max_connections);
int persist_connection(int socket_fd, double wait_time);

void print_client(struct sockaddr *);
int extract_headers(char* buffer, unordered_map<string, string> *headers);

void handle_sigchld(int sig);


std::string exec(const char* cmd);




#endif //SOCKET_PROG_SERVER_UTILS_H
