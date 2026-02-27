#pragma once
#include <pqxx/pqxx>
#include <string>
#include <memory>
#include <mutex>
#include <queue>

namespace checkin {

class Database {
public:
    explicit Database(const std::string& conn_string);
    ~Database() = default;

    // Non-copyable
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    // Execute a transaction
    template<typename Func>
    auto transaction(Func&& func) {
        auto conn = acquire();
        pqxx::work txn(*conn);
        auto result = func(txn);
        txn.commit();
        release(std::move(conn));
        return result;
    }

    // Execute read-only query
    template<typename Func>
    auto query(Func&& func) {
        auto conn = acquire();
        pqxx::nontransaction ntxn(*conn);
        auto result = func(ntxn);
        release(std::move(conn));
        return result;
    }

    bool isHealthy();

private:
    std::string conn_string_;
    std::mutex pool_mutex_;
    std::queue<std::unique_ptr<pqxx::connection>> pool_;
    static constexpr int POOL_SIZE = 5;

    void initPool();
    std::unique_ptr<pqxx::connection> acquire();
    void release(std::unique_ptr<pqxx::connection> conn);
};

} // namespace checkin
