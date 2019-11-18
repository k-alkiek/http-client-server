//
// Created by khaled on 11/17/19.
//

#include <string>
#include <fstream>
#include "utils.h"

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

