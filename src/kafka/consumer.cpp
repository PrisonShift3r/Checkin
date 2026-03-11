#include "kafka/consumer.h"
#include <spdlog/spdlog.h>
#include <stdexcept>

namespace checkin {

KafkaConsumer::KafkaConsumer(const std::string& brokers,
                              const std::string& group_id,
                              const std::vector<std::string>& topics) {
    std::string errstr;
    auto conf = std::unique_ptr<RdKafka::Conf>(
        RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));

    conf->set("bootstrap.servers", brokers, errstr);
    conf->set("group.id", group_id, errstr);
    conf->set("auto.offset.reset", "earliest", errstr);
    conf->set("enable.auto.commit", "true", errstr);
    conf->set("auto.commit.interval.ms", "1000", errstr);
    // Session timeout: allows more time for rebalance
    conf->set("session.timeout.ms", "30000", errstr);

    consumer_.reset(RdKafka::KafkaConsumer::create(conf.get(), errstr));
    if (!consumer_) {
        throw std::runtime_error("Failed to create Kafka consumer: " + errstr);
    }

    RdKafka::ErrorCode err = consumer_->subscribe(topics);
    if (err != RdKafka::ERR_NO_ERROR) {
        throw std::runtime_error("Failed to subscribe to Kafka topics: " +
                                 RdKafka::err2str(err));
    }
    spdlog::info("Kafka consumer subscribed to {} topic(s)", topics.size());
}

KafkaConsumer::~KafkaConsumer() {
    stop();
    if (consumer_) consumer_->close();
}

void KafkaConsumer::setHandler(MessageHandler handler) {
    handler_ = std::move(handler);
}

void KafkaConsumer::start() {
    running_ = true;
    worker_ = std::thread(&KafkaConsumer::run, this);
    spdlog::info("Kafka consumer thread started");
}

void KafkaConsumer::stop() {
    running_ = false;
    if (worker_.joinable()) worker_.join();
}

void KafkaConsumer::run() {
    while (running_) {
        auto msg = std::unique_ptr<RdKafka::Message>(
            consumer_->consume(500 /* ms */));

        switch (msg->err()) {
            case RdKafka::ERR_NO_ERROR: {
                if (!handler_) break;
                try {
                    std::string topic = msg->topic_name();
                    std::string key   = msg->key()
                                        ? *msg->key()
                                        : "";
                    std::string body(static_cast<const char*>(msg->payload()),
                                     msg->len());
                    auto json = nlohmann::json::parse(body, nullptr, false);
                    if (json.is_discarded()) {
                        spdlog::warn("Kafka: invalid JSON from topic {}", topic);
                        break;
                    }
                    handler_(topic, key, json);
                } catch (const std::exception& e) {
                    spdlog::error("Kafka message handler error: {}", e.what());
                }
                break;
            }
            case RdKafka::ERR__TIMED_OUT:
                // Normal — no messages in poll window
                break;
            case RdKafka::ERR__PARTITION_EOF:
                // End of partition, not an error
                break;
            default:
                spdlog::warn("Kafka consume error: {}", msg->errstr());
                break;
        }
    }
}

} // namespace checkin
