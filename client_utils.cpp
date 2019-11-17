//
// Created by khaled on 11/16/19.
//

#include <sstream>
#include <fstream>
#include <cstring>
#include <iostream>
#include "utils.h"
#include "client_utils.h"

#define CLIENT_ROOT "./client-root"

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

string get_file_name(string path) {
    char sep = '/';
    size_t i = path.rfind(sep, path.length());
    if (i!= string::npos) {
        return (path.substr(i+1, path.length() - i));
    }
    return "";
}

string get_file_extension(string path) {
    char sep = '.';
    size_t i = path.rfind(sep, path.length());
    if (i!= string::npos) {
        return (path.substr(i+1, path.length() - i));
    }
    return "";
}

// Returns number of bytes used by the request
int load_get_request(char* buffer, string path) {
    string request = "GET " + path + " HTML/1.1\r\n";
    request += "Connection: keep-alive\r\n";
    request += "\r\n";
    memcpy(buffer, request.c_str(), request.length());

    return request.length();
}

int load_post_request(char* buffer, string path) {
    char* file_buffer;
    string actual_path(CLIENT_ROOT);
    actual_path += path;
    int content_length = read_file(actual_path.c_str(), &file_buffer);

    // TODO
    string dest_path = "/" + get_file_name(path);
    string headers = "POST " + dest_path + " HTML/1.1\r\n";
    headers += "Content-Type: " + get_file_extension(path) + "\r\n";
    headers += "Content-Length: " + to_string(content_length) + "\r\n";
    headers += "Connection: keep-alive\r\n";
    headers += "\r\n";
    memcpy(buffer, headers.c_str(), headers.length());
    memcpy(buffer + headers.length(), file_buffer, content_length);

    return headers.length() + content_length;
}

int extract_headers(char* buffer, unordered_map<string, string> *headers) {
    char *headers_end = strstr(buffer, "\r\n\r\n");
    *headers_end = '\0';
    int headers_length = strlen(buffer)+4;
    char* token = strtok(buffer, "\r\n");
    headers->insert(make_pair("Status", string(token)));
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

void log_body(char* buffer, int content_length) {
    char* end = buffer + content_length;
    for (char* c = buffer; c != end; c++) {
        printf("%c", *c);
    }
}

void extract_body_to_file(char* buffer, int content_length, string path) {
    ofstream file(path);
    file.write(buffer, content_length);
    file.close();
}

int handle_get_response(char* buffer, string path, int extracted_bytes) {
    unordered_map<string, string> headers;
    extracted_bytes += extract_headers(buffer+extracted_bytes, &headers);
    int content_length = stoi(headers.find("Content-Length")->second);
    if (headers.find("Content-Type")->second.substr(0, 4).compare("text") == 0) {
        log_body(buffer+extracted_bytes, content_length);
    }
    string dest_path = CLIENT_ROOT;
    dest_path += "/" + get_file_name(path);
    extract_body_to_file(buffer+extracted_bytes, content_length, dest_path);
    extracted_bytes += content_length;
    return extracted_bytes;
}