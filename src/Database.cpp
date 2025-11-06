#include "Database.hpp"
#include <fstream>
#include <sstream>

Database::Database(const std::string& connectionString) {
    try {
        conn = std::make_unique<pqxx::connection>(connectionString);
        std::cout << "Подключение к базе данных установлено!" << std::endl;
        std::cout << "База данных: " << conn->dbname() << std::endl;
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
        std::cout << "Пользователь " << username << " добавлен!" << std::endl; 
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
        std::cout << "Привычка '" << title << "' добавлена!" << std::endl;
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
        std::cout << "Привычка #" << habitId << " отмечена выполненной!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Ошибка логирования привычки: " << e.what() << std::endl;
    }
}

void Database::showProgressBars(int userId) {
    if (!isConnected()) return;
    
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
        
        std::cout << "\n ПРОГРЕСС ЗА НЕДЕЛЮ:" << std::endl;
        std::cout << "======================" << std::endl;
        
        if (result.empty()) {
            std::cout << "• Нет активных привычек" << std::endl;
            return;
        }
        
        for (const auto& row : result) {
            int completed = row["completed"].as<int>();
            int target = row["target"].as<int>();
            int percent = target > 0 ? (completed * 100) / target : 0;
            
            std::cout << "\n" << row["title"].c_str() << ":" << std::endl;
            std::cout << "[";
            
            int bars = (percent * 20) / 100;  
            for (int i = 0; i < 20; i++) {
                if (i < bars) std::cout << "█";
                else std::cout << "░";
            }
            
            std::cout << "] " << completed << "/" << target << " (" << percent << "%)" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << " Ошибка отображения прогресса: " << e.what() << std::endl;
    }
}

void Database::showHabitStrength(int userId) {
    if (!isConnected()) return;

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

        std::cout << "\n СИЛА ПРИВЫЧЕК (% за 30 дней):" << std::endl;
        std::cout << "==================================" << std::endl;

        for (const auto& row : result) {
            std::cout << "• " << row["title"].c_str() << ": "        // ← Исправлен символ
                      << row["habit_strength_percent"].c_str() << "%" // ← Исправлена опечатка
                      << " (" << row["completed_days"].c_str() << " дней)"
                      << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "Ошибка анализа привычек: " << e.what() << std::endl;
    }
}

void Database::showWeakestWeekday(int userId) {
    if (!isConnected()) return;

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
        
        if (!result.empty()) {
            int worstDay = result[0]["day_of_week"].as<int>();
            double successRate = result[0]["success_rate_percent"].as<double>();
            
            std::string days[] = {"Воскресенье", "Понедельник", "Вторник", "Среда", 
                                 "Четверг", "Пятница", "Суббота"};
            
            std::cout << "\n САМЫЙ СЛАБЫЙ ДЕНЬ:" << std::endl;
            std::cout << "====================" << std::endl;
            std::cout << "• " << days[worstDay] << ": " << successRate << "% успеха" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Ошибка анализа дней: " << e.what() << std::endl;
    }
}

void Database::showCurrentStreaks(int userId) {
    if (!isConnected()) return;

    try {
        pqxx::work txn(*conn);
        
        pqxx::result result = txn.exec_params(R"(
            WITH habit_dates AS (
                SELECT 
                    h.habit_id,
                    h.title,
                    hl.completed_date,
                    -- Проверяем, был ли предыдущий день тоже выполнен
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
                    -- Суммируем "начала серий" чтобы получить группы серий
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

        std::cout << "\n ТЕКУЩИЕ СЕРИИ:" << std::endl;
        std::cout << "=================" << std::endl;

        if (result.empty()) {
            std::cout << "• Пока нет активных серий" << std::endl;
        } else {
            for (const auto& row : result) {
                std::cout << "• " << row["title"].c_str() 
                          << ": " << row["current_streak"].c_str() << " дней подряд"
                          << " (до " << row["last_date"].c_str() << ")"
                          << std::endl;
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Ошибка анализа серий: " << e.what() << std::endl;
    }
}

Database::~Database() {
    if (conn && conn->is_open()) {
        conn->close();
        std::cout << "Подключение к базе данных закрыто" << std::endl;
    }
}