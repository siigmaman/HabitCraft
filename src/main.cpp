#include "Database.hpp"
#include "TelegramBot.hpp"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    std::cout << "Запуск HabitCraft Telegram Bot!" << std::endl;

    std::string bot_token = "8179345923:AAFcuRfw9Ez9I1DYaZ0DX2FOc2T7RHXuCL8";

    Database db("host=127.0.0.1 port=5432 dbname=habitcraft user=habit_user password=my_secure_password");

    if (!db.isConnected()) {
        std::cerr << "Не удалось подключиться к базе данных" << std::endl;
        return 1;
    }

    TelegramBot bot(bot_token);

    std::cout << "Бот запущен! Ожидание сообщений..." << std::endl;

    while (true) {
        bot.processUpdates(db);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    return 0;
}