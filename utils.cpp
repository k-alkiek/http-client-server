//
// Created by khaled on 11/17/19.
//

#include <string>
#include "utils.h"

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

