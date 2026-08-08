#include "GmailClient.hpp"

#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <openssl/evp.h>

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{
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
            "Failed to encode Gmail search query."
        );
    }

    std::string encoded(encodedValue);

    curl_free(encodedValue);
    curl_easy_cleanup(curl);

    return encoded;
}

std::string getHeaderValue(
    const nlohmann::json& message,
    const std::string& headerName
)
{
    if (
        !message.contains("payload") ||
        !message["payload"].contains("headers")
    )
    {
        return "(not available)";
    }

    for (
        const auto& header :
        message["payload"]["headers"]
    )
    {
        const std::string currentName =
            header.value("name", "");

        if (currentName == headerName)
        {
            return header.value(
                "value",
                "(not available)"
            );
        }
    }

    return "(not available)";
}

std::string base64UrlDecode(std::string encoded)
{
    if (encoded.empty())
    {
        return {};
    }

    std::replace(
        encoded.begin(),
        encoded.end(),
        '-',
        '+'
    );

    std::replace(
        encoded.begin(),
        encoded.end(),
        '_',
        '/'
    );

    while (encoded.size() % 4 != 0)
    {
        encoded.push_back('=');
    }

    std::vector<unsigned char> decoded(
        (encoded.size() * 3) / 4 + 1
    );

    const int decodedLength = EVP_DecodeBlock(
        decoded.data(),
        reinterpret_cast<const unsigned char*>(
            encoded.data()
        ),
        static_cast<int>(encoded.size())
    );

    if (decodedLength < 0)
    {
        throw std::runtime_error(
            "Failed to decode Gmail message body."
        );
    }

    std::size_t padding = 0;

    if (!encoded.empty() && encoded.back() == '=')
    {
        ++padding;
    }

    if (
        encoded.size() >= 2 &&
        encoded[encoded.size() - 2] == '='
    )
    {
        ++padding;
    }

    const std::size_t finalLength =
        static_cast<std::size_t>(decodedLength) -
        padding;

    return std::string(
        reinterpret_cast<const char*>(decoded.data()),
        finalLength
    );
}

std::string findMimePart(
    const nlohmann::json& part,
    const std::string& requiredMimeType
)
{
    const std::string mimeType =
        part.value("mimeType", "");

    if (
        mimeType == requiredMimeType &&
        part.contains("body")
    )
    {
        const auto& body = part["body"];

        if (
            body.contains("data") &&
            body["data"].is_string()
        )
        {
            return base64UrlDecode(
                body["data"].get<std::string>()
            );
        }
    }

    if (
        part.contains("parts") &&
        part["parts"].is_array()
    )
    {
        for (const auto& childPart : part["parts"])
        {
            std::string result =
                findMimePart(
                    childPart,
                    requiredMimeType
                );

            if (!result.empty())
            {
                return result;
            }
        }
    }

    return {};
}

std::string extractEmailBody(
    const nlohmann::json& message
)
{
    if (!message.contains("payload"))
    {
        return message.value(
            "snippet",
            "(email body is not available)"
        );
    }

    const auto& payload = message["payload"];

    std::string plainText =
        findMimePart(
            payload,
            "text/plain"
        );

    if (!plainText.empty())
    {
        return plainText;
    }

    std::string htmlText =
        findMimePart(
            payload,
            "text/html"
        );

    if (!htmlText.empty())
    {
        return htmlText;
    }

    return message.value(
        "snippet",
        "(email body is not available)"
    );
}
}

GmailClient::GmailClient(std::string accessToken)
    : accessToken_(std::move(accessToken))
{
    if (accessToken_.empty())
    {
        throw std::invalid_argument(
            "Gmail access token cannot be empty."
        );
    }
}

std::string GmailClient::performGet(
    const std::string& url
) const
{
    CURL* curl = curl_easy_init();

    if (curl == nullptr)
    {
        throw std::runtime_error(
            "Failed to initialize CURL."
        );
    }

    HttpResponse response;

    const std::string authorizationHeader =
        "Authorization: Bearer " +
        accessToken_;

    curl_slist* headers = nullptr;

    headers = curl_slist_append(
        headers,
        authorizationHeader.c_str()
    );

    headers = curl_slist_append(
        headers,
        "Accept: application/json"
    );

    curl_easy_setopt(
        curl,
        CURLOPT_URL,
        url.c_str()
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
        CURLOPT_FOLLOWLOCATION,
        1L
    );

    curl_easy_setopt(
        curl,
        CURLOPT_TIMEOUT,
        30L
    );

    curl_easy_setopt(
        curl,
        CURLOPT_USERAGENT,
        "AIEmailAssistant/0.2"
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
            "Gmail request failed: " +
            errorMessage
        );
    }

    curl_easy_getinfo(
        curl,
        CURLINFO_RESPONSE_CODE,
        &response.statusCode
    );

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (
        response.statusCode < 200 ||
        response.statusCode >= 300
    )
    {
        std::string errorMessage =
            response.body;

        const nlohmann::json errorJson =
            nlohmann::json::parse(
                response.body,
                nullptr,
                false
            );

        if (
            !errorJson.is_discarded() &&
            errorJson.contains("error")
        )
        {
            errorMessage =
                errorJson["error"].value(
                    "message",
                    response.body
                );
        }

        throw std::runtime_error(
            "Gmail API returned HTTP " +
            std::to_string(response.statusCode) +
            ": " +
            errorMessage
        );
    }

    return response.body;
}

std::vector<Email> GmailClient::getMessages(
    const std::string& query,
    std::size_t maxResults
) const
{
    if (maxResults == 0)
    {
        return {};
    }

    if (maxResults > 100)
    {
        maxResults = 100;
    }

    const std::string listUrl =
        "https://gmail.googleapis.com/"
        "gmail/v1/users/me/messages"
        "?q=" + urlEncode(query) +
        "&maxResults=" +
        std::to_string(maxResults);

    const nlohmann::json messageList =
        nlohmann::json::parse(
            performGet(listUrl)
        );

    std::vector<Email> emails;

    if (
        !messageList.contains("messages") ||
        !messageList["messages"].is_array()
    )
    {
        return emails;
    }

    for (
        const auto& messageReference :
        messageList["messages"]
    )
    {
        const std::string messageId =
            messageReference.at("id")
                .get<std::string>();

        const std::string messageUrl =
            "https://gmail.googleapis.com/"
            "gmail/v1/users/me/messages/" +
            messageId +
            "?format=full";

        const nlohmann::json message =
            nlohmann::json::parse(
                performGet(messageUrl)
            );

        Email email;

        email.id =
            message.value("id", messageId);

        email.threadId =
            message.value("threadId", "");

        email.sender =
            getHeaderValue(message, "From");

        email.subject =
            getHeaderValue(message, "Subject");

        email.date =
            getHeaderValue(message, "Date");

        email.body =
            extractEmailBody(message);

        emails.push_back(std::move(email));
    }

    return emails;
}