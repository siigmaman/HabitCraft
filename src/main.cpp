#include "Database.hpp"
#include "TelegramBot.hpp"
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>

std::atomic<bool> stop(false);

void signalHandler(int signum) {
    std::cout << "Получен сигнал " << signum << ", завершение работы..." << std::endl;
    stop = true;
}

int main() {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    std::cout << "Запуск HabitCraft Telegram Bot" << std::endl;
    std::string bot_token = "8179345923:AAFcuRfw9Ez9I1DYaZ0DX2FOc2T7RHXuCL8";

    while (!stop) {
        try {
            std::cout << "Подключение к базе данных..." << std::endl;
            Database db("host=127.0.0.1 port=5432 dbname=habitcraft user=habit_user password=my_secure_password");

            if (!db.isConnected()) {
                std::cerr << "Не удалось подключиться к базе данных" << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(5));
                continue;
            }

            std::cout << "Подключение к базе данных успешно" << std::endl;
            
            TelegramBot bot(bot_token);
            std::cout << "Бот инициализирован, запуск цикла обновлений..." << std::endl;

            int error_count = 0;
            while (!stop && error_count < 10) {
                try {
                    bot.processUpdates(db);
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    error_count = 0;
                } catch (const std::exception& e) {
                    error_count++;
                    std::cerr << "Ошибка в цикле обновлений: " << e.what() << std::endl;
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
            }
            
        } catch (const std::exception& e) {
            std::cerr << "Критическая ошибка: " << e.what() << std::endl;
            std::cout << "Перезапуск через 5 секунд..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }

    std::cout << "Работа бота завершена" << std::endl;
    return 0;
}