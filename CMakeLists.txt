cmake_minimum_required(VERSION 3.15)
project(HabitCraftBot VERSION 1.0.0 DESCRIPTION "Telegram Bot for Habit Tracking")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2")
endif()

find_package(CURL REQUIRED)
find_package(PostgreSQL REQUIRED)
find_package(nlohmann_json 3.8.0 REQUIRED)

find_path(PQXX_INCLUDE_DIR
    NAMES pqxx/pqxx
    PATHS /usr/include /usr/local/include
)

find_library(PQXX_LIBRARY
    NAMES pqxx
    PATHS /usr/lib /usr/local/lib
)

if(NOT PQXX_INCLUDE_DIR OR NOT PQXX_LIBRARY)
    message(FATAL_ERROR "libpqxx not found. Install with: sudo apt-get install libpqxx-dev")
endif()

add_executable(HabitCraftBot
    src/main.cpp
    src/Database.cpp
    src/TelegramBot.cpp
)

target_include_directories(HabitCraftBot PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${PQXX_INCLUDE_DIR}
    ${CURL_INCLUDE_DIRS}
)

target_link_libraries(HabitCraftBot PRIVATE
    ${PQXX_LIBRARY}
    PostgreSQL::PostgreSQL
    CURL::libcurl
    nlohmann_json::nlohmann_json
)

install(TARGETS HabitCraftBot
    RUNTIME DESTINATION bin
)

option(BUILD_TESTING "Build tests" OFF)
option(ENABLE_DEBUG "Enable debug output" OFF)

if(ENABLE_DEBUG)
    target_compile_definitions(HabitCraftBot PRIVATE ENABLE_DEBUG=1)
endif()

message(STATUS "HabitCraftBot Configuration:")
message(STATUS "  C++ Compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "  C++ Flags: ${CMAKE_CXX_FLAGS}")
message(STATUS "  PostgreSQL: ${PostgreSQL_VERSION}")
message(STATUS "  CURL: ${CURL_VERSION}")
message(STATUS "  nlohmann_json: ${nlohmann_json_VERSION}")
message(STATUS "  libpqxx: found")

add_custom_target(check-dependencies ALL
    COMMAND ${CMAKE_COMMAND} -E echo "All dependencies satisfied"
    COMMENT "Checking build dependencies"
)

set_target_properties(HabitCraftBot PROPERTIES
    OUTPUT_NAME "habitcraft-bot"
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

message(STATUS "")
message(STATUS "Build configuration complete!")
message(STATUS "To build the project run:")
message(STATUS "  cmake --build . --config Release")
message(STATUS "")
message(STATUS "Or use:")
message(STATUS "  make")