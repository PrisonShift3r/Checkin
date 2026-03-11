#pragma once
#include <string>
#include <cstdlib>

namespace checkin {

struct Config {
    // Server
    int port = 8000;

    // Database
    std::string db_host     = "postgres";
    int         db_port     = 5432;
    std::string db_name     = "checkin";
    std::string db_user     = "checkin";
    std::string db_password = "checkin";

    // Kafka
    std::string kafka_brokers    = "kafka:9092";
    std::string kafka_group_id   = "checkin-service";

    // Kafka topics consumed
    std::string topic_flights = "flights.events";
    std::string topic_tickets = "tickets.events";

    // Kafka topic produced
    std::string topic_checkin = "checkin.events";

    static Config fromEnv() {
        Config c;
        auto getenv_or = [](const char* key, const std::string& def) -> std::string {
            const char* v = std::getenv(key);
            return v ? v : def;
        };
        auto getenv_int = [&](const char* key, int def) -> int {
            const char* v = std::getenv(key);
            return v ? std::stoi(v) : def;
        };

        c.port          = getenv_int("PORT", 8000);
        c.db_host       = getenv_or("DB_HOST", "postgres");
        c.db_port       = getenv_int("DB_PORT", 5432);
        c.db_name       = getenv_or("DB_NAME", "checkin");
        c.db_user       = getenv_or("DB_USER", "checkin");
        c.db_password   = getenv_or("DB_PASSWORD", "checkin");
        c.kafka_brokers = getenv_or("KAFKA_BROKERS", "kafka:9092");
        c.kafka_group_id= getenv_or("KAFKA_GROUP_ID", "checkin-service");
        c.topic_flights = getenv_or("TOPIC_FLIGHTS", "flights.events");
        c.topic_tickets = getenv_or("TOPIC_TICKETS", "tickets.events");
        c.topic_checkin = getenv_or("TOPIC_CHECKIN", "checkin.events");
        return c;
    }

    std::string connectionString() const {
        return "host=" + db_host +
               " port=" + std::to_string(db_port) +
               " dbname=" + db_name +
               " user=" + db_user +
               " password=" + db_password +
               " connect_timeout=5";
    }
};

} // namespace checkin
