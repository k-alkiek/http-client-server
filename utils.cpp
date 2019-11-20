//
// Created by khaled on 11/17/19.
//

#include <string>
#include <fstream>
#include <sys/socket.h>
#include <pcap/socket.h>
#include <unordered_map>
#include <cstring>
#include "utils.h"

int send_all(int socket, const void *data, int data_size) {
    const char *data_ptr = (const char*) data;
    int total_bytes_sent = 0;
    int bytes_sent;

    while (data_size > 0)
    {
        int to_send = min(data_size, 1024*1024);
        bytes_sent = send(socket, data_ptr, to_send, 0);
        printf("sent %d bytes\n", bytes_sent);
        if (bytes_sent == -1)
            return -1;

        data_ptr += bytes_sent;
        total_bytes_sent += bytes_sent;
        data_size -= bytes_sent;
    }

    return total_bytes_sent;
}

int send_all(int socket, vector<char> *send_buffer) {
    int data_size = send_buffer->size();
    char *buffer_arr = new char[data_size];
    for (int i = 0; i < data_size; i++) {
        buffer_arr[i] = send_buffer->at(i);
    }

    char *data_ptr = buffer_arr;

    int total_bytes_sent = 0;
    int bytes_sent;

    while (data_size > 0)
    {
        int to_send = min(data_size, 1024*1024*4);
        bytes_sent = send(socket, data_ptr, to_send, 0);
        printf("sent %d bytes\n", bytes_sent);
        if (bytes_sent == -1)
            return -1;

        data_ptr += bytes_sent;
        total_bytes_sent += bytes_sent;
        data_size -= bytes_sent;
    }
    delete[] buffer_arr;
    return total_bytes_sent;
}

void print_chars(char *buffer, int length) {
    for (char *c = buffer; c - buffer < length; c++) {
        printf("%c", *c);
    }
    printf("\n");
}

void extract_body_to_file(char* buffer, int content_length, string path) {
    ofstream file(path);
    file.write(buffer, content_length);
    file.close();
}

string get_file_extension(string path) {
    char sep = '.';
    size_t i = path.rfind(sep, path.length());
    if (i!= string::npos) {
        return (path.substr(i+1, path.length() - i));
    }
    return "";
}

string get_actual_path(const char* path, const char* root_path) {
    string actual_path(root_path);
    actual_path += path;
    return actual_path;
}

std::string& ltrim(std::string& str, const std::string& chars)
{
    str.erase(0, str.find_first_not_of(chars));
    return str;
}

std::string& rtrim(std::string& str, const std::string& chars)
{
    str.erase(str.find_last_not_of(chars) + 1);
    return str;
}

std::string& trim(std::string& str, const std::string& chars)
{
    return ltrim(rtrim(str, chars), chars);
}

