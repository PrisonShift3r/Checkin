#include "kafka/producer.h"
#include <spdlog/spdlog.h>
#include <stdexcept>

namespace checkin {

KafkaProducer::KafkaProducer(const std::string& brokers) {
    std::string errstr;
    auto conf = std::unique_ptr<RdKafka::Conf>(
        RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));

    conf->set("bootstrap.servers", brokers, errstr);
    conf->set("acks", "all", errstr);                    // durability
    conf->set("enable.idempotence", "true", errstr);     // exactly-once delivery
    conf->set("retries", "5", errstr);
    conf->set("retry.backoff.ms", "500", errstr);
    conf->set("linger.ms", "5", errstr);                 // slight batching

    producer_.reset(RdKafka::Producer::create(conf.get(), errstr));
    if (!producer_) {
        throw std::runtime_error("Failed to create Kafka producer: " + errstr);
    }
    spdlog::info("Kafka producer connected to: {}", brokers);
}

KafkaProducer::~KafkaProducer() {
    flush(10000);
}

bool KafkaProducer::publish(const std::string& topic,
                             const std::string& key,
                             const nlohmann::json& payload) {
    std::string payload_str = payload.dump();

    RdKafka::ErrorCode err = producer_->produce(
        topic,
        RdKafka::Topic::PARTITION_UA,         // auto-select partition
        RdKafka::Producer::RK_MSG_COPY,
        const_cast<char*>(payload_str.c_str()),
        payload_str.size(),
        key.empty() ? nullptr : key.c_str(),
        key.size(),
        0,    // timestamp
        nullptr
    );

    if (err != RdKafka::ERR_NO_ERROR) {
        spdlog::error("Kafka produce failed [{}]: {}",
                      topic, RdKafka::err2str(err));
        return false;
    }

    producer_->poll(0); // trigger delivery callbacks without blocking
    spdlog::debug("Kafka message queued: topic={} key={}", topic, key);
    return true;
}

void KafkaProducer::flush(int timeout_ms) {
    if (producer_) {
        producer_->flush(timeout_ms);
    }
}

} // namespace checkin
