#pragma once
#include <rdkafkacpp.h>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <thread>
#include <atomic>
#include <nlohmann/json.hpp>

namespace checkin {

using MessageHandler = std::function<void(const std::string& topic,
                                          const std::string& key,
                                          const nlohmann::json& payload)>;

class KafkaConsumer {
public:
    KafkaConsumer(const std::string& brokers,
                  const std::string& group_id,
                  const std::vector<std::string>& topics);
    ~KafkaConsumer();

    void setHandler(MessageHandler handler);
    void start();   // starts background thread
    void stop();

private:
    std::unique_ptr<RdKafka::KafkaConsumer> consumer_;
    MessageHandler handler_;
    std::thread worker_;
    std::atomic<bool> running_{false};

    void run();
};

} // namespace checkin
