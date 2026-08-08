#pragma once

#include "Email.hpp"
#include "EmailAnalysis.hpp"

#include <cstddef>
#include <string>
#include <vector>

struct sqlite3;

struct StoredEmailAnalysis
{
    int id = 0;

    std::string gmailMessageId;
    std::string sender;
    std::string subject;

    std::string category;
    std::string priority;
    std::string summary;

    bool actionRequired = false;

    std::string recommendedAction;
    std::string suggestedReply;
    std::string processedAt;
};

class Database
{
public:
    explicit Database(const std::string& databasePath);
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    bool isEmailProcessed(
        const std::string& gmailMessageId
    ) const;

    void saveAnalysis(
        const Email& email,
        const EmailAnalysis& analysis
    );

    std::vector<StoredEmailAnalysis>
    getRecentAnalyses(
        std::size_t limit = 10
    ) const;

    std::vector<StoredEmailAnalysis>
    getHighPriorityAnalyses(
        std::size_t limit = 10
    ) const;

private:
    void createTables();

    sqlite3* database_ = nullptr;
};