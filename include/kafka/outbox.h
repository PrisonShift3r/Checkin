#pragma once
// Transactional outbox pattern:
// Instead of publishing directly to Kafka (which can fail independently of DB),
// we write events to the DB in the same transaction, then a background worker
// reads and publishes them.  On publish success the outbox row is deleted.

#include "db/database.h"
#include "kafka/producer.h"
#include <thread>
#include <atomic>

namespace checkin {

class OutboxPublisher {
public:
    OutboxPublisher(Database& db, KafkaProducer& producer);
    ~OutboxPublisher();

    void start();  // background polling thread
    void stop();

    // Called inside a DB transaction to enqueue an event
    static void enqueue(pqxx::work& txn,
                        const std::string& topic,
                        const std::string& key,
                        const std::string& payload_json);

private:
    Database& db_;
    KafkaProducer& producer_;
    std::thread worker_;
    std::atomic<bool> running_{false};

    void run();
};

} // namespace checkin
