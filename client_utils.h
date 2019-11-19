//
// Created by khaled on 11/16/19.
//

#ifndef SOCKET_PROG_CLIENT_UTILS_H
#define SOCKET_PROG_CLIENT_UTILS_H


#include <utility>
#include <vector>
#include <string>
#include <unordered_map>

#define CLIENT_ROOT "./client-root"

using namespace std;

vector<pair<string, string>> read_commands(string path);
int load_get_request(char* buffer, string path);
int load_post_request_headers(char* buffer, string path, int content_length);
int load_post_request(char* buffer, string path);
string get_file_name(string path);
string get_file_extension(string path);
int read_file(const char* path, char** file_buffer);

int extract_headers(char* buffer, unordered_map<string, string> *headers);
void log_body(char* buffer, int content_length);
char* construct_body(vector<pair<char*, int>> body, int content_length);
int handle_get_response_body(char* body, int content_length, string path);
int handle_get_response(char* buffer, string path, int extracted_bytes);

#endif //SOCKET_PROG_CLIENT_UTILS_H