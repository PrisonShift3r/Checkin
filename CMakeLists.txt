cmake_minimum_required(VERSION 3.16)
project(checkin-service VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
find_package(Threads REQUIRED)

# ─── nlohmann/json ────────────────────────────────────────────────────────────
FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

# ─── spdlog ───────────────────────────────────────────────────────────────────
FetchContent_Declare(spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.14.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(spdlog)

# ─── Crow + Asio через vcpkg ──────────────────────────────────────────────────
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/share/crow")
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/share/asio")

find_package(Crow CONFIG REQUIRED)
find_package(asio CONFIG REQUIRED)

# ─── libpqxx ──────────────────────────────────────────────────────────────────
find_package(PostgreSQL REQUIRED)
set(SKIP_BUILD_TEST ON CACHE BOOL "" FORCE)
set(BUILD_TEST      OFF CACHE BOOL "" FORCE)
FetchContent_Declare(libpqxx
    GIT_REPOSITORY https://github.com/jtv/libpqxx.git
    GIT_TAG        7.10.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(libpqxx)

# ─── librdkafka ───────────────────────────────────────────────────────────────
set(RDKAFKA_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(RDKAFKA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(WITH_SASL              OFF CACHE BOOL "" FORCE)
set(WITH_SSL               ON  CACHE BOOL "" FORCE)
set(WITH_ZLIB              ON  CACHE BOOL "" FORCE)
FetchContent_Declare(librdkafka
    GIT_REPOSITORY https://github.com/confluentinc/librdkafka.git
    GIT_TAG        v2.4.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(librdkafka)

# ─── Исполняемый файл ─────────────────────────────────────────────────────────
set(SOURCES
    src/main.cpp
    src/db/database.cpp
    src/db/migrator.cpp
    src/kafka/consumer.cpp
    src/kafka/producer.cpp
    src/kafka/outbox.cpp
    src/services/checkin_service.cpp
    src/services/cache_service.cpp
    src/handlers/checkin_handlers.cpp
    src/handlers/health_handler.cpp
    src/utils/uuid.cpp
    src/utils/time_utils.cpp
)

add_executable(checkin-service ${SOURCES})

target_include_directories(checkin-service PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(checkin-service PRIVATE
    Threads::Threads
    Crow::Crow
    asio::asio
    pqxx
    rdkafka++
    nlohmann_json::nlohmann_json
    spdlog::spdlog
)

target_compile_definitions(checkin-service PRIVATE
    ASIO_STANDALONE
    CROW_USE_BOOST=0
)