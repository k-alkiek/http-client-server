//
// Created by khaled on 11/17/19.
//

#ifndef SOCKET_PROG_UTILS_H
#define SOCKET_PROG_UTILS_H

#define SERVER_ROOT "./server-root"

using namespace std;

string get_actual_path(const char* path, const char* root_path);

string& ltrim(string& str, const string& chars = "\t\n\v\f\r ");
string& rtrim(string& str, const string& chars = "\t\n\v\f\r ");
string& trim(string& str, const string& chars = "\t\n\v\f\r ");

#endif //SOCKET_PROG_UTILS_H
