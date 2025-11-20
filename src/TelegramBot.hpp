#pragma once
#include "Database.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <string>
#include <map>
#include <mutex>
#include <chrono>

using json = nlohmann::json;

class TelegramBot {
private:
    std::string token;
    std::string api_url;
    long long last_update_id = 0;
    
    std::map<long, std::string> user_states;
    std::map<long, std::string> user_temp_data;
    std::map<long, std::chrono::steady_clock::time_point> state_timestamps;
    std::mutex state_mutex;

    std::string httpRequest(const std::string& url, const std::string& post_data = "");
    static size_t writeCallback(void* contents, size_t size, size_t nmemb, std::string* response);
    
    json createMainKeyboard();
    json createHabitsKeyboard(Database& db, int userId);
    json createConfirmationKeyboard();
    json createDescriptionKeyboard();

    void setUserState(long chat_id, const std::string& state);
    void clearUserState(long chat_id);
    std::string getUserState(long chat_id);

public:
    TelegramBot(const std::string& bot_token);
    void sendMessage(long chat_id, const std::string& text, const json& keyboard = json::object());
    json getUpdates();
    void processUpdates(Database& db);
    
    void cleanupExpiredStates();  
    
    void handleStart(long chat_id);
    void handleAddHabit(long chat_id, const std::string& text, Database& db);
    void handleListHabits(long chat_id, Database& db);
    void handleStats(long chat_id, Database& db);
    void handleProgress(long chat_id, Database& db);
    void handleLogHabit(long chat_id, const std::string& text, Database& db);
    void handleDeleteHabit(long chat_id, const std::string& text, Database& db);
};