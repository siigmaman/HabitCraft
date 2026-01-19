#include "TelegramBot.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <sstream>
#include <curl/curl.h>
#include <iomanip>

void TelegramBot::setUserState(long chat_id, const std::string& state) {
    std::lock_guard<std::mutex> lock(state_mutex);
    user_states[chat_id] = state;
    state_timestamps[chat_id] = std::chrono::steady_clock::now();
    std::cout << "STATE: Set state for " << chat_id << " to '" << state << "'" << std::endl;
}

void TelegramBot::clearUserState(long chat_id) {
    std::lock_guard<std::mutex> lock(state_mutex);
    user_states.erase(chat_id);
    user_temp_data.erase(chat_id);
    std::cout << "STATE: Cleared ALL state data for " << chat_id << std::endl;
}

void TelegramBot::cleanupExpiredStates() {
    auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(state_mutex);
    
    for (auto it = user_states.begin(); it != user_states.end(); ) {
        auto timestamp_it = state_timestamps.find(it->first);
        if (timestamp_it != state_timestamps.end()) {
            auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(now - timestamp_it->second);
            if (elapsed.count() > 30) { 
                std::cout << "STATE: Auto-cleaning expired state for " << it->first << std::endl;
                user_temp_data.erase(it->first);
                state_timestamps.erase(it->first);
                it = user_states.erase(it);
                continue;
            }
        }
        ++it;
    }
}

std::string TelegramBot::getUserState(long chat_id) {
    std::lock_guard<std::mutex> lock(state_mutex);
    auto it = user_states.find(chat_id);
    return (it != user_states.end()) ? it->second : "none";
}

std::string TelegramBot::httpRequest(const std::string& url, const std::string& post_data) {
    CURL* curl = curl_easy_init();
    std::string response;

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "HabitCraftBot/1.0");
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
        curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, 120L);
        curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL, 60L);

        if (!post_data.empty()) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
        }

        CURLcode res = curl_easy_perform(curl);
        
        if (res != CURLE_OK) {
            std::cerr << "CURL error: " << curl_easy_strerror(res) << std::endl;
        } else {
            long http_code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        }

        curl_easy_cleanup(curl);
    } else {
        std::cerr << "Failed to initialize CURL" << std::endl;
    }
    
    return response;
}

size_t TelegramBot::writeCallback(void* contents, size_t size, size_t nmemb, std::string* response) {
    size_t total_size = size * nmemb;
    response->append((char*)contents, total_size);
    return total_size;
}

TelegramBot::TelegramBot(const std::string& bot_token) : token(bot_token) {
    api_url = "https://api.telegram.org/bot" + token + "/";
}

json TelegramBot::createMainKeyboard() {
    return {
        {"keyboard", {
            {
                {{"text", "Добавить привычку"}},
                {{"text", "Мои привычки"}}
            },
            {
                {{"text", "Статистика"}},
                {{"text", "Прогресс"}}
            },
            {
                {{"text", "Отметить выполнение"}},
                {{"text", "Удалить привычку"}}
            }
        }},
        {"resize_keyboard", true},
        {"one_time_keyboard", false}
    };
}

json TelegramBot::createHabitsKeyboard(Database& db, int userId) {
    json keyboard = json::array();
    json row = json::array();
    
    std::string habits_info = db.getUserHabitsForKeyboard(userId);
    std::stringstream ss(habits_info);
    std::string line;
    int count = 0;
    
    while (std::getline(ss, line)) {
        if (!line.empty()) {
            row.push_back({{"text", line}});
            count++;
            
            if (count % 2 == 0) {
                keyboard.push_back(row);
                row = json::array();
            }
        }
    }
    
    if (!row.empty()) {
        keyboard.push_back(row);
    }
    
    keyboard.push_back({{{"text", "Назад"}}});
    
    return {
        {"keyboard", keyboard},
        {"resize_keyboard", true},
        {"one_time_keyboard", true}
    };
}

