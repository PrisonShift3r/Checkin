#pragma once
#include <librdkafka/rdkafkacpp.h>
#include <string>
#include <memory>
#include <nlohmann/json.hpp>

namespace checkin {

class KafkaProducer {
public:
    explicit KafkaProducer(const std::string& brokers);
    ~KafkaProducer();

    // Publish message to topic with optional key
    bool publish(const std::string& topic,
                 const std::string& key,
                 const nlohmann::json& payload);

    void flush(int timeout_ms = 5000);

private:
    std::unique_ptr<RdKafka::Producer> producer_;
    std::unique_ptr<RdKafka::Topic> checkin_topic_;

    std::string getLastError() const { return last_error_; }
    mutable std::string last_error_;
};

} // namespace checkin
