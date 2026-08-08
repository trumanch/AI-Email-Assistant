#include "Database.hpp"
#include "GmailClient.hpp"
#include "OAuthClient.hpp"
#include "OllamaClient.hpp"

#include <curl/curl.h>

#include <cstddef>
#include <exception>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace
{
void printNewAnalysis(
    const Email& email,
    const EmailAnalysis& analysis
)
{
    std::cout
        << "\n========================================\n"
        << "LOCAL AI EMAIL ANALYSIS\n"
        << "========================================\n"
        << "From: " << email.sender << '\n'
        << "Subject: " << email.subject << '\n'
        << "Category: " << analysis.category << '\n'
        << "Priority: " << analysis.priority << '\n'
        << "Action required: "
        << (analysis.actionRequired ? "Yes" : "No")
        << "\n\nSummary:\n"
        << analysis.summary
        << "\n\nRecommended action:\n"
        << analysis.recommendedAction
        << "\n\nSuggested reply:\n"
        << (
            analysis.suggestedReply.empty()
                ? "(No reply required)"
                : analysis.suggestedReply
        )
        << "\n\nResult saved to SQLite."
        << "\n========================================\n";
}

void printStoredAnalysis(
    const StoredEmailAnalysis& analysis
)
{
    std::cout
        << "\n========================================\n"
        << "SAVED EMAIL ANALYSIS #" << analysis.id
        << "\n========================================\n"
        << "From: " << analysis.sender << '\n'
        << "Subject: " << analysis.subject << '\n'
        << "Category: " << analysis.category << '\n'
        << "Priority: " << analysis.priority << '\n'
        << "Action required: "
        << (analysis.actionRequired ? "Yes" : "No")
        << "\nProcessed at: "
        << analysis.processedAt
        << "\n\nSummary:\n"
        << analysis.summary
        << "\n\nRecommended action:\n"
        << analysis.recommendedAction
        << "\n\nSuggested reply:\n"
        << (
            analysis.suggestedReply.empty()
                ? "(No reply required)"
                : analysis.suggestedReply
        )
        << "\n========================================\n";
}

void showSavedAnalyses(
    const std::vector<StoredEmailAnalysis>& analyses
)
{
    if (analyses.empty())
    {
        std::cout
            << "\nNo saved analyses were found.\n";

        return;
    }

    for (const StoredEmailAnalysis& analysis : analyses)
    {
        printStoredAnalysis(analysis);
    }
}

std::size_t readProcessingLimit()
{
    std::cout
        << "How many new emails should be analysed? "
        << "(1-10): ";

    std::string input;
    std::getline(std::cin, input);

    try
    {
        const unsigned long value =
            std::stoul(input);

        if (value >= 1 && value <= 10)
        {
            return static_cast<std::size_t>(value);
        }
    }
    catch (...)
    {
        // Default value will be returned below.
    }

    std::cout
        << "Invalid number. One email will be analysed.\n";

    return 1;
}

std::string removeQuotationMarks(std::string value)
{
    std::string cleaned;

    for (const char character : value)
    {
        if (character != '"')
        {
            cleaned.push_back(character);
        }
    }

    return cleaned;
}

void processEmails(
    GmailClient& gmailClient,
    OllamaClient& ollamaClient,
    Database& database,
    const std::string& query,
    std::size_t processingLimit
)
{
    std::cout
        << "\nSearching Gmail with query:\n"
        << query
        << "\n\n";

    // Fetch more than the processing limit because some messages
    // may already exist in SQLite.
    const std::vector<Email> emails =
        gmailClient.getMessages(query, 50);

    if (emails.empty())
    {
        std::cout
            << "No matching emails were found.\n";

        return;
    }

    std::size_t processedCount = 0;
    std::size_t skippedCount = 0;

    for (const Email& email : emails)
    {
        if (database.isEmailProcessed(email.id))
        {
            ++skippedCount;
            continue;
        }

        std::cout
            << "\nAnalysing: "
            << email.subject
            << "\nPlease wait while Ollama processes the email...\n";

        try
        {
            const EmailAnalysis analysis =
                ollamaClient.analyzeEmail(email);

            database.saveAnalysis(
                email,
                analysis
            );

            printNewAnalysis(
                email,
                analysis
            );

            ++processedCount;
        }
        catch (const std::exception& exception)
        {
            std::cerr
                << "\nCould not analyse email \""
                << email.subject
                << "\": "
                << exception.what()
                << '\n';
        }

        if (processedCount >= processingLimit)
        {
            break;
        }
    }

    if (processedCount == 0)
    {
        std::cout
            << "\nNo new emails were analysed.\n";

        if (skippedCount > 0)
        {
            std::cout
                << skippedCount
                << " matching email(s) had already "
                << "been processed.\n";
        }

        return;
    }

    std::cout
        << "\nFinished. Analysed "
        << processedCount
        << " new email(s).\n";

    if (skippedCount > 0)
    {
        std::cout
            << "Skipped "
            << skippedCount
            << " previously processed email(s).\n";
    }
}

void printMenu()
{
    std::cout
        << "\n\n========== AI EMAIL ASSISTANT ==========\n"
        << "1. Analyse unread emails\n"
        << "2. Analyse unread emails from a sender\n"
        << "3. Analyse unread emails by subject\n"
        << "4. Show recent analyses\n"
        << "5. Show high-priority emails\n"
        << "6. Exit\n"
        << "========================================\n"
        << "Choose an option: ";
}
}