json TelegramBot::createConfirmationKeyboard() {
    return {
        {"keyboard", {
            {
                {{"text", "Да, удалить"}},
                {{"text", "Нет, отменить"}}
            }
        }},
        {"resize_keyboard", true},
        {"one_time_keyboard", true}
    };
}

json TelegramBot::createDescriptionKeyboard() {
    return {
        {"keyboard", {
            {
                {{"text", "Пропустить"}},
                {{"text", "Назад"}}
            }
        }},
        {"resize_keyboard", true},
        {"one_time_keyboard", true}
    };
}

std::string urlEncode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (char c : value) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        }
        else if (c == ' ') {
            escaped << '+';
        }
        else {
            escaped << '%' << std::setw(2) << int((unsigned char)c);
        }
    }

    return escaped.str();
}

void TelegramBot::sendMessage(long chat_id, const std::string& text, const json& keyboard) {
    std::string url = api_url + "sendMessage";
    
    std::string encoded_text = urlEncode(text);
    
    std::string post_data = "chat_id=" + std::to_string(chat_id) + 
                           "&text=" + encoded_text;
    
    if (!keyboard.empty()) {
        post_data += "&reply_markup=" + keyboard.dump();
    }
    
    std::string response = httpRequest(url, post_data);

    if (response.find("\"ok\":true") == std::string::npos) {
        std::cerr << "ERROR: Failed to send message to " << chat_id << std::endl;
    }
}

json TelegramBot::getUpdates() {
    try {
        std::string url = api_url + "getUpdates?timeout=5&offset=" + std::to_string(last_update_id + 1);
        std::string response = httpRequest(url);
        
        if (response.empty()) {
            return json::object();
        }
        
        json result = json::parse(response);
        
        if (result.contains("ok") && result["ok"] == true) {
            auto updates = result["result"];
            if (!updates.empty()) {
                last_update_id = updates.back()["update_id"];
            }
            return result;
        } else {
            std::cerr << "Telegram API error: " << result.dump() << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error in getUpdates: " << e.what() << std::endl;
    }
    
    return json::object();
}

int TelegramBot::getOrCacheUserId(long chat_id, Database& db, const json& message) {
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        auto it = user_id_cache.find(chat_id);
        if (it != user_id_cache.end()) {
            return it->second;
        }
    }
    
    std::string username = "";
    std::string first_name = "";
    std::string last_name = "";
    
    if (message.contains("username")) {
        username = message["username"];
    }
    if (message.contains("first_name")) {
        first_name = message["first_name"];
    }
    if (message.contains("last_name")) {
        last_name = message["last_name"];
    }
    
    int user_id = db.getOrCreateUserByTelegramId(chat_id, username, first_name, last_name);
    
    if (user_id != -1) {
        std::lock_guard<std::mutex> lock(state_mutex);
        user_id_cache[chat_id] = user_id;
    }
    
    return user_id;
}

void TelegramBot::handleStart(long chat_id, int user_id) {
    clearUserState(chat_id);
    
    std::string welcome = 
        "HabitCraft Bot - трекер привычек\n\n"
        "Используйте кнопки ниже для управления привычками";
    
    sendMessage(chat_id, welcome, createMainKeyboard());
}

