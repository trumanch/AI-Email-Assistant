# AI Email Assistant

A terminal-based email automation application built with modern C++.

The application connects to Gmail, retrieves unread emails, analyses their content locally using Ollama, and stores structured results in an SQLite database.

The AI analysis runs on the user's computer, so email content is not sent to paid third-party AI services.

## Overview

AI Email Assistant helps users quickly understand and prioritise incoming emails.

For every processed email, the application can generate:

- Email category
- Priority level
- Short summary
- Recommended action
- Suggested reply
- Action-required status

Previously processed emails are stored in SQLite and are not analysed repeatedly.

## Features

- Google OAuth 2.0 authentication
- Gmail API integration
- Retrieval of unread Gmail messages
- Search by sender
- Search by subject
- Local email analysis with Ollama
- Email categorisation
- Priority detection
- Automatic summaries
- Recommended next actions
- Suggested email replies
- SQLite data storage
- Duplicate-processing prevention
- High-priority email filtering
- Terminal-based interface
- No paid AI API required
- Read-only Gmail access

## Example Output

```text
========================================
LOCAL AI EMAIL ANALYSIS
========================================
From: Recruitment Team <jobs@example.com>
Subject: Interview Invitation
Category: job
Priority: high
Action required: Yes

Summary:
The recruiter would like to arrange an interview next Tuesday.

Recommended action:
Confirm availability and ask whether the interview will be online or in person.

Suggested reply:
Hello,

Thank you for the invitation. I would be happy to attend the interview
next Tuesday. Could you please confirm the time and interview format?

Kind regards,
Nazar
========================================
```

## Application Menu

```text
========== AI EMAIL ASSISTANT ==========
1. Analyse unread emails
2. Analyse unread emails from a sender
3. Analyse unread emails by subject
4. Show recent analyses
5. Show high-priority emails
6. Exit
========================================
```

## Technologies

- C++20
- CMake
- Gmail REST API
- Google OAuth 2.0
- Ollama
- Qwen 3
- SQLite
- libcurl
- nlohmann/json
- OpenSSL
- cpp-httplib
- vcpkg

## Project Structure

```text
AIEmailAssistant/
├── include/
│   ├── Database.hpp
│   ├── Email.hpp
│   ├── EmailAnalysis.hpp
│   ├── GmailClient.hpp
│   ├── OAuthClient.hpp
│   └── OllamaClient.hpp
│
├── src/
│   ├── Database.cpp
│   ├── GmailClient.cpp
│   ├── main.cpp
│   ├── OAuthClient.cpp
│   └── OllamaClient.cpp
│
├── data/
├── CMakeLists.txt
├── .gitignore
└── README.md
```

## How It Works

1. The user signs in through Google OAuth 2.0.
2. The application receives a temporary Gmail access token.
3. Gmail API returns emails matching the selected search query.
4. The application extracts the sender, subject, date, and message body.
5. Ollama analyses the email locally.
6. The result is converted into structured data.
7. SQLite stores the analysis and Gmail message ID.
8. Previously processed messages are skipped during future runs.

## Requirements

This project was developed and tested on Windows.

Required software:

- Windows 10 or Windows 11
- Visual Studio C++ Build Tools
- CMake
- Git
- vcpkg
- Ollama
- A Google account
- Google Cloud OAuth credentials

## Install Dependencies

Install the required C++ libraries using vcpkg:

```powershell
C:\vcpkg\vcpkg.exe install curl:x64-windows
C:\vcpkg\vcpkg.exe install nlohmann-json:x64-windows
C:\vcpkg\vcpkg.exe install cpp-httplib:x64-windows
C:\vcpkg\vcpkg.exe install openssl:x64-windows
C:\vcpkg\vcpkg.exe install sqlite3:x64-windows
```

## Install Ollama

Install Ollama for Windows and download the local model:

```powershell
ollama pull qwen3:4b
```

Test that the model is available:

```powershell
ollama run qwen3:4b
```

Ollama must be running while the application analyses emails.

## Google OAuth Setup

### 1. Create a Google Cloud project

Create a project in Google Cloud Console.

### 2. Enable Gmail API

Open the API Library and enable:

```text
Gmail API
```

### 3. Configure the OAuth consent screen

Create an external OAuth application and add your Google account as a test user.

### 4. Add the Gmail scope

The application only requires read-only access:

```text
https://www.googleapis.com/auth/gmail.readonly
```

### 5. Create OAuth credentials

Create an OAuth client with the following application type:

```text
Desktop app
```

Download the generated JSON file and rename it to:

```text
credentials.json
```

Place it in the project root:

```text
AIEmailAssistant/
└── credentials.json
```

The application will create `token.json` after the first successful authorisation.

## Security

The following files contain private information and must never be committed to GitHub:

```text
credentials.json
token.json
data/ai_email_assistant.db
```

They are excluded through `.gitignore`.

The application:

- Does not store Gmail passwords
- Uses Google OAuth 2.0
- Requests read-only Gmail access
- Does not send emails automatically
- Does not delete or modify Gmail messages
- Does not send email content to a paid AI API
- Runs AI analysis locally through Ollama

## Build

Open PowerShell in the project directory:

```powershell
cd C:\Users\YourName\Desktop\AIEmailAssistant
```

Configure the project:

```powershell
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
```

Build the application:

```powershell
cmake --build build --config Debug
```

## Run

```powershell
.\build\Debug\ai_email_assistant.exe
```

During the first run, the application will open Google authorisation in the browser.

After successful authorisation, the OAuth tokens will be stored locally in `token.json`.

## Database

The application creates the following database automatically:

```text
data/ai_email_assistant.db
```

For each processed email, it stores:

- Gmail message ID
- Gmail thread ID
- Sender
- Subject
- Received date
- Category
- Priority
- Summary
- Action-required status
- Recommended action
- Suggested reply
- Processing date

The complete email body is not stored in the database.

## Gmail Search Examples

The application supports Gmail search queries such as:

```text
is:unread
```

```text
is:unread from:recruiter@example.com
```

```text
is:unread subject:"interview"
```

## Current Limitations

- Designed primarily for Windows
- Requires Ollama to run locally
- Processes text email content only
- HTML email cleaning is basic
- Attachments are not analysed
- Gmail messages are not automatically marked as read
- The application does not send emails

## Possible Future Improvements

- Unit and integration tests
- Attachment analysis
- Better HTML-to-text conversion
- Encrypted OAuth token storage
- Automatic Gmail labels
- Multiple Gmail account support
- Additional local Ollama models
- Background email monitoring
- Cross-platform Linux and macOS support

## Author

**Nazar Turenko**

This project was created as a practical demonstration of:

- Modern C++ development
- REST API integration
- OAuth 2.0 authentication
- Local AI integration
- JSON processing
- SQLite database management
- Secure handling of credentials
- CMake and dependency management