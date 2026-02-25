#pragma once
#include "models.h"
#include "db/database.h"
#include "services/cache_service.h"
#include "kafka/producer.h"
#include <vector>
#include <optional>
#include <variant>

namespace checkin {

struct CheckinError {
    int http_status; // 400, 409, 500
    ApiError error;
};

using CheckinResult = std::variant<Checkin, CheckinError>;

class CheckinService {
public:
    CheckinService(Database& db, CacheService& cache, KafkaProducer& producer);

    // Manual check-in via REST
    CheckinResult startCheckin(const StartCheckinRequest& req);

    // Auto check-in triggered by RegistrationOpen event
    void runAutoCheckin(const std::string& flight_id, const std::string& sim_time);

    // Query
    std::vector<Checkin> listCheckins(
        const std::optional<std::string>& flight_id,
        const std::optional<std::string>& passenger_id,
        const std::optional<std::string>& status);

    std::optional<Checkin> getCheckin(const std::string& checkin_id);

    // Returns true if this event_id was already processed (idempotency)
    bool isEventProcessed(const std::string& event_id);
    void markEventProcessed(const std::string& event_id);

private:
    Database& db_;
    CacheService& cache_;
    KafkaProducer& producer_;

    // Create checkin record; returns created Checkin
    // Skips if ticketId already has a successful checkin (idempotent)
    std::optional<Checkin> createCheckin(
        const std::string& flight_id,
        const std::string& passenger_id,
        const std::string& ticket_id,
        const std::string& sim_time = "");

    void publishCheckinCompleted(const Checkin& c, const std::string& sim_time);
};

} // namespace checkin
