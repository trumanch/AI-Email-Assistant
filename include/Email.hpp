#pragma once

#include <string>

struct Email
{
    std::string id;
    std::string threadId;
    std::string sender;
    std::string subject;
    std::string date;
    std::string body;
};