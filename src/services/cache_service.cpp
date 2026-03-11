#include "services/cache_service.h"
#include <spdlog/spdlog.h>

namespace checkin {

// ─── Flight cache ─────────────────────────────────────────────────────────────

void CacheService::updateFlight(const std::string& flight_id,
                                 FlightStatus status,
                                 const std::string& sim_time) {
    std::unique_lock lock(flight_mutex_);
    auto& entry = flight_cache_[flight_id];
    entry.flight_id = flight_id;
    entry.status    = status;
    if (!sim_time.empty()) entry.sim_time = sim_time;
    spdlog::debug("FlightCache updated: {} -> {}", flight_id,
                  static_cast<int>(status));
}

std::optional<FlightCacheEntry>
CacheService::getFlight(const std::string& flight_id) const {
    std::shared_lock lock(flight_mutex_);
    auto it = flight_cache_.find(flight_id);
    if (it == flight_cache_.end()) return std::nullopt;
    return it->second;
}

bool CacheService::isRegistrationOpen(const std::string& flight_id) const {
    auto f = getFlight(flight_id);
    return f && f->status == FlightStatus::RegistrationOpen;
}

// ─── Ticket cache ─────────────────────────────────────────────────────────────

void CacheService::upsertTicket(const std::string& ticket_id,
                                 const std::string& flight_id,
                                 const std::string& passenger_id,
                                 TicketStatus status) {
    std::unique_lock lock(ticket_mutex_);
    auto& entry = ticket_cache_[ticket_id];
    entry.ticket_id    = ticket_id;
    entry.flight_id    = flight_id;
    entry.passenger_id = passenger_id;
    entry.status       = status;
    spdlog::debug("TicketCache upserted: {} status={}", ticket_id,
                  static_cast<int>(status));
}

void CacheService::updateTicketStatus(const std::string& ticket_id,
                                       TicketStatus status) {
    std::unique_lock lock(ticket_mutex_);
    auto it = ticket_cache_.find(ticket_id);
    if (it != ticket_cache_.end()) {
        it->second.status = status;
        spdlog::debug("TicketCache status updated: {} -> {}", ticket_id,
                      static_cast<int>(status));
    } else {
        spdlog::warn("TicketCache: ticket {} not found for status update", ticket_id);
    }
}

std::optional<TicketCacheEntry>
CacheService::getTicket(const std::string& ticket_id) const {
    std::shared_lock lock(ticket_mutex_);
    auto it = ticket_cache_.find(ticket_id);
    if (it == ticket_cache_.end()) return std::nullopt;
    return it->second;
}

bool CacheService::isTicketActive(const std::string& ticket_id) const {
    auto t = getTicket(ticket_id);
    return t && t->status == TicketStatus::Active;
}

std::vector<TicketCacheEntry>
CacheService::getActiveTicketsForFlight(const std::string& flight_id) const {
    std::shared_lock lock(ticket_mutex_);
    std::vector<TicketCacheEntry> result;
    for (const auto& [id, entry] : ticket_cache_) {
        if (entry.flight_id == flight_id && entry.status == TicketStatus::Active) {
            result.push_back(entry);
        }
    }
    return result;
}

} // namespace checkin
