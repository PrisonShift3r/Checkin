#pragma once
#include "models.h"
#include <unordered_map>
#include <vector>
#include <shared_mutex>
#include <optional>

namespace checkin {

// Thread-safe in-memory caches for flights and tickets
// Populated by Kafka events to avoid REST dependencies
class CacheService {
public:
    CacheService() = default;

    // ─── Flight cache ──────────────────────────────────────────────────────
    void updateFlight(const std::string& flight_id, FlightStatus status,
                      const std::string& sim_time = "");
    std::optional<FlightCacheEntry> getFlight(const std::string& flight_id) const;
    bool isRegistrationOpen(const std::string& flight_id) const;

    // ─── Ticket cache ──────────────────────────────────────────────────────
    void upsertTicket(const std::string& ticket_id, const std::string& flight_id,
                      const std::string& passenger_id, TicketStatus status);
    void updateTicketStatus(const std::string& ticket_id, TicketStatus status);
    std::optional<TicketCacheEntry> getTicket(const std::string& ticket_id) const;
    bool isTicketActive(const std::string& ticket_id) const;

    // Returns all active tickets for a given flight
    std::vector<TicketCacheEntry> getActiveTicketsForFlight(
        const std::string& flight_id) const;

private:
    mutable std::shared_mutex flight_mutex_;
    std::unordered_map<std::string, FlightCacheEntry> flight_cache_;

    mutable std::shared_mutex ticket_mutex_;
    std::unordered_map<std::string, TicketCacheEntry> ticket_cache_;
};

} // namespace checkin
