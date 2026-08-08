#include "OAuthClient.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <curl/curl.h>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <openssl/evp.h>
#include <openssl/rand.h>

#ifdef _WIN32
#include <shellapi.h>
#include <windows.h>
#endif

namespace
{
struct AuthorizationResult
{
    bool success = false;
    std::string code;
    std::string error;
};

struct HttpResponse
{
    long statusCode = 0;
    std::string body;
};

std::size_t writeCallback(
    void* contents,
    std::size_t size,
    std::size_t count,
    void* userData
)
{
    const std::size_t totalSize = size * count;

    auto* response =
        static_cast<std::string*>(userData);

    response->append(
        static_cast<char*>(contents),
        totalSize
    );

    return totalSize;
}

std::string base64UrlEncode(
    const unsigned char* data,
    std::size_t length
)
{
    const std::size_t encodedLength =
        4 * ((length + 2) / 3);

    std::string encoded(encodedLength, '\0');

    const int actualLength = EVP_EncodeBlock(
        reinterpret_cast<unsigned char*>(encoded.data()),
        data,
        static_cast<int>(length)
    );

    if (actualLength < 0)
    {
        throw std::runtime_error(
            "Failed to Base64 encode data."
        );
    }

    encoded.resize(
        static_cast<std::size_t>(actualLength)
    );

    for (char& character : encoded)
    {
        if (character == '+')
        {
            character = '-';
        }
        else if (character == '/')
        {
            character = '_';
        }
    }

    while (!encoded.empty() && encoded.back() == '=')
    {
        encoded.pop_back();
    }

    return encoded;
}

std::string generateRandomString(std::size_t byteCount)
{
    std::vector<unsigned char> bytes(byteCount);

    if (
        RAND_bytes(
            bytes.data(),
            static_cast<int>(bytes.size())
        ) != 1
    )
    {
        throw std::runtime_error(
            "Failed to generate secure random data."
        );
    }

    return base64UrlEncode(
        bytes.data(),
        bytes.size()
    );
}

std::string createCodeChallenge(
    const std::string& codeVerifier
)
{
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digestLength = 0;

    const int result = EVP_Digest(
        codeVerifier.data(),
        codeVerifier.size(),
        digest,
        &digestLength,
        EVP_sha256(),
        nullptr
    );

    if (result != 1)
    {
        throw std::runtime_error(
            "Failed to create PKCE code challenge."
        );
    }

    return base64UrlEncode(
        digest,
        digestLength
    );
}

std::string urlEncode(const std::string& value)
{
    CURL* curl = curl_easy_init();

    if (curl == nullptr)
    {
        throw std::runtime_error(
            "Failed to initialize CURL for URL encoding."
        );
    }

    char* encodedValue = curl_easy_escape(
        curl,
        value.c_str(),
        static_cast<int>(value.size())
    );

    if (encodedValue == nullptr)
    {
        curl_easy_cleanup(curl);

        throw std::runtime_error(
            "Failed to URL encode value."
        );
    }

    std::string result(encodedValue);

    curl_free(encodedValue);
    curl_easy_cleanup(curl);

    return result;
}

HttpResponse postForm(
    const std::string& url,
    const std::string& formBody
)
{
    CURL* curl = curl_easy_init();

    if (curl == nullptr)
    {
        throw std::runtime_error(
            "Failed to initialize CURL."
        );
    }

    HttpResponse response;

    curl_slist* headers = nullptr;
    headers = curl_slist_append(
        headers,
        "Content-Type: application/x-www-form-urlencoded"
    );

    curl_easy_setopt(
        curl,
        CURLOPT_URL,
        url.c_str()
    );

    curl_easy_setopt(
        curl,
        CURLOPT_POST,
        1L
    );

    curl_easy_setopt(
        curl,
        CURLOPT_POSTFIELDS,
        formBody.c_str()
    );

    curl_easy_setopt(
        curl,
        CURLOPT_POSTFIELDSIZE,
        static_cast<long>(formBody.size())
    );

    curl_easy_setopt(
        curl,
        CURLOPT_HTTPHEADER,
        headers
    );

    curl_easy_setopt(
        curl,
        CURLOPT_WRITEFUNCTION,
        writeCallback
    );

    curl_easy_setopt(
        curl,
        CURLOPT_WRITEDATA,
        &response.body
    );

    curl_easy_setopt(
        curl,
        CURLOPT_TIMEOUT,
        30L
    );

    curl_easy_setopt(
        curl,
        CURLOPT_USERAGENT,
        "AIEmailAssistant/0.1"
    );

    const CURLcode result =
        curl_easy_perform(curl);

    if (result != CURLE_OK)
    {
        const std::string errorMessage =
            curl_easy_strerror(result);

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        throw std::runtime_error(
            "Token request failed: " + errorMessage
        );
    }

    curl_easy_getinfo(
        curl,
        CURLINFO_RESPONSE_CODE,
        &response.statusCode
    );

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return response;
}

void openBrowser(const std::string& url)
{
#ifdef _WIN32
    const auto result = reinterpret_cast<std::intptr_t>(
        ShellExecuteA(
            nullptr,
            "open",
            url.c_str(),
            nullptr,
            nullptr,
            SW_SHOWNORMAL
        )
    );

    if (result <= 32)
    {
        std::cout
            << "Could not open the browser automatically.\n"
            << "Open this URL manually:\n\n"
            << url
            << "\n\n";
    }
#else
    std::cout
        << "Open this URL in your browser:\n\n"
        << url
        << "\n\n";
#endif
}
}

