#include "services/checkin_service.h"
#include "kafka/outbox.h"
#include "utils/uuid.h"
#include "utils/time_utils.h"
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

namespace checkin {

    namespace {

        // Parse a Checkin from a DB row
        Checkin rowToCheckin(const pqxx::row& row) {
            Checkin c;
            c.checkin_id = row["checkin_id"].as<std::string>();
            c.flight_id = row["flight_id"].as<std::string>();
            c.passenger_id = row["passenger_id"].as<std::string>();
            c.ticket_id = row["ticket_id"].as<std::string>();
            c.status = statusFromString(row["status"].as<std::string>());
            if (!row["fail_reason"].is_null())
                c.fail_reason = row["fail_reason"].as<std::string>();
            c.created_at = row["created_at"].as<std::string>();
            c.updated_at = row["updated_at"].as<std::string>();
            return c;
        }


        nlohmann::json buildCheckinCompletedEvent(const Checkin& c, const std::string& sim_time) {
            const auto effective_sim_time = sim_time.empty() ? utils::nowISO() : sim_time;
            nlohmann::json payload = {
                {"checkinId",   c.checkin_id},
                {"flightId",    c.flight_id},
                {"passengerId", c.passenger_id},
                {"ticketId",    c.ticket_id},
                {"status",      "success"},
                {"simTime",     effective_sim_time}
            };
            return {
                {"eventId", utils::generateUUID()},
                {"type", "checkin.completed"},
                {"eventType", "checkin.completed"},
                {"ts", effective_sim_time},
                {"occurredAt", effective_sim_time},
                {"producer", "checkin"},
                {"source", "checkin"},
                {"entity", {
                    {"kind", "checkin"},
                    {"id", c.checkin_id}
                }},
                {"correlationId", nullptr},
                {"payload", payload}
            };
        }

    } // namespace

    CheckinService::CheckinService(Database& db,
        CacheService& cache,
        KafkaProducer& producer)
        : db_(db), cache_(cache), producer_(producer) {
    }

    // ─── Idempotency ──────────────────────────────────────────────────────────────

    bool CheckinService::isEventProcessed(const std::string& event_id) {
        return db_.query([&](pqxx::nontransaction& ntxn) {
            auto r = ntxn.exec_params(
                "SELECT 1 FROM processed_events WHERE event_id = $1::uuid",
                event_id);
            return !r.empty();
            });
    }

    void CheckinService::markEventProcessed(const std::string& event_id) {
        db_.transaction([&](pqxx::work& txn) {
            txn.exec_params(
                "INSERT INTO processed_events (event_id) VALUES ($1::uuid) "
                "ON CONFLICT DO NOTHING",
                event_id);
            return true;
            });
    }

    // ─── Query ────────────────────────────────────────────────────────────────────

    std::vector<Checkin> CheckinService::listCheckins(
        const std::optional<std::string>& flight_id,
        const std::optional<std::string>& passenger_id,
        const std::optional<std::string>& status)
    {
        return db_.query([&](pqxx::nontransaction& ntxn) {
            std::string sql =
                "SELECT checkin_id, flight_id, passenger_id, ticket_id, "
                "       status, fail_reason, "
                "       to_char(created_at,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS created_at,"
                "       to_char(updated_at,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS updated_at "
                "FROM checkins WHERE 1=1";

            std::vector<std::string> binds;
            int idx = 1;

            if (flight_id) {
                sql += " AND flight_id = $" + std::to_string(idx++);
                binds.push_back(*flight_id);
            }
            if (passenger_id) {
                sql += " AND passenger_id = $" + std::to_string(idx++);
                binds.push_back(*passenger_id);
            }
            if (status) {
                sql += " AND status = $" + std::to_string(idx++);
                binds.push_back(*status);
            }
            sql += " ORDER BY created_at DESC";

            pqxx::result rows;
            if (binds.empty()) {
                rows = ntxn.exec(sql);
            }
            else if (binds.size() == 1) {
                rows = ntxn.exec_params(sql, binds[0]);
            }
            else if (binds.size() == 2) {
                rows = ntxn.exec_params(sql, binds[0], binds[1]);
            }
            else {
                rows = ntxn.exec_params(sql, binds[0], binds[1], binds[2]);
            }

            std::vector<Checkin> result;
            result.reserve(rows.size());
            for (const auto& row : rows)
                result.push_back(rowToCheckin(row));
            return result;
            });
    }

    std::optional<std::vector<nlohmann::json>>
        CheckinService::getCheckinHistory(const std::string& checkin_id) {
        return db_.query(
            [&](pqxx::nontransaction& ntxn)
            -> std::optional<std::vector<nlohmann::json>> {
                // First verify the checkin exists
                auto exists = ntxn.exec_params(
                    "SELECT 1 FROM checkins WHERE checkin_id = $1", checkin_id);
                if (exists.empty()) return std::nullopt;

                auto rows = ntxn.exec_params(
                    "SELECT id, checkin_id, old_status, new_status, fail_reason, "
                    "       changed_by, "
                    "       to_char(changed_at,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS changed_at "
                    "FROM checkin_history "
                    "WHERE checkin_id = $1 "
                    "ORDER BY id ASC",
                    checkin_id);

                std::vector<nlohmann::json> result;
                result.reserve(rows.size());
                for (const auto& row : rows) {
                    nlohmann::json entry;
                    entry["id"] = row["id"].as<int64_t>();
                    entry["checkinId"] = row["checkin_id"].as<std::string>();
                    entry["oldStatus"] = row["old_status"].is_null()
                        ? nullptr
                        : nlohmann::json(row["old_status"].as<std::string>());
                    entry["newStatus"] = row["new_status"].as<std::string>();
                    entry["failReason"] = row["fail_reason"].is_null()
                        ? nullptr
                        : nlohmann::json(row["fail_reason"].as<std::string>());
                    entry["changedBy"] = row["changed_by"].as<std::string>();
                    entry["changedAt"] = row["changed_at"].as<std::string>();
                    result.push_back(std::move(entry));
                }
                return result;
            });
    }

