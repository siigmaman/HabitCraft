#include "Database.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>

Database::Database(const std::string& connectionString) {
    try {
        conn = std::make_unique<pqxx::connection>(connectionString);
        std::cout << "Подключение к базе данных установлено!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Ошибка подключения: " << e.what() << std::endl;
    }
}

bool Database::isConnected() const {
    return conn && conn->is_open();
}

void Database::createTables(const std::string& sqlFilePath) {
    if (!isConnected()) {
        std::cerr << "Нет подключения к БД" << std::endl;
        return;
    }

    try {
        std::ifstream file(sqlFilePath);
        if (!file.is_open()) {
            std::cerr << "Не получается открыть файл: " << sqlFilePath << std::endl; 
            return;  
        }
        
        std::stringstream sqlStream;
        sqlStream << file.rdbuf();
        std::string sql = sqlStream.str();

        pqxx::work txn(*conn);
        txn.exec(sql);
        txn.commit();

        std::cout << "Таблицы успешно созданы!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Ошибка создания таблиц: " << e.what() << std::endl;
    }
}

void Database::addUser(const std::string& username, const std::string& email) {
    if (!isConnected()) return;

    try {
        pqxx::work txn(*conn);
        txn.exec_params(
            "INSERT INTO users (username, email) VALUES ($1, $2)",
            username, email
        );
        txn.commit();
    } catch (const std::exception& e) {
        std::cerr << "Ошибка добавления пользователя: " << e.what() << std::endl;
    }
}

void Database::addHabit(int userId, const std::string& title,
                       const std::string& description, int targetFrequency) {
    if (!isConnected()) return;

    try {
        pqxx::work txn(*conn);
        txn.exec_params(
            "INSERT INTO habits (user_id, title, description, target_frequency) VALUES ($1, $2, $3, $4)",
            userId, title, description, targetFrequency
        );
        txn.commit();
    } catch (const std::exception& e) {
        std::cerr << "Ошибка добавления привычки: " << e.what() << std::endl;
    }
}

void Database::logHabitComplection(int habitId, const std::string& date, 
                                  const std::string& notes, int rating) {
    if (!isConnected()) return;

    try {
        pqxx::work txn(*conn);
        
        if (rating > 0) {
            txn.exec_params(
                "INSERT INTO habit_logs (habit_id, completed_date, notes, rating) VALUES ($1, $2, $3, $4)",
                habitId, date, notes, rating
            );
        } else {
            txn.exec_params(
                "INSERT INTO habit_logs (habit_id, completed_date, notes) VALUES ($1, $2, $3)",
                habitId, date, notes
            );
        }
        
        txn.commit();
    } catch (const std::exception& e) {
        std::cerr << "Ошибка логирования привычки: " << e.what() << std::endl;
    }
}

std::string Database::getProgressBars(int userId) {
    if (!isConnected()) return "Ошибка подключения к базе данных";

    try {
        pqxx::work txn(*conn);
        pqxx::result result = txn.exec_params(R"(
            SELECT 
                h.habit_id,
                h.title, 
                COUNT(hl.completed_date) as completed,
                h.target_frequency as target
            FROM habits h
            LEFT JOIN habit_logs hl ON h.habit_id = hl.habit_id 
                AND hl.completed_date >= CURRENT_DATE - INTERVAL '7 days'
            WHERE h.user_id = $1 AND h.is_active = true
            GROUP BY h.habit_id, h.title, h.target_frequency
            ORDER BY h.habit_id
        )", userId);
        
        std::stringstream output;
        output << "Прогресс за неделю:\n\n";
        
        if (result.empty()) {
            output << "Нет активных привычек";
            return output.str();
        }
        
        for (const auto& row : result) {
            int completed = row["completed"].as<int>();
            int target = row["target"].as<int>();
            int percent = target > 0 ? (completed * 100) / target : 0;
            
            output << row["title"].c_str() << ":\n";
            output << "[";
            
            int bars = (percent * 20) / 100;
            for (int i = 0; i < 20; i++) {
                if (i < bars) output << "█";
                else output << "░";
            }
            
            output << "] " << completed << "/" << target << " (" << percent << "%)\n\n";
        }
        
        return output.str();
        
    } catch (const std::exception& e) {
        return "Ошибка получения прогресса: " + std::string(e.what());
    }
}

