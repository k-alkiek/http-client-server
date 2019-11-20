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



void log_vector(vector<char> *v) {
    string s(v->begin(), v->end());
    cout << s << endl;
}

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

string get_content_type(string extension) {
    if (extension.compare("txt") == 0) {
        return "text/plain";
    }
    if (extension.compare("html") == 0) {
        return "text/html";
    }
    if (extension.compare("jpg") == 0) {
        return "image/jpg";
    }
    if (extension.compare("png") == 0) {
        return "image/png";
    }
    if (extension.compare("gif") == 0) {
        return "image/gif";
    }
    if (extension.compare("ico") == 0) {
        return "image/x-icon";
    }
    if (extension.compare("pdf") == 0) {
        return "application/pdf";
    }
    return "text/plain";
}

struct Response *create_get_response(string status, string connection, string path, char* file_buffer, int file_size) {
    struct Response *response = new Response;
    response->status = status;
    response->connection = connection;
    response->content_length = file_size;
    response->body = file_buffer;

    std::vector<std::string> v;
    boost::algorithm::split(v, path, boost::is_any_of("."));
    if (v.size() == 1) {
        response->content_type = "text/plain";
    }
    else {
        string extension = boost::algorithm::to_lower_copy(v[1]);
        response->content_type = get_content_type(extension);
    }

    return response;
}

struct Response *create_post_response(string status, string connection) {
    struct Response *response = new Response;
    response->status = status;
    response->connection = connection;
    response->content_length = 0;
    response->body = NULL;
    response->content_type = nullptr;

    return response;
}

vector<char>::iterator find_crlf2(vector<char> *buffer) {
    const char *headers_delim = "\r\n\r\n";
    auto it = search(buffer->begin(), buffer->end(), headers_delim, headers_delim + strlen(headers_delim));

    return it;
}

void extract_headers_from_leftover(vector<char> *headers_buffer, vector<char> *leftover) {
    auto it = find_crlf2(leftover);
    headers_buffer->reserve(it - leftover->begin());
    copy(leftover->begin(), it, back_inserter(*headers_buffer));

    leftover->erase(leftover->begin(), it);
}

bool headers_complete(vector<char> *headers_buffer) {
    auto it = find_crlf2(headers_buffer);
    if (it == headers_buffer->end()) {      // delimiter not found
        return false;
    }
    return true;

}

void append_headers(vector<char> *headers_buffer, char *receive_buffer, int received_bytes, vector<char> *leftover) {
    // Find iterator on crlf2
    vector<char> receive_vector(receive_buffer, receive_buffer + received_bytes);
    auto it = find_crlf2(&receive_vector);
    it = min(it + 4, receive_vector.end());

    // Copy character upto crlf2 to headers buffer
    headers_buffer->reserve(headers_buffer->size() + distance(receive_vector.begin(), it));
    headers_buffer->insert(headers_buffer->end(), receive_vector.begin(), it);

    // Copy remaining characters if any to leftover
    leftover->reserve(distance(it, receive_vector.end()));
    leftover->insert(leftover->begin(), it, receive_vector.end());
}

void remove_headers_leftover(vector<char> *headers_buffer, vector<char> *leftover) {
    auto it = find_crlf2(headers_buffer) + 4;

    leftover->resize(leftover->size() + distance(it, headers_buffer->end()));
    leftover->insert(leftover->begin(), it, headers_buffer->end());
}

map<string, string> process_headers(vector<char> *headers_buffer) {
    map<string, string> headers_data;
    string header;
    stringstream headers_stream(string(headers_buffer->begin(), headers_buffer->end()));

    getline(headers_stream, header);
    headers_data.insert(make_pair("Head", boost::algorithm::trim_copy(header)));

    string::size_type index;
    while (getline(headers_stream, header) && header != "\r") {
        index = header.find(':', 0);
        if(index != std::string::npos) {
            headers_data.insert(std::make_pair(
                    boost::algorithm::trim_copy(header.substr(0, index)),
                    boost::algorithm::trim_copy(header.substr(index + 1))
            ));
        }
    }
    return headers_data;
}

int extract_body_from_leftover(vector<char> *body_buffer, vector<char> *leftover, int remaining_length) {
    auto start = leftover->begin();
    auto end = min(start + remaining_length, leftover->end());

    body_buffer->reserve(distance(start, end));
    copy(start, end, back_inserter(*body_buffer));

    leftover->erase(start, end);
}

int append_body(vector<char> *body_buffer, char *receive_buffer, int received_bytes,
                vector<char> *leftover, int remaining_length) {

    vector<char> receive_vector(receive_buffer, receive_buffer + received_bytes);

    auto start = receive_vector.begin();
    auto end = min(start + remaining_length, receive_vector.end());

    // Copy character upto crlf2 to headers buffer
    body_buffer->reserve(body_buffer->size() + distance(start, end));
    body_buffer->insert(body_buffer->end(), start, end);

    // Copy remaining characters if any to leftover
    leftover->reserve(distance(end, receive_vector.end()));
    leftover->insert(leftover->begin(), end, receive_vector.end());

    return distance(start, end);
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
    actual_path += path;
    return actual_path;
}

int load_file(string path, char **buffer) {
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
    char buffer_arr[buffer->size()];

    for (int i = 0; i < buffer->size(); i++) {
        buffer_arr[i] = buffer->at(i);
    }

    ofstream file(actual_path);
    file.write(buffer_arr, buffer->size());
    file.close();
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
