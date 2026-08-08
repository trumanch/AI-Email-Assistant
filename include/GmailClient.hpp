#pragma once

#include "Email.hpp"

#include <cstddef>
#include <string>
#include <vector>

class GmailClient
{
public:
    explicit GmailClient(std::string accessToken);

    std::vector<Email> getMessages(
        const std::string& query,
        std::size_t maxResults = 20
    ) const;

private:
    std::string performGet(
        const std::string& url
    ) const;

    std::string accessToken_;
};