int main()
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    if (
        curl_global_init(
            CURL_GLOBAL_DEFAULT
        ) != CURLE_OK
    )
    {
        std::cerr
            << "Failed to initialize CURL.\n";

        return 1;
    }

    int exitCode = 0;

    try
    {
        OAuthClient oauthClient(
            "credentials.json",
            "token.json"
        );

        const std::string accessToken =
            oauthClient.getAccessToken();

        GmailClient gmailClient(accessToken);

        OllamaClient ollamaClient(
            "qwen3:4b"
        );

        Database database(
            "data/ai_email_assistant.db"
        );

        bool applicationRunning = true;

        while (applicationRunning)
        {
            printMenu();

            std::string choice;
            std::getline(std::cin, choice);

            if (choice == "1")
            {
                const std::size_t limit =
                    readProcessingLimit();

                processEmails(
                    gmailClient,
                    ollamaClient,
                    database,
                    "is:unread",
                    limit
                );
            }
            else if (choice == "2")
            {
                std::cout
                    << "Enter sender email or domain: ";

                std::string sender;
                std::getline(std::cin, sender);

                if (sender.empty())
                {
                    std::cout
                        << "Sender cannot be empty.\n";

                    continue;
                }

                const std::size_t limit =
                    readProcessingLimit();

                processEmails(
                    gmailClient,
                    ollamaClient,
                    database,
                    "is:unread from:" + sender,
                    limit
                );
            }
            else if (choice == "3")
            {
                std::cout
                    << "Enter subject text: ";

                std::string subject;
                std::getline(std::cin, subject);

                subject =
                    removeQuotationMarks(subject);

                if (subject.empty())
                {
                    std::cout
                        << "Subject text cannot be empty.\n";

                    continue;
                }

                const std::size_t limit =
                    readProcessingLimit();

                processEmails(
                    gmailClient,
                    ollamaClient,
                    database,
                    "is:unread subject:\"" +
                        subject +
                        "\"",
                    limit
                );
            }
            else if (choice == "4")
            {
                showSavedAnalyses(
                    database.getRecentAnalyses(10)
                );
            }
            else if (choice == "5")
            {
                showSavedAnalyses(
                    database.getHighPriorityAnalyses(10)
                );
            }
            else if (choice == "6")
            {
                applicationRunning = false;

                std::cout
                    << "\nAI Email Assistant closed.\n";
            }
            else
            {
                std::cout
                    << "\nInvalid option. "
                    << "Choose a number from 1 to 6.\n";
            }
        }
    }
    catch (const std::exception& exception)
    {
        std::cerr
            << "\nError: "
            << exception.what()
            << '\n';

        exitCode = 1;
    }

    curl_global_cleanup();

    return exitCode;
}