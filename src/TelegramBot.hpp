#pragma once
#include "Database.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

class TelegramBot {
private:
    std::string token;
    std::string api_url;
    long long last_update_id = 0;

    std::string httpRequest(const std::string& url, const std::string& post_data = "");
    static size_t writeCallback(void* contents, size_t size, size_t nmemb, std::string* response);

public:
    TelegramBot(const std::string& bot_token);
    void sendMessage(long chat_id, const std::string& text);
    json getUpdates();
    void processUpdates(Database& db);
};