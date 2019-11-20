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
#include <map>
#include <boost/algorithm/string.hpp>
#include <math.h>
#include "server_utils.h"


using namespace std;





struct Request *create_request(map<string, string> headers_map) {
    struct Request *request = new Request;

    string head = headers_map.find("Head")->second;
    std::vector<std::string> v;
    boost::algorithm::split(v, head, boost::algorithm::is_space());
    request->method = v[0];
    request->path = v[1];

    request->connection = "keep-alive";
    if (headers_map.find("Connection") != headers_map.end()) {
        request->connection = boost::algorithm::to_lower_copy(headers_map.find("Connection")->second);
    }

    // Headers specific to POST requests
    if (request->method.compare("POST") == 0) {
        request->content_length = stoi(headers_map.find("Content-Length")->second);
        request->content_type = headers_map.find("Content-Type")->second;
    }

    return request;
}

struct Response *create_get_response(string status, string connection, string path, char* file_buffer, int file_size) {
    struct Response *response = new Response;
    response->status = status;
    response->connection = connection;
    response->content_length = file_size;
    response->body = file_buffer;

    string extension = get_file_extension(path);
    response->content_type = get_content_type(extension);

    return response;
}

struct Response *create_post_response(string status, string connection) {
    struct Response *response = new Response;
    response->status = status;
    response->connection = connection;
    response->content_length = 0;
    response->body = NULL;


    return response;
}


void populate_send_buffer(vector<char> *send_buffer, string method, struct Response response) {
    string headers = "";
    headers += "HTTP/1.1 " + response.status + "\r\n";
    headers += "Connection: " + response.connection + "\r\n";

    if (method.compare("GET") == 0) {
        headers += "Content-Length: " + to_string(response.content_length) + "\r\n";
        headers += "Content-Type: " + response.content_type + "\r\n";
    }
    headers += "\r\n";
    int headers_length = headers.size();
    send_buffer->reserve(headers_length);
    copy(headers.begin(), headers.end(), back_inserter(*send_buffer));

    if (method.compare("GET") == 0) {
        send_buffer->reserve(headers_length + response.content_length);
        send_buffer->insert(send_buffer->end(), response.body, response.body + response.content_length);
    }

    cout << "response: headers length = " << headers.size() << "bytes" << endl;
    cout << "response: " << endl;
    if (response.content_type.substr(0, 4).compare("text") == 0) {
        log_vector(send_buffer);
    }
    else {
        cout << headers;
    }
}

string get_actual_path(string path) {
    string actual_path = SERVER_ROOT_DIR;
    if (path.at(0) != '/')
        actual_path += "/";
    actual_path += path;
    return actual_path;
}

int read_file(string path, char **buffer) {
    ifstream file;
    file.open(get_actual_path(path), ios_base::binary);

    file.seekg(0, ios::end);
    int length = file.tellg();
    file.seekg(0, ios::beg);

    if (length == -1)
        return -1;

    char* res = (char *)malloc(length);
    file.read(res, length);

    *buffer = res;
    return length;
}

void write_file(vector<char> *buffer, string path) {
    string actual_path = get_actual_path(path);
    char* buffer_arr = (char *)malloc(buffer->size());

    for (int i = 0; i < buffer->size(); i++) {
        buffer_arr[i] = buffer->at(i);
    }

    ofstream file(actual_path);
    file.write(buffer_arr, buffer->size());
    file.close();
    free(buffer_arr);
}

double wait_time_heuristic(int max_connections) {
    char cmd[100];
    sprintf(cmd, "pgrep -P %d | wc -l\0", getppid());

    int n_processes = stoi(exec(cmd));
    double time = 10/n_processes;
    return time;
}

int persist_connection(int socket_fd, double wait_time) {

    fd_set rfds;
    struct timeval tv;
    int retval;

    /* Watch stdin (fd 0) to see when it has input. */
    FD_ZERO(&rfds);
    FD_SET(socket_fd, &rfds);

    /* Wait up to five seconds. */
    tv.tv_sec = floor(wait_time);
    tv.tv_usec = (int)(wait_time - floor(wait_time))*1000000;

    return select(socket_fd+1, &rfds, NULL, NULL, &tv);
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


void handle_sigchld(int sig) {
    int saved_errno = errno;
    while (waitpid((pid_t)(-1), 0, WNOHANG) > 0) {}
    errno = saved_errno;
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
