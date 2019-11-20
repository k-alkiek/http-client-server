//
// Created by khaled on 11/17/19.
//

#ifndef SOCKET_PROG_UTILS_H
#define SOCKET_PROG_UTILS_H



#include <vector>
#include <map>

using namespace std;

struct Request {
    string method;
    string path;
    string connection;

    // Specific to POST requests
    string content_type;
    int content_length;

    // Specific to client POST
    char *body;
};

struct Response {
    string status;
    string connection;

    // Specific to GET requests
    string content_type;
    int content_length;
    char* body;

};

vector<char>::iterator find_crlf2(vector<char> *buffer);
void extract_headers_from_leftover(vector<char> *headers_buffer, vector<char> *leftover);
bool headers_complete(vector<char> *headers_buffer);
void append_headers(vector<char> *headers_buffer, char *receive_buffer, int received_bytes, vector<char> *leftover);
void remove_headers_leftover(vector<char> *headers_buffer, vector<char> *leftover);
map<string, string> process_headers(vector<char> *headers_buffer);

int extract_body_from_leftover(vector<char> *body_buffer, vector<char> *leftover, int remaining_length);
int append_body(vector<char> *body_buffer, char *receive_buffer, int received_bytes, vector<char> *leftover, int remaining_length);




int send_all(int socket, vector<char> *send_buffer);

void log_vector(vector<char> *v);
void print_chars(char *buffer, int length);
void extract_body_to_file(char* buffer, int content_length, string path);
string get_file_extension(string path);
string get_content_type(string extension);


string& ltrim(string& str, const string& chars = "\t\n\v\f\r ");
string& rtrim(string& str, const string& chars = "\t\n\v\f\r ");
string& trim(string& str, const string& chars = "\t\n\v\f\r ");

#endif //SOCKET_PROG_UTILS_H
