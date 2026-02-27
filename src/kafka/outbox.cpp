#include "kafka/outbox.h"
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <chrono>
#include <thread>

namespace checkin {

// SQL: create table checkin_outbox (
//   id         bigserial primary key,
//   topic      text not null,
//   key        text,
//   payload    text not null,
//   created_at timestamp not null default now()
// );

OutboxPublisher::OutboxPublisher(Database& db, KafkaProducer& producer)
    : db_(db), producer_(producer) {}

OutboxPublisher::~OutboxPublisher() {
    stop();
}

void OutboxPublisher::enqueue(pqxx::work& txn,
                               const std::string& topic,
                               const std::string& key,
                               const std::string& payload_json) {
    txn.exec_params(
        "INSERT INTO checkin_outbox (topic, key, payload) VALUES ($1, $2, $3)",
        topic, key, payload_json
    );
}

void OutboxPublisher::start() {
    running_ = true;
    worker_ = std::thread(&OutboxPublisher::run, this);
    spdlog::info("Outbox publisher started");
}

void OutboxPublisher::stop() {
    running_ = false;
    if (worker_.joinable()) worker_.join();
}

void OutboxPublisher::run() {
    while (running_) {
        try {
            // Fetch up to 100 pending outbox messages
            auto rows = db_.query([](pqxx::nontransaction& ntxn) {
                return ntxn.exec(
                    "SELECT id, topic, key, payload FROM checkin_outbox "
                    "ORDER BY id LIMIT 100"
                );
            });

            if (rows.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                continue;
            }

            for (const auto& row : rows) {
                auto id      = row[0].as<int64_t>();
                auto topic   = row[1].as<std::string>();
                auto key     = row[2].is_null() ? "" : row[2].as<std::string>();
                auto payload = row[3].as<std::string>();

                try {
                    auto json = nlohmann::json::parse(payload);
                    bool ok   = producer_.publish(topic, key, json);
                    if (ok) {
                        producer_.flush(1000);
                        // Delete from outbox after successful publish
                        db_.transaction([&](pqxx::work& txn) {
                            txn.exec_params(
                                "DELETE FROM checkin_outbox WHERE id = $1", id);
                            return true;
                        });
                        spdlog::debug("Outbox: published id={} topic={}", id, topic);
                    }
                } catch (const std::exception& e) {
                    spdlog::error("Outbox publish error id={}: {}", id, e.what());
                }
            }
        } catch (const std::exception& e) {
            spdlog::error("Outbox run error: {}", e.what());
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    }
}

} // namespace checkin
