cmake_minimum_required(VERSION 3.20)

project(
    AIEmailAssistant
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(
    nlohmann_json
    CONFIG
    REQUIRED
)

find_package(
    CURL
    CONFIG
    REQUIRED
)

find_package(
    httplib
    CONFIG
    REQUIRED
)

find_package(
    OpenSSL
    REQUIRED
)

find_package(
    unofficial-sqlite3
    CONFIG
    REQUIRED
)

add_executable(
    ai_email_assistant
    src/main.cpp
    src/OAuthClient.cpp
    src/GmailClient.cpp
    src/OllamaClient.cpp
    src/Database.cpp
)

target_include_directories(
    ai_email_assistant
    PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(
    ai_email_assistant
    PRIVATE
    nlohmann_json::nlohmann_json
    CURL::libcurl
    httplib::httplib
    OpenSSL::Crypto
    unofficial::sqlite3::sqlite3
)

if(WIN32)
    target_link_libraries(
        ai_email_assistant
        PRIVATE
        shell32
        ws2_32
    )
endif()

if(MSVC)
    target_compile_options(
        ai_email_assistant
        PRIVATE
        /W4
        /utf-8
    )
else()
    target_compile_options(
        ai_email_assistant
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()