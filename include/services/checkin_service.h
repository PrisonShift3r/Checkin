#pragma once
#include "models.h"
#include "db/database.h"
#include "services/cache_service.h"
#include "kafka/producer.h"
#include <vector>
#include <optional>
#include <variant>
#include <nlohmann/json.hpp>

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

        // ── Query ──────────────────────────────────────────────────────────────────
        std::vector<Checkin> listCheckins(
            const std::optional<std::string>& flight_id,
            const std::optional<std::string>& passenger_id,
            const std::optional<std::string>& status);

        std::optional<Checkin> getCheckin(const std::string& checkin_id);

        // Returns nullopt if the checkin doesn't exist; empty vector if no history yet.
        std::optional<std::vector<nlohmann::json>> getCheckinHistory(
            const std::string& checkin_id);

        // ── Idempotency ────────────────────────────────────────────────────────────
        bool isEventProcessed(const std::string& event_id);
        void markEventProcessed(const std::string& event_id);

    private:
        Database& db_;
        CacheService& cache_;
        KafkaProducer& producer_;

        // Create checkin record in a single DB transaction.
        // Returns the existing checkin if ticketId already has one (idempotent).
        std::optional<Checkin> createCheckin(
            const std::string& flight_id,
            const std::string& passenger_id,
            const std::string& ticket_id,
            const std::string& sim_time = "");

        void publishCheckinCompleted(const Checkin& c, const std::string& sim_time);
    };

} // namespace checkin