void TelegramBot::handleAddHabit(long chat_id, int user_id, const std::string& text, Database& db) {
    std::string current_state = getUserState(chat_id);

    if (text == "Добавить привычку") {
        setUserState(chat_id, "waiting_habit_name");
        sendMessage(chat_id, "Введите название новой привычки:");
        return;
    }

    if (current_state == "waiting_habit_name") {
        if (text == "Назад") {
            clearUserState(chat_id);
            sendMessage(chat_id, "Добавление привычки отменено", createMainKeyboard());
            return;
        }
        
        if (text.empty()) {
            sendMessage(chat_id, "Название привычки не может быть пустым. Попробуйте снова:");
            return;
        }

        {
            std::lock_guard<std::mutex> lock(state_mutex);
            user_temp_data[chat_id] = text;
        }
        setUserState(chat_id, "waiting_habit_frequency");
        sendMessage(chat_id, "Сколько раз в неделю вы планируете выполнять эту привычку? (1-7):");
        return;
    }

    if (current_state == "waiting_habit_frequency") {
        if (text == "Назад") {
            clearUserState(chat_id);
            sendMessage(chat_id, "Добавление привычки отменено", createMainKeyboard());
            return;
        }
        
        try {
            int frequency = std::stoi(text);
            if (frequency < 1 || frequency > 7) {
                sendMessage(chat_id, "Пожалуйста, введите число от 1 до 7:");
                return;
            }

            std::string habit_name;
            {
                std::lock_guard<std::mutex> lock(state_mutex);
                habit_name = user_temp_data[chat_id];
                user_temp_data[chat_id] = habit_name + "|" + std::to_string(frequency);
            }
            
            setUserState(chat_id, "waiting_habit_description");
            sendMessage(chat_id, "Введите описание привычки:");
            return;
            
        } catch (const std::exception& e) {
            sendMessage(chat_id, "Пожалуйста, введите число от 1 до 7:");
            return;
        }
    }

    if (current_state == "waiting_habit_description") {
        if (text == "Назад") {
            clearUserState(chat_id);
            sendMessage(chat_id, "Добавление привычки отменено", createMainKeyboard());
            return;
        }
        
        std::string habit_data;
        {
            std::lock_guard<std::mutex> lock(state_mutex);
            habit_data = user_temp_data[chat_id];
        }
        
        size_t separator = habit_data.find("|");
        if (separator == std::string::npos) {
            sendMessage(chat_id, "Ошибка при добавлении привычки.", createMainKeyboard());
            clearUserState(chat_id);
            return;
        }
        
        std::string habit_name = habit_data.substr(0, separator);
        int frequency = std::stoi(habit_data.substr(separator + 1));
        std::string description = text.empty() ? "Добавлено через бота" : text;
        
        try {
            db.addHabit(user_id, habit_name, description, frequency);
            clearUserState(chat_id);
            
            std::string message = "Привычка добавлена!\n\nНазвание: " + habit_name + 
                                 "\nЦель: " + std::to_string(frequency) + " раз/неделю" +
                                 "\nОписание: " + description;
            
            sendMessage(chat_id, message, createMainKeyboard());
            
        } catch (const std::exception& e) {
            std::cerr << "Ошибка при добавлении привычки: " << e.what() << std::endl;
            sendMessage(chat_id, "Ошибка при добавлении привычки.", createMainKeyboard());
            clearUserState(chat_id);
        }
    }
}

void TelegramBot::handleListHabits(long chat_id, int user_id, Database& db) {
    clearUserState(chat_id);
    std::string habits_list = db.getHabitsList(user_id);
    sendMessage(chat_id, habits_list, createMainKeyboard());
}

void TelegramBot::handleStats(long chat_id, int user_id, Database& db) {
    clearUserState(chat_id);
    std::stringstream stats;
    stats << "Статистика:\n\n";
    
    stats << db.getHabitStrength(user_id) << "\n\n";
    stats << db.getWeakestWeekday(user_id) << "\n\n";
    stats << db.getCurrentStreaks(user_id);
    
    sendMessage(chat_id, stats.str(), createMainKeyboard());
}

void TelegramBot::handleProgress(long chat_id, int user_id, Database& db) {
    clearUserState(chat_id);
    std::string progress = db.getProgressBars(user_id);
    sendMessage(chat_id, progress, createMainKeyboard());
}

