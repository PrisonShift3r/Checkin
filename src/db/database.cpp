#include "db/database.h"
#include <spdlog/spdlog.h>
#include <stdexcept>

namespace checkin {

Database::Database(const std::string& conn_string)
    : conn_string_(conn_string)
{
    initPool();
}

void Database::initPool() {
    for (int i = 0; i < POOL_SIZE; ++i) {
        try {
            pool_.push(std::make_unique<pqxx::connection>(conn_string_));
        } catch (const std::exception& e) {
            spdlog::error("DB pool init error: {}", e.what());
            throw;
        }
    }
    spdlog::info("DB connection pool initialized ({} connections)", POOL_SIZE);
}

std::unique_ptr<pqxx::connection> Database::acquire() {
    std::unique_lock<std::mutex> lock(pool_mutex_);
    if (pool_.empty()) {
        // All connections in use — create a temporary one
        spdlog::warn("DB pool exhausted, creating temporary connection");
        return std::make_unique<pqxx::connection>(conn_string_);
    }
    auto conn = std::move(pool_.front());
    pool_.pop();

    // Reconnect if connection was lost
    if (!conn->is_open()) {
        spdlog::warn("DB connection was closed, reconnecting...");
        conn = std::make_unique<pqxx::connection>(conn_string_);
    }
    return conn;
}

void Database::release(std::unique_ptr<pqxx::connection> conn) {
    std::unique_lock<std::mutex> lock(pool_mutex_);
    if (static_cast<int>(pool_.size()) < POOL_SIZE) {
        pool_.push(std::move(conn));
    }
    // Otherwise discard (temporary connection)
}

bool Database::isHealthy() {
    try {
        query([](pqxx::nontransaction& ntxn) {
            ntxn.exec("SELECT 1");
            return true;
        });
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace checkin
