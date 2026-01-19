#pragma once
#include <pqxx/pqxx>
#include <string>
#include <iostream>

class Database {
private:
    std::unique_ptr<pqxx::connection> conn;

public:
    Database(const std::string& connectionString);

    bool isConnected() const;

    void createTables(const std::string& sqlFilePath);

    int getOrCreateUserByTelegramId(long telegram_id, 
                                   const std::string& username = "", 
                                   const std::string& first_name = "", 
                                   const std::string& last_name = "");

    int getUserByTelegramId(long telegram_id);

    void addUser(const std::string& username, const std::string& email);

    void addHabit(int userId, const std::string& title,
                  const std::string& description = "", int targetFrequency = 7);

    void logHabitComplection(int userId, int habitId, const std::string& date, 
                            const std::string& notes = "", int rating = 0);

    std::string getProgressBars(int userId);
    std::string getHabitStrength(int userId);
    std::string getWeakestWeekday(int userId);
    std::string getCurrentStreaks(int userId);
    std::string getHabitsList(int userId);
    std::string getUserHabitsForKeyboard(int userId);

    void deleteHabit(int userId, int habitId);

    ~Database();
};