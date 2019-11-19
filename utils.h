//
// Created by khaled on 11/17/19.
//

#ifndef SOCKET_PROG_UTILS_H
#define SOCKET_PROG_UTILS_H

#define SERVER_ROOT "./server-root"

using namespace std;

int send_all(int socket, const void *data, int data_size);

void print_chars(char *buffer, int length);
void extract_body_to_file(char* buffer, int content_length, string path);
string get_file_extension(string path);
string get_actual_path(const char* path, const char* root_path);

string& ltrim(string& str, const string& chars = "\t\n\v\f\r ");
string& rtrim(string& str, const string& chars = "\t\n\v\f\r ");
string& trim(string& str, const string& chars = "\t\n\v\f\r ");

#endif //SOCKET_PROG_UTILS_H
