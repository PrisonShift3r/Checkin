cmake_minimum_required(VERSION 3.16)
project(checkin-service VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
find_package(Threads REQUIRED)
find_package(PostgreSQL REQUIRED)

# Optional switch kept for compatibility with older build scripts.
option(USE_SYSTEM_DEPS "Prefer system packages where possible" OFF)

# ─── Header-only libs ────────────────────────────────────────────────────────
FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

FetchContent_Declare(spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.14.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(spdlog)

# Crow is used as a header-only framework. We do not rely on its CMake package
# because the project is built inside Linux containers and the previous setup
# accidentally pointed CMAKE_PREFIX_PATH to a Windows vcpkg layout.
FetchContent_Declare(crow
    GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
    GIT_TAG        v1.2.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(crow)

# ─── libpqxx ────────────────────────────────────────────────────────────────
set(SKIP_BUILD_TEST ON CACHE BOOL "" FORCE)
set(BUILD_TEST      OFF CACHE BOOL "" FORCE)
FetchContent_Declare(libpqxx
    GIT_REPOSITORY https://github.com/jtv/libpqxx.git
    GIT_TAG        7.10.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(libpqxx)

# ─── librdkafka ──────────────────────────────────────────────────────────────
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

# ─── Sources ─────────────────────────────────────────────────────────────────
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
    ${crow_SOURCE_DIR}/include
    ${librdkafka_SOURCE_DIR}/src
    ${librdkafka_SOURCE_DIR}/src-cpp
)

target_link_libraries(checkin-service PRIVATE
    Threads::Threads
    pqxx
    rdkafka++
    nlohmann_json::nlohmann_json
    spdlog::spdlog
)

target_compile_definitions(checkin-service PRIVATE
    ASIO_STANDALONE
)