void TelegramBot::handleLogHabit(long chat_id, int user_id, const std::string& text, Database& db) {
    std::string current_state = getUserState(chat_id);
    
    if (current_state == "waiting_habit_id") {
        try {
            size_t start = text.find("(ID: ");
            if (start != std::string::npos) {
                start += 5;
                size_t end = text.find(")", start);
                if (end != std::string::npos) {
                    std::string id_str = text.substr(start, end - start);
                    int habit_id = std::stoi(id_str);

                    auto now = std::chrono::system_clock::now();
                    std::time_t time = std::chrono::system_clock::to_time_t(now);
                    std::tm* tm = std::localtime(&time);
                    std::stringstream date_ss;
                    date_ss << (tm->tm_year + 1900) << "-" 
                           << std::setw(2) << std::setfill('0') << (tm->tm_mon + 1) << "-" 
                           << std::setw(2) << std::setfill('0') << tm->tm_mday;
                    
                    db.logHabitComplection(user_id, habit_id, date_ss.str(), "Выполнено через бота", 0);
                    clearUserState(chat_id);
                    sendMessage(chat_id, "Привычка отмечена как выполненная!", createMainKeyboard());
                    return;
                }
            }
            sendMessage(chat_id, "Ошибка: не удалось распознать ID привычки", createMainKeyboard());
        } catch (...) {
            sendMessage(chat_id, "Ошибка: неверный формат ID привычки", createMainKeyboard());
        }
        clearUserState(chat_id);
    } else {
        std::string habits_info = db.getUserHabitsForKeyboard(user_id);
        if (habits_info.empty()) {
            sendMessage(chat_id, "У вас нет активных привычек. Сначала добавьте привычку.", createMainKeyboard());
            return;
        }
        
        std::string message = "Выберите привычку для отметки:\n\n" + habits_info;
        setUserState(chat_id, "waiting_habit_id");
        sendMessage(chat_id, message, createHabitsKeyboard(db, user_id));
    }
}

void TelegramBot::handleDeleteHabit(long chat_id, int user_id, const std::string& text, Database& db) {
    std::string current_state = getUserState(chat_id);

    if (text == "Удалить привычку") {
        clearUserState(chat_id);
        
        std::string habits_info = db.getUserHabitsForKeyboard(user_id);
        if (habits_info.empty()) {
            sendMessage(chat_id, "У вас нет привычек для удаления.", createMainKeyboard());
            return;
        }
        
        setUserState(chat_id, "waiting_habit_to_delete");
        std::string message = "Выберите привычку для удаления:\n\n" + habits_info;
        sendMessage(chat_id, message, createHabitsKeyboard(db, user_id));
        return;
    }

    if (current_state == "waiting_habit_to_delete") {
        if (text == "Назад") {
            clearUserState(chat_id);
            sendMessage(chat_id, "Удаление отменено.", createMainKeyboard());
            return;
        }

        try {
            size_t id_start = text.find("(ID: ");
            if (id_start == std::string::npos) {
                sendMessage(chat_id, "Не удалось распознать привычку. Попробуйте снова:", createHabitsKeyboard(db, user_id));
                return;
            }
            
            id_start += 5;
            size_t id_end = text.find(")", id_start);
            if (id_end == std::string::npos) {
                sendMessage(chat_id, "Не удалось распознать привычку. Попробуйте снова:", createHabitsKeyboard(db, user_id));
                return;
            }
            
            std::string id_str = text.substr(id_start, id_end - id_start);

            std::string habit_name = text.substr(0, text.find("(ID:"));
            size_t last_char = habit_name.find_last_not_of(" \n\r\t");
            if (last_char != std::string::npos) {
                habit_name = habit_name.substr(0, last_char + 1);
            }

            {
                std::lock_guard<std::mutex> lock(state_mutex);
                user_temp_data[chat_id] = id_str + "|" + habit_name;
            }
            
            setUserState(chat_id, "confirming_deletion");
            std::string confirmation_message = 
                "Вы уверены, что хотите удалить привычку:\n\""
                + habit_name + "\"?\n\n"
                "Это действие нельзя отменить!";
            
            sendMessage(chat_id, confirmation_message, createConfirmationKeyboard());
            
        } catch (const std::exception& e) {
            std::cerr << "Ошибка при разборе привычки: " << e.what() << std::endl;
            sendMessage(chat_id, "Ошибка при выборе привычки. Попробуйте снова:", createHabitsKeyboard(db, user_id));
        }
        return;
    }

    if (current_state == "confirming_deletion") {
        if (text == "Да, удалить") {
            std::string habit_data;
            {
                std::lock_guard<std::mutex> lock(state_mutex);
                auto it = user_temp_data.find(chat_id);
                if (it == user_temp_data.end()) {
                    sendMessage(chat_id, "Ошибка: данные о привычке не найдены.", createMainKeyboard());
                    clearUserState(chat_id);
                    return;
                }
                habit_data = it->second;
            }
            
            try {
                size_t separator = habit_data.find("|");
                if (separator == std::string::npos) {
                    sendMessage(chat_id, "Ошибка: неверный формат данных привычки.", createMainKeyboard());
                    clearUserState(chat_id);
                    return;
                }
                
                std::string id_str = habit_data.substr(0, separator);
                std::string habit_name = habit_data.substr(separator + 1);
                
                int habit_id = std::stoi(id_str);
                
                db.deleteHabit(user_id, habit_id);

                clearUserState(chat_id);
                
                sendMessage(chat_id, "Привычка \"" + habit_name + "\" успешно удалена!", createMainKeyboard());
                
            } catch (const std::exception& e) {
                std::cerr << "Ошибка при удалении привычки: " << e.what() << std::endl;
                sendMessage(chat_id, "Ошибка при удалении привычки: " + std::string(e.what()), createMainKeyboard());
                clearUserState(chat_id);
            }
        } 
        else if (text == "Нет, отменить") {
            clearUserState(chat_id);
            sendMessage(chat_id, "Удаление отменено.", createMainKeyboard());
        }
        else {
            sendMessage(chat_id, "Пожалуйста, выберите вариант подтверждения:", createConfirmationKeyboard());
        }
    }
}

