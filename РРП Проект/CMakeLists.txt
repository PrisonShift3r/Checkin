cmake_minimum_required(VERSION 3.16)
project(checkin-service VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
find_package(Threads REQUIRED)

# ══════════════════════════════════════════════════════════════════════════════
# Option A (default): все зависимости через FetchContent
#   cmake -B build                    ← нужен только cmake + g++ + libpq-dev + libssl-dev + zlib1g-dev
# Option B: системные пакеты
#   cmake -B build -DUSE_SYSTEM_DEPS=ON
# ══════════════════════════════════════════════════════════════════════════════
option(USE_SYSTEM_DEPS "Use system-installed C++ libraries instead of FetchContent" OFF)

# ─── nlohmann/json ────────────────────────────────────────────────────────────
if(USE_SYSTEM_DEPS)
    find_package(nlohmann_json REQUIRED)
else()
    FetchContent_Declare(nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG        v3.11.3
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# ─── spdlog ───────────────────────────────────────────────────────────────────
if(USE_SYSTEM_DEPS)
    find_package(spdlog REQUIRED)
else()
    FetchContent_Declare(spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG        v1.14.1
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(spdlog)
endif()

# ─── Standalone Asio (нужен Crow без Boost) ───────────────────────────────────
FetchContent_Declare(asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG        asio-1-30-2
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(asio)

# Создаём INTERFACE-цель для Asio, чтобы Crow мог на неё ссылаться
add_library(asio_headers INTERFACE)
target_include_directories(asio_headers INTERFACE ${asio_SOURCE_DIR}/asio/include)
target_compile_definitions(asio_headers INTERFACE ASIO_STANDALONE)

# ─── Crow (HTTP, header-only) ─────────────────────────────────────────────────
if(USE_SYSTEM_DEPS)
    find_package(Crow REQUIRED)
else()
    set(CROW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(CROW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
    set(CROW_FEATURES       "" CACHE STRING "" FORCE)
    FetchContent_Declare(crow
        GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
        GIT_TAG        v1.2.0
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(crow)
    # Подключаем standalone Asio к Crow::Crow (INTERFACE, так что это безопасно)
    target_link_libraries(Crow::Crow INTERFACE asio_headers)
    target_compile_definitions(Crow::Crow INTERFACE CROW_USE_BOOST=0 ASIO_STANDALONE)
endif()

# ─── libpqxx (PostgreSQL C++ client) ──────────────────────────────────────────
# ВНИМАНИЕ: libpq-dev должен быть установлен системно:
#   Ubuntu/Debian:  sudo apt install libpq-dev
#   macOS:          brew install postgresql@16
if(USE_SYSTEM_DEPS)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(PQXX REQUIRED libpqxx)
    add_library(pqxx_if INTERFACE)
    target_include_directories(pqxx_if INTERFACE ${PQXX_INCLUDE_DIRS})
    target_link_libraries     (pqxx_if INTERFACE ${PQXX_LIBRARIES})
    target_compile_options    (pqxx_if INTERFACE ${PQXX_CFLAGS_OTHER})
else()
    find_package(PostgreSQL REQUIRED)  # ищет системный libpq
    set(SKIP_BUILD_TEST ON  CACHE BOOL "" FORCE)
    set(BUILD_TEST      OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(libpqxx
        GIT_REPOSITORY https://github.com/jtv/libpqxx.git
        GIT_TAG        7.9.1
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(libpqxx)
    add_library(pqxx_if INTERFACE)
    target_link_libraries(pqxx_if INTERFACE pqxx)
endif()

# ─── librdkafka (Kafka C++ client) ────────────────────────────────────────────
# ВНИМАНИЕ: libssl-dev + zlib1g-dev должны быть установлены системно:
#   Ubuntu/Debian:  sudo apt install libssl-dev zlib1g-dev
#   macOS:          brew install openssl zlib
if(USE_SYSTEM_DEPS)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(RDKAFKA REQUIRED rdkafka++)
    add_library(rdkafka_if INTERFACE)
    target_include_directories(rdkafka_if INTERFACE ${RDKAFKA_INCLUDE_DIRS})
    target_link_libraries     (rdkafka_if INTERFACE ${RDKAFKA_LIBRARIES})
else()
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
    add_library(rdkafka_if INTERFACE)
    target_link_libraries(rdkafka_if INTERFACE rdkafka++)
endif()

# ══════════════════════════════════════════════════════════════════════════════
# Исполняемый файл
# ══════════════════════════════════════════════════════════════════════════════
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
    asio_headers
    pqxx_if
    rdkafka_if
    nlohmann_json::nlohmann_json
    spdlog::spdlog
)

target_compile_definitions(checkin-service PRIVATE
    ASIO_STANDALONE
    CROW_USE_BOOST=0
)

target_compile_options(checkin-service PRIVATE
    -Wall -Wextra -Wno-unused-parameter -O2
)