OAuthClient::OAuthClient(
    std::string credentialsPath,
    std::string tokenPath
)
    : credentialsPath_(std::move(credentialsPath)),
      tokenPath_(std::move(tokenPath))
{
}

void OAuthClient::loadCredentials()
{
    std::ifstream credentialsFile(credentialsPath_);

    if (!credentialsFile)
    {
        throw std::runtime_error(
            "Could not open " + credentialsPath_ +
            ". Make sure it is in the project folder."
        );
    }

    nlohmann::json credentials;
    credentialsFile >> credentials;

    if (!credentials.contains("installed"))
    {
        throw std::runtime_error(
            "The credentials file is not for a Desktop app. "
            "Create an OAuth client with application type Desktop app."
        );
    }

    const auto& installed =
        credentials.at("installed");

    clientId_ =
        installed.at("client_id").get<std::string>();

    clientSecret_ =
        installed.value("client_secret", "");

    tokenUri_ = installed.value(
        "token_uri",
        "https://oauth2.googleapis.com/token"
    );
}

bool OAuthClient::authorize()
{
    if (std::filesystem::exists(tokenPath_))
    {
        std::cout
            << tokenPath_
            << " already exists. Authorization skipped.\n"
            << "Delete token.json to authorize again.\n";

        return true;
    }

    loadCredentials();

    const std::string scope =
        "https://www.googleapis.com/auth/gmail.readonly";

    const std::string codeVerifier =
        generateRandomString(64);

    const std::string codeChallenge =
        createCodeChallenge(codeVerifier);

    const std::string state =
        generateRandomString(32);

    httplib::Server server;

    const int port =
        server.bind_to_any_port("127.0.0.1");

    if (port <= 0)
    {
        throw std::runtime_error(
            "Could not start the local OAuth server."
        );
    }

    const std::string redirectUri =
        "http://127.0.0.1:" +
        std::to_string(port);

    std::promise<AuthorizationResult> promise;
    std::future<AuthorizationResult> future =
        promise.get_future();

    std::atomic_bool resultReceived = false;

    server.Get(
        "/",
        [&](const httplib::Request& request,
            httplib::Response& response)
        {
            AuthorizationResult result;

            if (
                !request.has_param("state") ||
                request.get_param_value("state") != state
            )
            {
                result.error =
                    "Invalid OAuth state value.";
            }
            else if (request.has_param("error"))
            {
                result.error =
                    request.get_param_value("error");
            }
            else if (!request.has_param("code"))
            {
                result.error =
                    "Authorization code was not returned.";
            }
            else
            {
                result.success = true;
                result.code =
                    request.get_param_value("code");
            }

            if (result.success)
            {
                response.set_content(
                    R"(
                    <!doctype html>
                    <html>
                    <head>
                        <meta charset="utf-8">
                        <title>AI Email Assistant</title>
                    </head>
                    <body>
                        <h2>Authorization completed successfully.</h2>
                        <p>You may close this browser tab.</p>
                    </body>
                    </html>
                    )",
                    "text/html; charset=UTF-8"
                );
            }
            else
            {
                response.set_content(
                    R"(
                    <!doctype html>
                    <html>
                    <head>
                        <meta charset="utf-8">
                        <title>AI Email Assistant</title>
                    </head>
                    <body>
                        <h2>Authorization failed.</h2>
                        <p>Return to the application terminal.</p>
                    </body>
                    </html>
                    )",
                    "text/html; charset=UTF-8"
                );
            }

            if (!resultReceived.exchange(true))
            {
                promise.set_value(result);
            }
        }
    );

    const std::string authorizationUrl =
        "https://accounts.google.com/o/oauth2/v2/auth"
        "?client_id=" + urlEncode(clientId_) +
        "&redirect_uri=" + urlEncode(redirectUri) +
        "&response_type=code" +
        "&scope=" + urlEncode(scope) +
        "&state=" + urlEncode(state) +
        "&code_challenge=" + urlEncode(codeChallenge) +
        "&code_challenge_method=S256" +
        "&access_type=offline" +
        "&prompt=consent";

    std::thread serverThread(
        [&server]()
        {
            server.listen_after_bind();
        }
    );

    std::cout
        << "Opening Google authorization in your browser...\n"
        << "Waiting for authorization...\n";

    openBrowser(authorizationUrl);

    const auto waitResult =
        future.wait_for(std::chrono::minutes(10));

    if (waitResult != std::future_status::ready)
    {
        server.stop();

        if (serverThread.joinable())
        {
            serverThread.join();
        }

        throw std::runtime_error(
            "Authorization timed out."
        );
    }

    const AuthorizationResult authorization =
        future.get();

    std::this_thread::sleep_for(
        std::chrono::milliseconds(300)
    );

    server.stop();

    if (serverThread.joinable())
    {
        serverThread.join();
    }

    if (!authorization.success)
    {
        throw std::runtime_error(
            "Google authorization failed: " +
            authorization.error
        );
    }

    std::string formBody =
        "code=" + urlEncode(authorization.code) +
        "&client_id=" + urlEncode(clientId_) +
        "&code_verifier=" + urlEncode(codeVerifier) +
        "&redirect_uri=" + urlEncode(redirectUri) +
        "&grant_type=authorization_code";

    if (!clientSecret_.empty())
    {
        formBody +=
            "&client_secret=" +
            urlEncode(clientSecret_);
    }

    const HttpResponse tokenResponse =
        postForm(tokenUri_, formBody);

    nlohmann::json tokenJson =
        nlohmann::json::parse(
            tokenResponse.body,
            nullptr,
            false
        );

    if (
        tokenJson.is_discarded() ||
        tokenResponse.statusCode != 200 ||
        tokenJson.contains("error")
    )
    {
        std::string errorMessage =
            tokenResponse.body;

        if (!tokenJson.is_discarded())
        {
            errorMessage = tokenJson.value(
                "error_description",
                tokenJson.value(
                    "error",
                    tokenResponse.body
                )
            );
        }

        throw std::runtime_error(
            "Token exchange failed. HTTP " +
            std::to_string(tokenResponse.statusCode) +
            ": " +
            errorMessage
        );
    }

    if (!tokenJson.contains("access_token"))
    {
        throw std::runtime_error(
            "Google did not return an access token."
        );
    }

    const std::string grantedScopes =
        tokenJson.value("scope", "");

    if (grantedScopes.find(scope) == std::string::npos)
    {
        throw std::runtime_error(
            "The Gmail read-only permission was not granted."
        );
    }

    const auto currentTime =
        std::chrono::system_clock::now();

    const auto unixTime =
        std::chrono::duration_cast<std::chrono::seconds>(
            currentTime.time_since_epoch()
        ).count();

    tokenJson["created_at"] = unixTime;

    std::ofstream tokenFile(tokenPath_);

    if (!tokenFile)
    {
        throw std::runtime_error(
            "Could not create " + tokenPath_
        );
    }

    tokenFile << tokenJson.dump(4);

    std::cout
        << "Authorization successful.\n"
        << "Tokens were saved to "
        << tokenPath_
        << ".\n";

    return true;
}

