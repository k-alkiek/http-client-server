//
// Created by khaled on 11/16/19.
//

#include <sstream>
#include <fstream>
#include <cstring>
#include <iostream>
#include "utils.h"
#include "client_utils.h"



using namespace std;

vector<pair<string, string>> read_commands(std::string path) {
    ifstream file(path);
    string line;

    vector<pair<string, string>> commands;
    while (getline(file, line)) {
        if (line.at(0) == '#') continue;    // commented line
        stringstream line_stream(line);
        string method, path;
        line_stream >> method >> path;

        if (method.compare("client_get") == 0) {
            method = "GET";
        } else {
            method = "POST";
        }
        commands.push_back(make_pair(method, path));
    }
    return commands;
}


string get_file_name(string path) {
    char sep = '/';
    size_t i = path.rfind(sep, path.length());
    if (i!= string::npos) {
        return (path.substr(i+1, path.length() - i));
    }
    return "";
}

struct Request *create_request(string method, string path) {
    struct Request *request = new Request;
    request->method = method;
    request->path = path;
    request->connection = "keep-alive";

    if (method.compare("POST") == 0) {
        char *file_buffer;
        request->content_length = read_file(path, &file_buffer);
        request->body = file_buffer;
        string extension = get_file_extension(path);
        request->content_type = get_content_type(extension);
    }
    return request;
}


void append_send_buffer(vector<char> *send_buffer, struct Request *request) {
    string headers = "";
    headers += request->method + " " + request->path + " HTTP/1.1\r\n";
    headers += "Connection: " + request->connection + "\r\n";

    if (request->method.compare("POST") == 0) {
        headers += "Content-Length: " + to_string(request->content_length) + "\r\n";
        headers += "Content-Type: " + request->content_type + "\r\n";
    }
    headers += "\r\n";
    int headers_length = headers.size();
    send_buffer->reserve(headers_length);
    copy(headers.begin(), headers.end(), back_inserter(*send_buffer));

    cout << headers << endl;

    if (request->method.compare("POST") == 0) {
        char *file_buffer;
        int file_size = read_file(request->path, &file_buffer);
        if (file_size == -1) {
            cerr << "File: " << request->path << " not found" << endl;
        }

        if (request->content_type.substr(0, 4).compare("text") == 0) {
            print_chars(file_buffer, file_size);
        }
        send_buffer->reserve(headers_length + file_size);
        send_buffer->insert(send_buffer->end(), file_buffer, file_buffer + file_size);
        free(file_buffer);
    }

}


string get_actual_path(string path) {
    string actual_path = CLIENT_ROOT_DIR;
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