    std::optional<Checkin> CheckinService::getCheckin(const std::string& checkin_id) {
        return db_.query([&](pqxx::nontransaction& ntxn) -> std::optional<Checkin> {
            auto rows = ntxn.exec_params(
                "SELECT checkin_id, flight_id, passenger_id, ticket_id, "
                "       status, fail_reason, "
                "       to_char(created_at,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS created_at,"
                "       to_char(updated_at,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS updated_at "
                "FROM checkins WHERE checkin_id = $1",
                checkin_id);
            if (rows.empty()) return std::nullopt;
            return rowToCheckin(rows[0]);
            });
    }

    // ─── Core logic ───────────────────────────────────────────────────────────────

    std::optional<Checkin> CheckinService::createCheckin(
        const std::string& flight_id,
        const std::string& passenger_id,
        const std::string& ticket_id,
        const std::string& sim_time)
    {
        return db_.transaction([&](pqxx::work& txn) -> std::optional<Checkin> {
            // Idempotency: if successful checkin already exists for this ticket
            auto existing = txn.exec_params(
                "SELECT checkin_id, flight_id, passenger_id, ticket_id, "
                "       status, fail_reason, "
                "       to_char(created_at,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS created_at,"
                "       to_char(updated_at,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS updated_at "
                "FROM checkins WHERE ticket_id = $1 AND status = 'success'",
                ticket_id);

            if (!existing.empty()) {
                spdlog::debug("Checkin already exists for ticket {}, skipping", ticket_id);
                return rowToCheckin(existing[0]);
            }

            std::string checkin_id = utils::newCheckinId();
            auto rows = txn.exec_params(
                "INSERT INTO checkins "
                "  (checkin_id, flight_id, passenger_id, ticket_id, status) "
                "VALUES ($1, $2, $3, $4, 'success') "
                "RETURNING checkin_id, flight_id, passenger_id, ticket_id, "
                "          status, fail_reason, "
                "          to_char(created_at,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS created_at,"
                "          to_char(updated_at,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS updated_at",
                checkin_id, flight_id, passenger_id, ticket_id);

            if (rows.empty()) return std::nullopt;

            Checkin c = rowToCheckin(rows[0]);

            // Enqueue Kafka event via outbox (same transaction → atomicity)
            auto event = buildCheckinCompletedEvent(c, sim_time);
            OutboxPublisher::enqueue(txn, "checkin.events", c.flight_id, event.dump());

            spdlog::info("Checkin created: {} for ticket {}", c.checkin_id, ticket_id);
            return c;
            });
    }

    void CheckinService::publishCheckinCompleted(const Checkin& c,
        const std::string& sim_time) {
        auto event = buildCheckinCompletedEvent(c, sim_time);
        producer_.publish("checkin.events", c.flight_id, event);
    }

    // ─── Manual check-in ─────────────────────────────────────────────────────────

    CheckinResult CheckinService::startCheckin(const StartCheckinRequest& req) {
        // Validate: registration must be open
        if (!cache_.isRegistrationOpen(req.flight_id)) {
            return CheckinError{ 409, {"registration_closed",
                                      "Registration is not open for flight " +
                                      req.flight_id} };
        }

        // Validate: ticket must be active
        if (!cache_.isTicketActive(req.ticket_id)) {
            return CheckinError{ 409, {"ticket_inactive",
                                      "Ticket " + req.ticket_id + " is not active"} };
        }

        // Check existing success (idempotent)
        auto existing = db_.query(
            [&](pqxx::nontransaction& ntxn) -> std::optional<Checkin> {
                auto rows = ntxn.exec_params(
                    "SELECT checkin_id, flight_id, passenger_id, ticket_id, "
                    "       status, fail_reason, "
                    "       to_char(created_at,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS created_at,"
                    "       to_char(updated_at,'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS updated_at "
                    "FROM checkins WHERE ticket_id = $1 AND status = 'success'",
                    req.ticket_id);
                if (rows.empty()) return std::nullopt;
                return rowToCheckin(rows[0]);
            });

        if (existing) {
            return CheckinError{ 409, {"already_checked_in",
                                      "Ticket " + req.ticket_id +
                                      " is already checked in"} };
        }

        // Create the checkin
        auto result = createCheckin(req.flight_id, req.passenger_id, req.ticket_id);
        if (!result) {
            return CheckinError{ 500, {"internal_error", "Failed to create checkin"} };
        }
        return *result;
    }

    // ─── Auto check-in (triggered by RegistrationOpen event) ─────────────────────

    void CheckinService::runAutoCheckin(const std::string& flight_id,
        const std::string& sim_time) {
        spdlog::info("Auto check-in started for flight {}", flight_id);

        auto tickets = cache_.getActiveTicketsForFlight(flight_id);
        spdlog::info("Auto check-in: {} active tickets for flight {}",
            tickets.size(), flight_id);

        int success = 0, skipped = 0;
        for (const auto& ticket : tickets) {
            try {
                auto result = createCheckin(flight_id,
                    ticket.passenger_id,
                    ticket.ticket_id,
                    sim_time);
                if (result) {
                    ++success;
                }
                else {
                    ++skipped;
                }
            }
            catch (const std::exception& e) {
                spdlog::error("Auto check-in error for ticket {}: {}",
                    ticket.ticket_id, e.what());
            }
        }
        spdlog::info("Auto check-in done for flight {}: {} success, {} skipped",
            flight_id, success, skipped);
    }

} // namespace checkin