std::string OAuthClient::getAccessToken()
{
    if (!std::filesystem::exists(tokenPath_))
    {
        authorize();
    }

    std::ifstream tokenFile(tokenPath_);

    if (!tokenFile)
    {
        throw std::runtime_error(
            "Could not open " + tokenPath_
        );
    }

    nlohmann::json tokenJson;
    tokenFile >> tokenJson;
    tokenFile.close();

    const auto currentTime =
        std::chrono::system_clock::now();

    const auto unixTime =
        std::chrono::duration_cast<std::chrono::seconds>(
            currentTime.time_since_epoch()
        ).count();

    const long long createdAt =
        tokenJson.value("created_at", 0LL);

    const long long expiresIn =
        tokenJson.value("expires_in", 0LL);

    const bool accessTokenMissing =
        !tokenJson.contains("access_token");

    const bool tokenExpired =
        createdAt <= 0 ||
        expiresIn <= 0 ||
        unixTime >= createdAt + expiresIn - 60;

    if (!accessTokenMissing && !tokenExpired)
    {
        return tokenJson.at("access_token")
            .get<std::string>();
    }

    const std::string refreshToken =
        tokenJson.value("refresh_token", "");

    if (refreshToken.empty())
    {
        throw std::runtime_error(
            "Refresh token is missing. "
            "Delete token.json and authorize again."
        );
    }

    loadCredentials();

    std::string formBody =
        "client_id=" + urlEncode(clientId_) +
        "&refresh_token=" + urlEncode(refreshToken) +
        "&grant_type=refresh_token";

    if (!clientSecret_.empty())
    {
        formBody +=
            "&client_secret=" +
            urlEncode(clientSecret_);
    }

    const HttpResponse tokenResponse =
        postForm(tokenUri_, formBody);

    nlohmann::json refreshedToken =
        nlohmann::json::parse(
            tokenResponse.body,
            nullptr,
            false
        );

    if (
        refreshedToken.is_discarded() ||
        tokenResponse.statusCode != 200 ||
        refreshedToken.contains("error")
    )
    {
        throw std::runtime_error(
            "Could not refresh access token. HTTP " +
            std::to_string(tokenResponse.statusCode) +
            ": " +
            tokenResponse.body
        );
    }

    if (!refreshedToken.contains("access_token"))
    {
        throw std::runtime_error(
            "Google did not return a new access token."
        );
    }

    refreshedToken["refresh_token"] = refreshToken;
    refreshedToken["created_at"] = unixTime;

    if (
        !refreshedToken.contains("scope") &&
        tokenJson.contains("scope")
    )
    {
        refreshedToken["scope"] = tokenJson["scope"];
    }

    if (
        !refreshedToken.contains("token_type") &&
        tokenJson.contains("token_type")
    )
    {
        refreshedToken["token_type"] =
            tokenJson["token_type"];
    }

    std::ofstream outputFile(tokenPath_);

    if (!outputFile)
    {
        throw std::runtime_error(
            "Could not update " + tokenPath_
        );
    }

    outputFile << refreshedToken.dump(4);

    std::cout
        << "Access token was refreshed successfully.\n";

    return refreshedToken.at("access_token")
        .get<std::string>();
}