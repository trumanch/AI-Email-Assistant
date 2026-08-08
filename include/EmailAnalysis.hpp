#pragma once

#include <string>

struct EmailAnalysis
{
    std::string category;
    std::string priority;
    std::string summary;
    bool actionRequired = false;
    std::string recommendedAction;
    std::string suggestedReply;
};