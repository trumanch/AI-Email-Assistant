#pragma once

#include "Email.hpp"
#include "EmailAnalysis.hpp"

#include <string>

class OllamaClient
{
public:
    explicit OllamaClient(
        std::string model = "qwen3:4b"
    );

    EmailAnalysis analyzeEmail(
        const Email& email
    ) const;

private:
    std::string postJson(
        const std::string& requestBody
    ) const;

    std::string model_;
};