void TelegramBot::processUpdates(Database& db) {
    try {
        json updates = getUpdates();
        
        if (updates.empty() || !updates.contains("result")) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            return;
        }

        auto update_list = updates["result"];

        for (const auto& update : update_list) {
            try {
                if (!update.contains("update_id") || !update.contains("message")) {
                    continue;
                }

                long chat_id = update["message"]["chat"]["id"];
                std::string text = update["message"]["text"];
                
                int user_id = getOrCacheUserId(chat_id, db, update["message"]["chat"]);
                
                if (user_id == -1) {
                    std::cerr << "Не удалось получить/создать пользователя для chat_id: " << chat_id << std::endl;
                    continue;
                }

                if (text == "/start" || text == "Назад") {
                    handleStart(chat_id, user_id);
                }
                else if (text == "Добавить привычку") {
                    handleAddHabit(chat_id, user_id, text, db);
                }
                else if (text == "Мои привычки") {
                    handleListHabits(chat_id, user_id, db);
                }
                else if (text == "Статистика") {
                    handleStats(chat_id, user_id, db);
                }
                else if (text == "Прогресс") {
                    handleProgress(chat_id, user_id, db);
                }
                else if (text == "Отметить выполнение") {
                    handleLogHabit(chat_id, user_id, text, db);
                }
                else if (text == "Удалить привычку") {
                    handleDeleteHabit(chat_id, user_id, text, db);
                }
                else {
                    std::string state = getUserState(chat_id);
                    
                    if (state == "waiting_habit_name") {
                        handleAddHabit(chat_id, user_id, text, db);
                    }
                    else if (state == "waiting_habit_frequency") {
                        handleAddHabit(chat_id, user_id, text, db);
                    }
                    else if (state == "waiting_habit_description") {
                        handleAddHabit(chat_id, user_id, text, db);
                    }
                    else if (state == "waiting_habit_id") {
                        handleLogHabit(chat_id, user_id, text, db);
                    }
                    else if (state == "waiting_habit_to_delete" || state == "confirming_deletion") {
                        handleDeleteHabit(chat_id, user_id, text, db);
                    }
                    else {
                        sendMessage(chat_id, "Используйте кнопки для управления привычками", createMainKeyboard());
                    }
                }
                
            } catch (const std::exception& e) {
                std::cerr << "Error processing update: " << e.what() << std::endl;
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Critical error in processUpdates: " << e.what() << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}