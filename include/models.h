#pragma once
#include <string>
#include <optional>
#include <nlohmann/json.hpp>

namespace checkin {

// ─── Checkin model ────────────────────────────────────────────────────────────
enum class CheckinStatus { Started, Success, Failed };

inline std::string statusToString(CheckinStatus s) {
    switch (s) {
        case CheckinStatus::Started: return "started";
        case CheckinStatus::Success: return "success";
        case CheckinStatus::Failed:  return "failed";
    }
    return "unknown";
}

inline CheckinStatus statusFromString(const std::string& s) {
    if (s == "success") return CheckinStatus::Success;
    if (s == "failed")  return CheckinStatus::Failed;
    return CheckinStatus::Started;
}

struct Checkin {
    std::string checkin_id;
    std::string flight_id;
    std::string passenger_id;
    std::string ticket_id;
    CheckinStatus status;
    std::optional<std::string> fail_reason;
    std::string created_at;
    std::string updated_at;

    nlohmann::json toJson() const {
        nlohmann::json j;
        j["checkinId"]   = checkin_id;
        j["flightId"]    = flight_id;
        j["passengerId"] = passenger_id;
        j["ticketId"]    = ticket_id;
        j["status"]      = statusToString(status);
        j["failReason"]  = fail_reason ? *fail_reason : nullptr;
        j["createdAt"]   = created_at;
        j["updatedAt"]   = updated_at;
        return j;
    }
};

// ─── Cache models ─────────────────────────────────────────────────────────────
enum class FlightStatus {
    Unknown, Scheduled, Boarding, RegistrationOpen,
    Departed, Arrived, Cancelled
};

inline FlightStatus flightStatusFromString(const std::string& s) {
    if (s == "RegistrationOpen") return FlightStatus::RegistrationOpen;
    if (s == "Boarding")         return FlightStatus::Boarding;
    if (s == "Departed")         return FlightStatus::Departed;
    if (s == "Arrived")          return FlightStatus::Arrived;
    if (s == "Cancelled")        return FlightStatus::Cancelled;
    if (s == "Scheduled")        return FlightStatus::Scheduled;
    return FlightStatus::Unknown;
}

struct FlightCacheEntry {
    std::string flight_id;
    FlightStatus status = FlightStatus::Unknown;
    std::string sim_time;
};

enum class TicketStatus { Unknown, Active, Refunded, Bumped };

inline TicketStatus ticketStatusFromString(const std::string& s) {
    if (s == "active")   return TicketStatus::Active;
    if (s == "refunded") return TicketStatus::Refunded;
    if (s == "bumped")   return TicketStatus::Bumped;
    return TicketStatus::Unknown;
}

struct TicketCacheEntry {
    std::string ticket_id;
    std::string flight_id;
    std::string passenger_id;
    TicketStatus status = TicketStatus::Unknown;
};

// ─── Request/Response ─────────────────────────────────────────────────────────
struct StartCheckinRequest {
    std::string flight_id;
    std::string passenger_id;
    std::string ticket_id;
};

struct ApiError {
    std::string code;
    std::string message;

    nlohmann::json toJson() const {
        return { {"code", code}, {"message", message} };
    }
};

} // namespace checkin
