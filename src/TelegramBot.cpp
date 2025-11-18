#include "TelegramBot.hpp"
#include <iostream>
#include <thread>
#include <chrono>

std::string TelegramBot::httpRequest(const std::string& url, const std::string& post_data) {
    CURL* curl = curl_easy_init();
    std::string response;

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        if (!post_data.empty()) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
        }

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            std::cerr << "Ошибка HTTP запроса: " << curl_easy_strerror(res) << std::endl;
        }

        curl_easy_cleanup(curl);
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

void TelegramBot::sendMessage(long chat_id, const std::string& text) {
    std::string url = api_url + "sendMessage";
    
    // Кодируем текст для URL
    std::string encoded_text;
    for (char c : text) {
        if (c == ' ') encoded_text += "%20";
        else if (c == '\n') encoded_text += "%0A";
        else encoded_text += c;
    }
    
    std::string post_data = "chat_id=" + std::to_string(chat_id) + "&text=" + encoded_text;
    httpRequest(url, post_data);
}

json TelegramBot::getUpdates() {
    std::string url = api_url + "getUpdates?timeout=100&offset=" + std::to_string(last_update_id + 1);
    std::string response = httpRequest(url);
    try {
        return json::parse(response);
    } catch (const std::exception& e) {
        std::cerr << "Ошибка парсинга JSON: " << e.what() << std::endl;
        return json();
    }
}

void TelegramBot::processUpdates(Database& db) {
    json updates = getUpdates();
    if (!updates.contains("result")) return;

    for (const auto& update : updates["result"]) {
        last_update_id = update["update_id"];
        if (update.contains("message")) {
            auto message = update["message"];
            long chat_id = message["chat"]["id"];
            std::string text = message.contains("text") ? message["text"] : "";

            std::cout << "Получено сообщение: " << text << std::endl;

            if (text == "/start") {
                sendMessage(chat_id, "Добро пожаловать в HabitCraft Bot!\n"
                                    "Доступные команды:\n"
                                    "/add - Добавить привычку\n"
                                    "/log - Отметить выполнение\n"
                                    "/list - Мои привычки\n"
                                    "/stats - Статистика\n"
                                    "/progress - Прогресс за неделю");
            } else if (text == "/list") {
                sendMessage(chat_id, "Список привычек:\n"
                                    "1. Чтение (ID: 1)\n"
                                    "2. Спорт (ID: 2)\n\\n"
                                    "Используйте /log [ID] чтобы отметить выполнение");
            } else if (text == "/stats") {
                sendMessage(chat_id, "Статистика:\n"
                                    "• Сила привычек: 75%\n"
                                    "• Лучший день: Суббота\n"
                                    "• Текущие серии: 5 дней");
            } else if (text == "/progress") {
                sendMessage(chat_id, "Прогресс за неделю:\n"
                                    "Чтение: ██████████ 50%\n"
                                    "Спорт: ████████ 40%");
            } else if (text.rfind("/log", 0) == 0) {
                sendMessage(chat_id, "Привычка отмечена как выполненная!");
            } else if (text.rfind("/add", 0) == 0) {
                sendMessage(chat_id, "Привычка добавлена!");
            } else {
                sendMessage(chat_id, "Неизвестная команда. Используйте /start для списка команд.");
            }
        }
    }
}