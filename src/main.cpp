#include "Database.hpp"
#include <iostream>
#include <string>

void showMainMenu() {
    std::cout << "\n HABIT CRAFT - Трекер привычек" << std::endl;
    std::cout << "==================================" << std::endl;
    std::cout << "1. Добавить привычку" << std::endl;
    std::cout << "2. Отметить выполнение привычки" << std::endl;
    std::cout << "3. Показать всю аналитику" << std::endl;
    std::cout << "4. Показать прогресс за неделю" << std::endl;
    std::cout << "5. Выйти" << std::endl;
    std::cout << "Выберите опцию: ";
}

void addHabitInteractive(Database& db) {
    std::string title, description;
    int targetFrequency;
    
    std::cout << "\n ДОБАВЛЕНИЕ ПРИВЫЧКИ" << std::endl;
    std::cout << "Название привычки: ";
    std::cin.ignore();
    std::getline(std::cin, title);
    
    std::cout << "Описание: ";
    std::getline(std::cin, description);
    
    std::cout << "Цель (раз в неделю): ";
    std::cin >> targetFrequency;
    
    db.addHabit(1, title, description, targetFrequency);
}

void logHabitInteractive(Database& db) {
    int habitId, rating;
    std::string notes, date;
    
    std::cout << "\n ОТМЕТИТЬ ВЫПОЛНЕНИЕ" << std::endl;
    std::cout << "ID привычки: ";
    std::cin >> habitId;
    
    std::cout << "Дата (ГГГГ-ММ-ДД): ";
    std::cin.ignore();
    std::getline(std::cin, date);
    
    std::cout << "Заметки: ";
    std::getline(std::cin, notes);
    
    std::cout << "Оценка (1-5, 0 если без оценки): ";
    std::cin >> rating;
    
    db.logHabitComplection(habitId, date, notes, rating);
}

void showAnalytics(Database& db) {
    std::cout << "\n ЗАГРУЗКА АНАЛИТИКИ..." << std::endl;
    db.showHabitStrength(1);
    db.showWeakestWeekday(1);
    db.showCurrentStreaks(1);
}

void showProgress(Database& db) {
    db.showProgressBars(1);
}

int main() {
    std::cout << "Запуск HabitCraft!" << std::endl;
    
    std::string connection_string = 
        "host=127.0.0.1 "
        "port=5432 "
        "dbname=habitcraft "
        "user=habit_user "
        "password=my_secure_password";
    
    Database db(connection_string);
    
    if (!db.isConnected()) {
        std::cerr << "Не удалось подключиться к базе данных. Выход..." << std::endl;
        return 1;
    }
    
    std::cout << "Успешное подключение к базе данных!" << std::endl;
    
    std::cout << "\n Проверка таблиц..." << std::endl;
    db.createTables("../sql/init_db.sql");
    
    int choice;
    bool running = true;
    
    while (running) {
        showMainMenu();
        std::cin >> choice;
        
        switch (choice) {
            case 1:
                addHabitInteractive(db);
                break;
            case 2:
                logHabitInteractive(db);
                break;
            case 3:
                showAnalytics(db);
                break;
            case 4:
                showProgress(db);
                break;
            case 5:
                running = false;
                std::cout << "\n До свидания!" << std::endl;
                break;
            default:
                std::cout << "Неверный выбор! Попробуйте снова." << std::endl;
        }
    }
    
    return 0;
}