std::string Database::getHabitStrength(int userId) {
    if (!isConnected()) return "Ошибка подключения к базе данных";

    try {
        pqxx::work txn(*conn);
        pqxx::result result = txn.exec_params(R"(
            SELECT 
                h.habit_id,
                h.title,
                COUNT(hl.completed_date) as completed_days,
                ROUND(
                    (COUNT(hl.completed_date) * 100.0 / 
                    NULLIF(GREATEST(30, h.target_frequency), 0)
                ), 1) as habit_strength_percent
            FROM habits h
            LEFT JOIN habit_logs hl ON h.habit_id = hl.habit_id
                AND hl.completed_date >= CURRENT_DATE - INTERVAL '30 days'
            WHERE h.user_id = $1 AND h.is_active = true
            GROUP BY h.habit_id, h.title, h.target_frequency
            ORDER BY habit_strength_percent DESC
        )", userId);

        std::stringstream output;
        output << "Сила привычек (% за 30 дней):\n\n";

        if (result.empty()) {
            output << "Нет данных для анализа";
            return output.str();
        }

        for (const auto& row : result) {
            output << row["title"].c_str() << ": "
                   << row["habit_strength_percent"].c_str() << "%"
                   << " (" << row["completed_days"].c_str() << " дней)\n";
        }

        return output.str();

    } catch (const std::exception& e) {
        return "Ошибка анализа привычек: " + std::string(e.what());
    }
}

std::string Database::getWeakestWeekday(int userId) {
    if (!isConnected()) return "Ошибка подключения к базе данных";

    try {
        pqxx::work txn(*conn);
        
        pqxx::result result = txn.exec_params(R"(
            WITH date_series AS (
                SELECT GENERATE_SERIES(
                    CURRENT_DATE - INTERVAL '30 days',
                    CURRENT_DATE,
                    '1 day'::INTERVAL
                )::DATE as day
            ),
            user_habits AS (
                SELECT habit_id FROM habits WHERE user_id = $1 AND is_active = true
            ),
            all_days AS (
                SELECT 
                    uh.habit_id,
                    ds.day,
                    EXTRACT(DOW FROM ds.day) as day_of_week
                FROM user_habits uh
                CROSS JOIN date_series ds
            ),
            completion_status AS (
                SELECT
                    ad.habit_id,
                    ad.day_of_week,
                    CASE WHEN hl.completed_date IS NOT NULL THEN 1 ELSE 0 END as was_completed
                FROM all_days ad
                LEFT JOIN habit_logs hl ON ad.habit_id = hl.habit_id AND ad.day = hl.completed_date
            )
            SELECT 
                day_of_week,
                ROUND(AVG(was_completed) * 100, 1) as success_rate_percent
            FROM completion_status
            GROUP BY day_of_week
            ORDER BY success_rate_percent ASC
            LIMIT 1
        )", userId);
        
        std::stringstream output;
        
        if (!result.empty()) {
            int worstDay = result[0]["day_of_week"].as<int>();
            double successRate = result[0]["success_rate_percent"].as<double>();
            
            std::string days[] = {"Воскресенье", "Понедельник", "Вторник", "Среда", 
                                 "Четверг", "Пятница", "Суббота"};
            
            output << "Самый слабый день:\n";
            output << days[worstDay] << ": " << successRate << "% успеха";
        } else {
            output << "Недостаточно данных для анализа дней недели";
        }
        
        return output.str();
        
    } catch (const std::exception& e) {
        return "Ошибка анализа дней: " + std::string(e.what());
    }
}

