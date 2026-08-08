#include "Database.hpp"

#include <sqlite3.h>

#include <filesystem>
#include <stdexcept>
#include <string>

namespace
{
class Statement
{
public:
    Statement(
        sqlite3* database,
        const char* sql
    )
        : database_(database)
    {
        const int result = sqlite3_prepare_v2(
            database_,
            sql,
            -1,
            &statement_,
            nullptr
        );

        if (result != SQLITE_OK)
        {
            throw std::runtime_error(
                "Could not prepare SQL statement: " +
                std::string(sqlite3_errmsg(database_))
            );
        }
    }

    ~Statement()
    {
        if (statement_ != nullptr)
        {
            sqlite3_finalize(statement_);
        }
    }

    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    sqlite3_stmt* get() const
    {
        return statement_;
    }

private:
    sqlite3* database_ = nullptr;
    sqlite3_stmt* statement_ = nullptr;
};

void bindText(
    sqlite3* database,
    sqlite3_stmt* statement,
    int parameterIndex,
    const std::string& value
)
{
    const int result = sqlite3_bind_text(
        statement,
        parameterIndex,
        value.c_str(),
        static_cast<int>(value.size()),
        SQLITE_TRANSIENT
    );

    if (result != SQLITE_OK)
    {
        throw std::runtime_error(
            "Could not bind SQL text value: " +
            std::string(sqlite3_errmsg(database))
        );
    }
}

void bindInteger(
    sqlite3* database,
    sqlite3_stmt* statement,
    int parameterIndex,
    int value
)
{
    const int result = sqlite3_bind_int(
        statement,
        parameterIndex,
        value
    );

    if (result != SQLITE_OK)
    {
        throw std::runtime_error(
            "Could not bind SQL integer value: " +
            std::string(sqlite3_errmsg(database))
        );
    }
}
}

Database::Database(
    const std::string& databasePath
)
{
    const std::filesystem::path path(databasePath);

    if (path.has_parent_path())
    {
        std::filesystem::create_directories(
            path.parent_path()
        );
    }

    const int result = sqlite3_open(
        databasePath.c_str(),
        &database_
    );

    if (result != SQLITE_OK)
    {
        const std::string errorMessage =
            database_ != nullptr
                ? sqlite3_errmsg(database_)
                : "Unknown SQLite error";

        if (database_ != nullptr)
        {
            sqlite3_close(database_);
            database_ = nullptr;
        }

        throw std::runtime_error(
            "Could not open SQLite database: " +
            errorMessage
        );
    }

    createTables();
}

Database::~Database()
{
    if (database_ != nullptr)
    {
        sqlite3_close(database_);
    }
}

void Database::createTables()
{
    const char* sql = R"sql(
        CREATE TABLE IF NOT EXISTS email_analyses
        (
            id INTEGER PRIMARY KEY AUTOINCREMENT,

            gmail_message_id TEXT NOT NULL UNIQUE,
            thread_id TEXT,

            sender TEXT NOT NULL,
            subject TEXT,
            received_at TEXT,

            category TEXT NOT NULL,
            priority TEXT NOT NULL,
            summary TEXT NOT NULL,

            action_required INTEGER NOT NULL,
            recommended_action TEXT,
            suggested_reply TEXT,

            processed_at TEXT NOT NULL
                DEFAULT CURRENT_TIMESTAMP
        );
    )sql";

    char* errorMessage = nullptr;

    const int result = sqlite3_exec(
        database_,
        sql,
        nullptr,
        nullptr,
        &errorMessage
    );

    if (result != SQLITE_OK)
    {
        const std::string message =
            errorMessage != nullptr
                ? errorMessage
                : "Unknown SQLite error";

        sqlite3_free(errorMessage);

        throw std::runtime_error(
            "Could not create database table: " +
            message
        );
    }
}

bool Database::isEmailProcessed(
    const std::string& gmailMessageId
) const
{
    const char* sql = R"sql(
        SELECT 1
        FROM email_analyses
        WHERE gmail_message_id = ?
        LIMIT 1;
    )sql";

    Statement statement(database_, sql);

    bindText(
        database_,
        statement.get(),
        1,
        gmailMessageId
    );

    const int result =
        sqlite3_step(statement.get());

    if (result == SQLITE_ROW)
    {
        return true;
    }

    if (result == SQLITE_DONE)
    {
        return false;
    }

    throw std::runtime_error(
        "Could not check processed email: " +
        std::string(sqlite3_errmsg(database_))
    );
}

