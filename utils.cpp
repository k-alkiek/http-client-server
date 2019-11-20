//
// Created by khaled on 11/17/19.
//

#include <string>
#include <fstream>
#include <sys/socket.h>
#include <pcap/socket.h>
#include <unordered_map>
#include <cstring>
#include <iostream>
#include <boost/algorithm/string.hpp>
#include <sstream>
#include "utils.h"



vector<char>::iterator find_crlf2(vector<char> *buffer) {
    const char *headers_delim = "\r\n\r\n";
    auto it = search(buffer->begin(), buffer->end(), headers_delim, headers_delim + strlen(headers_delim));

    return it;
}

void extract_headers_from_leftover(vector<char> *headers_buffer, vector<char> *leftover) {
    auto it = find_crlf2(leftover);
    it = min(it+4, leftover->end());

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
    return distance(start, end);
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

void log_vector(vector<char> *v) {
    string s(v->begin(), v->end());
    cout << s << endl;
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

string get_content_type(string extension) {
    if (extension.compare("txt") == 0) {
        return "text/plain";
    }
    if (extension.compare("html") == 0) {
        return "text/html";
    }
    if (extension.compare("css") == 0) {
        return "text/css";
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

