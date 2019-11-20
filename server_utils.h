//
// Created by khaled on 11/2/19.
//


#ifndef SOCKET_PROG_SERVER_UTILS_H
#define SOCKET_PROG_SERVER_UTILS_H

#include <unordered_map>

#define SERVER_ROOT_DIR "./server-root"

using namespace std;


struct Request {
    string method;
    string connection;
    string path;

    // Specific to POST requests
    string content_type;
    int content_length;
};

struct Response {
    string status;
    string connection;

    // Specific to GET requests
    string content_type;
    int content_length;
    char* body;

};

void log_vector(vector<char> *v);
struct Request *create_request(map<string, string> headers_map);
struct Response *create_get_response(string status, string connection, string path, char* file_buffer, int file_size);
struct Response *create_post_response(string status, string connection);

vector<char>::iterator find_crlf2(vector<char> *buffer);
void extract_headers_from_leftover(vector<char> *headers_buffer, vector<char> *leftover);
bool headers_complete(vector<char> *headers_buffer);
void append_headers(vector<char> *headers_buffer, char *receive_buffer, int received_bytes, vector<char> *leftover);
void remove_headers_leftover(vector<char> *headers_buffer, vector<char> *leftover);
map<string, string> process_headers(vector<char> *headers_buffer);

int extract_body_from_leftover(vector<char> *body_buffer, vector<char> *leftover, int remaining_length);
int append_body(vector<char> *body_buffer, char *receive_buffer, int received_bytes, vector<char> *leftover, int remaining_length);

void populate_send_buffer(vector<char> *send_buffer, string method, struct Response response);

string get_actual_path(string path);
int load_file(string path, char **buffer);
void write_file(vector<char> *buffer, string path);

double wait_time_heuristic(int max_connections);
int persist_connection(int socket_fd, double wait_time);

void print_client(struct sockaddr *);
int extract_headers(char* buffer, unordered_map<string, string> *headers);

void handle_sigchld(int sig);
int read_file(const char* path, char** file_buffer);


std::string exec(const char* cmd);




#endif //SOCKET_PROG_SERVER_UTILS_H
