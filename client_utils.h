//
// Created by khaled on 11/16/19.
//

#ifndef SOCKET_PROG_CLIENT_UTILS_H
#define SOCKET_PROG_CLIENT_UTILS_H


#include <utility>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>

#define CLIENT_ROOT_DIR "./client-root"

using namespace std;

vector<pair<string, string>> read_commands(string path);
string get_file_name(string path);
string get_file_extension(string path);

struct Request *create_request(string method, string path);
void append_send_buffer(vector<char> *send_buffer, struct Request *request);

string get_actual_path(string path);
int read_file(string path, char **buffer);
void write_file(vector<char> *buffer, string path);

#endif //SOCKET_PROG_CLIENT_UTILS_H