std::string Database::getCurrentStreaks(int userId) {
    if (!isConnected()) return "Ошибка подключения к базе данных";

    try {
        pqxx::work txn(*conn);
        
        pqxx::result result = txn.exec_params(R"(
            WITH habit_dates AS (
                SELECT 
                    h.habit_id,
                    h.title,
                    hl.completed_date,
                    CASE WHEN LAG(hl.completed_date) OVER (
                        PARTITION BY h.habit_id 
                        ORDER BY hl.completed_date
                    ) = hl.completed_date - INTERVAL '1 day' 
                    THEN 0 ELSE 1 END as is_start_of_streak
                FROM habits h
                JOIN habit_logs hl ON h.habit_id = hl.habit_id
                WHERE h.user_id = $1 AND h.is_active = true
                ORDER BY h.habit_id, hl.completed_date
            ),
            streaks AS (
                SELECT 
                    habit_id,
                    title,
                    completed_date,
                    SUM(is_start_of_streak) OVER (
                        PARTITION BY habit_id 
                        ORDER BY completed_date
                    ) as streak_group
                FROM habit_dates
            ),
            streak_lengths AS (
                SELECT 
                    habit_id,
                    title,
                    streak_group,
                    COUNT(*) as streak_days,
                    MAX(completed_date) as last_date
                FROM streaks
                GROUP BY habit_id, title, streak_group
            )
            SELECT 
                title,
                streak_days as current_streak,
                last_date
            FROM streak_lengths
            WHERE last_date >= CURRENT_DATE - INTERVAL '1 day'
            ORDER BY current_streak DESC
        )", userId);

        std::stringstream output;
        output << "Текущие серии:\n\n";

        if (result.empty()) {
            output << "Пока нет активных серий";
            return output.str();
        }

        for (const auto& row : result) {
            output << row["title"].c_str() 
                   << ": " << row["current_streak"].c_str() << " дней подряд"
                   << " (до " << row["last_date"].c_str() << ")\n";
        }

        return output.str();

    } catch (const std::exception& e) {
        return "Ошибка анализа серий: " + std::string(e.what());
    }
}

std::string Database::getHabitsList(int userId) {
    if (!isConnected()) return "Ошибка подключения к базе данных";

    try {
        pqxx::work txn(*conn);
        pqxx::result result = txn.exec_params(R"(
            SELECT habit_id, title, description, target_frequency, is_active
            FROM habits 
            WHERE user_id = $1
            ORDER BY habit_id
        )", userId);

        std::stringstream output;
        output << "Ваши привычки:\n\n";

        if (result.empty()) {
            output << "У вас пока нет привычек";
            return output.str();
        }

        for (const auto& row : result) {
            std::string status = row["is_active"].as<bool>() ? "активна" : "неактивна";
            output << "ID: " << row["habit_id"].c_str() 
                   << " | " << row["title"].c_str()
                   << " | " << status 
                   << "\nЦель: " << row["target_frequency"].c_str() << " раз/неделю";
            
            if (!row["description"].is_null()) {
                output << "\nОписание: " << row["description"].c_str();
            }
            output << "\n\n";
        }

        return output.str();

    } catch (const std::exception& e) {
        return "Ошибка получения списка привычек: " + std::string(e.what());
    }
}

std::string Database::getUserHabitsForKeyboard(int userId) {
    if (!isConnected()) return "";

    try {
        pqxx::work txn(*conn);
        pqxx::result result = txn.exec_params(R"(
            SELECT habit_id, title
            FROM habits 
            WHERE user_id = $1 AND is_active = true
            ORDER BY habit_id
        )", userId);

        std::stringstream output;

        for (const auto& row : result) {
            output << row["title"].c_str() << " (ID: " << row["habit_id"].c_str() << ")\n";
        }

        return output.str();

    } catch (const std::exception& e) {
        return "";
    }
}

void Database::deleteHabit(int habitId) {
    if (!isConnected()) {
        std::cerr << "Нет подключения к БД при удалении привычки" << std::endl;
        return;
    }

    try {
        pqxx::work txn(*conn);
        
        pqxx::result check = txn.exec_params(
            "SELECT title FROM habits WHERE habit_id = $1", 
            habitId
        );
        
        if (check.empty()) {
            std::cout << "Привычка с ID " << habitId << " не найдена" << std::endl;
            return;
        }
        
        std::string habitTitle = check[0]["title"].c_str();

        txn.exec_params("DELETE FROM habits WHERE habit_id = $1", habitId);
        txn.commit();
        
        std::cout << "Привычка удалена: " << habitTitle << " (ID: " << habitId << ")" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Ошибка удаления привычки ID " << habitId << ": " << e.what() << std::endl;
    }
}

Database::~Database() {
    if (conn && conn->is_open()) {
        conn->close();
        std::cout << "Подключение к базе данных закрыто" << std::endl;
    }
}