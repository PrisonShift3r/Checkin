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

# ─── Standalone Asio ──────────────────────────────────────────────────────────
FetchContent_Declare(asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG        asio-1-30-2
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(asio)

add_library(asio_headers INTERFACE)
target_include_directories(asio_headers INTERFACE ${asio_SOURCE_DIR}/asio/include)
target_compile_definitions(asio_headers INTERFACE ASIO_STANDALONE)

# Crow ищет Asio через эту переменную
set(ASIO_INCLUDE_DIR ${asio_SOURCE_DIR}/asio/include CACHE PATH "" FORCE)

# ─── Crow ─────────────────────────────────────────────────────────────────────
set(CROW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CROW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(CROW_USE_BOOST      OFF CACHE BOOL "" FORCE)

FetchContent_Declare(crow
    GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
    GIT_TAG        v1.2.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(crow)

# ─── libpqxx ──────────────────────────────────────────────────────────────────
# Windows: установить PostgreSQL https://www.postgresql.org/download/windows/
# Затем передать путь: cmake -B build -DPostgreSQL_ROOT="C:/Program Files/PostgreSQL/16"
find_package(PostgreSQL REQUIRED)

set(SKIP_BUILD_TEST ON  CACHE BOOL "" FORCE)
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

target_compile_definitions(checkin-service PRIVATE
    ASIO_STANDALONE
    CROW_USE_BOOST=0
    CROW_ENABLE_SSL
)

target_include_directories(checkin-service PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${asio_SOURCE_DIR}/asio/include
    ${crow_SOURCE_DIR}/include
)

target_link_libraries(checkin-service PRIVATE
    Threads::Threads
    asio_headers
    pqxx
    rdkafka++
    nlohmann_json::nlohmann_json
    spdlog::spdlog
)