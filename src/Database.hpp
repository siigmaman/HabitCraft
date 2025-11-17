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

    void addUser(const std::string& username, const std::string& email);

    void addHabit(int user_id, const std::string& title,
                    const std::string& description = "", int targetFrequency = 7);

    void logHabitComplection(int habitId, const std::string& date, 
                            const std::string& notes = "", int rating = 0);

    void showProgressBars(int userId);

    void showHabitStrength(int userId);

    void showWeakestWeekday(int userId);

    void showCurrentStreaks(int userId);

    void listUserHabits(int userId);

    void deleteHabit(int habitId);

    ~Database();
};