cmake_minimum_required(VERSION 3.10)
project(HabitCraft)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PostgreSQL REQUIRED)

find_path(LIBPQXX_INCLUDE_DIR pqxx/pqxx)
find_library(LIBPQXX_LIBRARY pqxx)

if (LIBPQXX_INCLUDE_DIR AND LIBPQXX_LIBRARY)
    message(STATUS "Найден libpqxx: ${LIBPQXX_INCLUDE_DIR}, ${LIBPQXX_LIBRARY}")
else()
    message(FATAL_ERROR "libpqxx не найден! Попробуй: sudo apt install libpqxx-dev")
endif()

add_executable(HabitCraft 
    src/main.cpp 
    src/Database.cpp
)

target_include_directories(HabitCraft PRIVATE ${LIBPQXX_INCLUDE_DIR})
target_link_libraries(HabitCraft PRIVATE 
    ${LIBPQXX_LIBRARY}
    PostgreSQL::PostgreSQL
)