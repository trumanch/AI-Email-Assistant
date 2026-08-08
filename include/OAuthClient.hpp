#pragma once

#include <string>

class OAuthClient
{
public:
    OAuthClient(
        std::string credentialsPath,
        std::string tokenPath
    );

    bool authorize();

    std::string getAccessToken();

private:
    void loadCredentials();

    std::string credentialsPath_;
    std::string tokenPath_;

    std::string clientId_;
    std::string clientSecret_;
    std::string tokenUri_;
};