void Database::saveAnalysis(
    const Email& email,
    const EmailAnalysis& analysis
)
{
    const char* sql = R"sql(
        INSERT INTO email_analyses
        (
            gmail_message_id,
            thread_id,
            sender,
            subject,
            received_at,
            category,
            priority,
            summary,
            action_required,
            recommended_action,
            suggested_reply
        )
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
    )sql";

    Statement statement(database_, sql);

    bindText(
        database_,
        statement.get(),
        1,
        email.id
    );

    bindText(
        database_,
        statement.get(),
        2,
        email.threadId
    );

    bindText(
        database_,
        statement.get(),
        3,
        email.sender
    );

    bindText(
        database_,
        statement.get(),
        4,
        email.subject
    );

    bindText(
        database_,
        statement.get(),
        5,
        email.date
    );

    bindText(
        database_,
        statement.get(),
        6,
        analysis.category
    );

    bindText(
        database_,
        statement.get(),
        7,
        analysis.priority
    );

    bindText(
        database_,
        statement.get(),
        8,
        analysis.summary
    );

    bindInteger(
        database_,
        statement.get(),
        9,
        analysis.actionRequired ? 1 : 0
    );

    bindText(
        database_,
        statement.get(),
        10,
        analysis.recommendedAction
    );

    bindText(
        database_,
        statement.get(),
        11,
        analysis.suggestedReply
    );

    const int result =
        sqlite3_step(statement.get());

    if (result != SQLITE_DONE)
    {
        throw std::runtime_error(
            "Could not save email analysis: " +
            std::string(sqlite3_errmsg(database_))
        );
    }
}
namespace
{
std::string readColumnText(
    sqlite3_stmt* statement,
    int columnIndex
)
{
    const unsigned char* value =
        sqlite3_column_text(
            statement,
            columnIndex
        );

    if (value == nullptr)
    {
        return {};
    }

    return reinterpret_cast<const char*>(value);
}

StoredEmailAnalysis readStoredAnalysis(
    sqlite3_stmt* statement
)
{
    StoredEmailAnalysis analysis;

    analysis.id =
        sqlite3_column_int(statement, 0);

    analysis.gmailMessageId =
        readColumnText(statement, 1);

    analysis.sender =
        readColumnText(statement, 2);

    analysis.subject =
        readColumnText(statement, 3);

    analysis.category =
        readColumnText(statement, 4);

    analysis.priority =
        readColumnText(statement, 5);

    analysis.summary =
        readColumnText(statement, 6);

    analysis.actionRequired =
        sqlite3_column_int(statement, 7) != 0;

    analysis.recommendedAction =
        readColumnText(statement, 8);

    analysis.suggestedReply =
        readColumnText(statement, 9);

    analysis.processedAt =
        readColumnText(statement, 10);

    return analysis;
}
}

std::vector<StoredEmailAnalysis>
Database::getRecentAnalyses(
    std::size_t limit
) const
{
    const char* sql = R"sql(
        SELECT
            id,
            gmail_message_id,
            sender,
            subject,
            category,
            priority,
            summary,
            action_required,
            recommended_action,
            suggested_reply,
            processed_at
        FROM email_analyses
        ORDER BY id DESC
        LIMIT ?;
    )sql";

    Statement statement(database_, sql);

    const int safeLimit =
        static_cast<int>(
            limit > 100 ? 100 : limit
        );

    const int bindResult =
        sqlite3_bind_int(
            statement.get(),
            1,
            safeLimit
        );

    if (bindResult != SQLITE_OK)
    {
        throw std::runtime_error(
            "Could not bind analysis limit: " +
            std::string(sqlite3_errmsg(database_))
        );
    }

    std::vector<StoredEmailAnalysis> analyses;

    while (true)
    {
        const int result =
            sqlite3_step(statement.get());

        if (result == SQLITE_ROW)
        {
            analyses.push_back(
                readStoredAnalysis(statement.get())
            );

            continue;
        }

        if (result == SQLITE_DONE)
        {
            break;
        }

        throw std::runtime_error(
            "Could not load saved analyses: " +
            std::string(sqlite3_errmsg(database_))
        );
    }

    return analyses;
}

std::vector<StoredEmailAnalysis>
Database::getHighPriorityAnalyses(
    std::size_t limit
) const
{
    const char* sql = R"sql(
        SELECT
            id,
            gmail_message_id,
            sender,
            subject,
            category,
            priority,
            summary,
            action_required,
            recommended_action,
            suggested_reply,
            processed_at
        FROM email_analyses
        WHERE priority = 'high'
        ORDER BY id DESC
        LIMIT ?;
    )sql";

    Statement statement(database_, sql);

    const int safeLimit =
        static_cast<int>(
            limit > 100 ? 100 : limit
        );

    const int bindResult =
        sqlite3_bind_int(
            statement.get(),
            1,
            safeLimit
        );

    if (bindResult != SQLITE_OK)
    {
        throw std::runtime_error(
            "Could not bind analysis limit: " +
            std::string(sqlite3_errmsg(database_))
        );
    }

    std::vector<StoredEmailAnalysis> analyses;

    while (true)
    {
        const int result =
            sqlite3_step(statement.get());

        if (result == SQLITE_ROW)
        {
            analyses.push_back(
                readStoredAnalysis(statement.get())
            );

            continue;
        }

        if (result == SQLITE_DONE)
        {
            break;
        }

        throw std::runtime_error(
            "Could not load high-priority analyses: " +
            std::string(sqlite3_errmsg(database_))
        );
    }

    return analyses;
}