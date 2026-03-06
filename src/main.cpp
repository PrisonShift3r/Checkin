#include "config.h"
#include "db/database.h"
#include "db/migrator.h"
#include "kafka/consumer.h"
#include "kafka/producer.h"
#include "kafka/outbox.h"
#include "services/checkin_service.h"
#include "services/cache_service.h"
#include "handlers/handlers.h"
#include <crow.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <nlohmann/json.hpp>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>

// ─── Global shutdown flag ─────────────────────────────────────────────────────
static std::atomic<bool> g_shutdown{ false };

void signalHandler(int) {
    spdlog::info("Shutdown signal received");
    g_shutdown = true;
}

// ─── Kafka message router ─────────────────────────────────────────────────────
void handleKafkaMessage(const std::string& topic,
    const std::string& /*key*/,
    const nlohmann::json& msg,
    checkin::CacheService& cache,
    checkin::CheckinService& service) {
    try {
        if (topic == "flights.events") {
            // Expected: { eventId, type, flightId, newStatus, simTime }
            std::string event_type = msg.value("type", "");
            if (event_type != "flight.status.changed") return;

            std::string event_id = msg.value("eventId", "");
            std::string flight_id = msg.value("flightId", "");
            std::string new_status = msg.value("newStatus", "");
            std::string sim_time = msg.value("simTime", "");

            if (flight_id.empty()) return;

            // Idempotency
            if (!event_id.empty() && service.isEventProcessed(event_id)) {
                spdlog::debug("Kafka: event {} already processed, skipping", event_id);
                return;
            }

            auto status = checkin::flightStatusFromString(new_status);
            cache.updateFlight(flight_id, status, sim_time);
            spdlog::info("FlightCache: {} → {}", flight_id, new_status);

            if (status == checkin::FlightStatus::RegistrationOpen) {
                spdlog::info("RegistrationOpen received for {}, running auto check-in",
                    flight_id);
                // Run in the consumer thread (acceptable for moderate load)
                service.runAutoCheckin(flight_id, sim_time);
            }

            if (!event_id.empty()) service.markEventProcessed(event_id);

        }
        else if (topic == "tickets.events") {
            // Expected: { eventId, type, ticketId, flightId, passengerId, status }
            std::string event_type = msg.value("type", "");
            std::string event_id = msg.value("eventId", "");
            std::string ticket_id = msg.value("ticketId", "");
            std::string flight_id = msg.value("flightId", "");
            std::string passenger_id = msg.value("passengerId", "");

            if (ticket_id.empty()) return;

            if (!event_id.empty() && service.isEventProcessed(event_id)) {
                spdlog::debug("Kafka: event {} already processed, skipping", event_id);
                return;
            }

            if (event_type == "ticket.bought") {
                cache.upsertTicket(ticket_id, flight_id, passenger_id,
                    checkin::TicketStatus::Active);
                spdlog::info("TicketCache: bought {} for flight {}", ticket_id,
                    flight_id);
            }
            else if (event_type == "ticket.refunded") {
                cache.updateTicketStatus(ticket_id, checkin::TicketStatus::Refunded);
                spdlog::info("TicketCache: refunded {}", ticket_id);
            }
            else if (event_type == "ticket.bumped") {
                cache.updateTicketStatus(ticket_id, checkin::TicketStatus::Bumped);
                spdlog::info("TicketCache: bumped {}", ticket_id);
            }

            if (!event_id.empty()) service.markEventProcessed(event_id);
        }
    }
    catch (const std::exception& e) {
        spdlog::error("handleKafkaMessage error: {}", e.what());
    }
}

// ─── Main ─────────────────────────────────────────────────────────────────────
int main() {
    // Logger setup
    spdlog::set_default_logger(spdlog::stdout_color_mt("checkin"));
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v");

    spdlog::info("=== Check-in Service starting ===");

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // ── Config ────────────────────────────────────────────────────────────────
    auto cfg = checkin::Config::fromEnv();
    spdlog::info("Config: port={}, db={}, kafka={}", cfg.port, cfg.db_name,
        cfg.kafka_brokers);

    // ── Database ──────────────────────────────────────────────────────────────
    checkin::Database db(cfg.connectionString());

    // ── Migrations ────────────────────────────────────────────────────────────
    checkin::Migrator migrator(db);
    migrator.migrate();

    // ── Services ──────────────────────────────────────────────────────────────
    checkin::CacheService  cache;
    checkin::KafkaProducer producer(cfg.kafka_brokers);
    checkin::CheckinService service(db, cache, producer);

    // ── Outbox publisher ──────────────────────────────────────────────────────
    checkin::OutboxPublisher outbox(db, producer);
    outbox.start();

    // ── Kafka consumer ────────────────────────────────────────────────────────
    checkin::KafkaConsumer consumer(
        cfg.kafka_brokers,
        cfg.kafka_group_id,
        { cfg.topic_flights, cfg.topic_tickets }
    );

    consumer.setHandler([&](const std::string& topic,
        const std::string& key,
        const nlohmann::json& msg) {
            handleKafkaMessage(topic, key, msg, cache, service);
        });
    consumer.start();

    // ── HTTP server ───────────────────────────────────────────────────────────
    crow::SimpleApp app;

    checkin::handlers::registerCorsHandler(app);
    checkin::handlers::registerHealthRoute(app, db);
    checkin::handlers::registerVersionRoute(app);
    checkin::handlers::registerCheckinRoutes(app, service);

    spdlog::info("HTTP server listening on port {}", cfg.port);
    app.port(cfg.port)
        .multithreaded()
        .concurrency(4)
        .run_async();

    // ── Wait for shutdown ─────────────────────────────────────────────────────
    while (!g_shutdown) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    spdlog::info("Shutting down...");
    consumer.stop();
    outbox.stop();
    producer.flush(5000);
    app.stop();

    spdlog::info("=== Check-in Service stopped ===");
    return 0;
}