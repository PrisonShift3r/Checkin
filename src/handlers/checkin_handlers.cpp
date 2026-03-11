#include "handlers/handlers.h"
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

namespace checkin::handlers {

namespace {

crow::response jsonResponse(int status, const nlohmann::json& body) {
    crow::response res(status, body.dump());
    res.add_header("Content-Type", "application/json");
    return res;
}

crow::response errorResponse(int status, const std::string& code,
                              const std::string& message) {
    return jsonResponse(status, {{"code", code}, {"message", message}});
}

} // namespace

// ─── CORS OPTIONS preflight ────────────────────────────────────────────────────
void registerCorsHandler(crow::SimpleApp& app) {
    CROW_ROUTE(app, "/<path>")
    .methods(crow::HTTPMethod::OPTIONS)
    ([](const crow::request&, const std::string&) {
        crow::response res(204);
        res.add_header("Access-Control-Allow-Origin",  "*");
        res.add_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.add_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
        res.add_header("Access-Control-Max-Age",       "86400");
        return res;
    });
}

// ─── Checkin routes ────────────────────────────────────────────────────────────
void registerCheckinRoutes(crow::SimpleApp& app, CheckinService& service) {

    // ── GET /v1/checkins ──────────────────────────────────────────────────────
    CROW_ROUTE(app, "/v1/checkins")
    .methods(crow::HTTPMethod::GET)
    ([&service](const crow::request& req) {
        try {
            std::optional<std::string> flight_id, passenger_id, status_filter;

            if (const char* v = req.url_params.get("flightId"))
                flight_id = v;
            if (const char* v = req.url_params.get("passengerId"))
                passenger_id = v;
            if (const char* v = req.url_params.get("status"))
                status_filter = v;

            if (status_filter &&
                *status_filter != "started" &&
                *status_filter != "success" &&
                *status_filter != "failed") {
                return errorResponse(400, "validation_error",
                                     "status must be one of: started, success, failed");
            }

            auto checkins = service.listCheckins(flight_id, passenger_id, status_filter);
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& c : checkins) arr.push_back(c.toJson());
            return jsonResponse(200, arr);

        } catch (const std::exception& e) {
            spdlog::error("GET /v1/checkins error: {}", e.what());
            return errorResponse(500, "internal_error", e.what());
        }
    });

    // ── POST /v1/checkins/start ───────────────────────────────────────────────
    // Must be registered BEFORE /v1/checkins/<string> so Crow doesn't match
    // "start" as a checkin ID.
    CROW_ROUTE(app, "/v1/checkins/start")
    .methods(crow::HTTPMethod::POST)
    ([&service](const crow::request& req) {
        try {
            auto body = nlohmann::json::parse(req.body, nullptr, false);
            if (body.is_discarded())
                return errorResponse(400, "validation_error", "Invalid JSON body");

            if (!body.contains("flightId") ||
                !body.contains("passengerId") ||
                !body.contains("ticketId")) {
                return errorResponse(400, "validation_error",
                                     "flightId, passengerId, ticketId are required");
            }

            StartCheckinRequest r;
            r.flight_id    = body["flightId"].get<std::string>();
            r.passenger_id = body["passengerId"].get<std::string>();
            r.ticket_id    = body["ticketId"].get<std::string>();

            if (r.flight_id.empty() || r.passenger_id.empty() || r.ticket_id.empty())
                return errorResponse(400, "validation_error", "Fields must not be empty");

            auto result = service.startCheckin(r);

            if (std::holds_alternative<Checkin>(result))
                return jsonResponse(201, std::get<Checkin>(result).toJson());

            const auto& err = std::get<CheckinError>(result);
            return jsonResponse(err.http_status, err.error.toJson());

        } catch (const std::exception& e) {
            spdlog::error("POST /v1/checkins/start error: {}", e.what());
            return errorResponse(500, "internal_error", e.what());
        }
    });

    // ── POST /v1/checkins/run/{flightId} ─────────────────────────────────────
    // Debug endpoint: force auto check-in for a specific flight.
    CROW_ROUTE(app, "/v1/checkins/run/<string>")
    .methods(crow::HTTPMethod::POST)
    ([&service](const crow::request&, const std::string& flight_id) {
        try {
            if (flight_id.empty())
                return errorResponse(400, "validation_error", "flightId is required");

            spdlog::info("Debug run: triggering auto check-in for flight {}", flight_id);
            service.runAutoCheckin(flight_id, "");

            auto checkins = service.listCheckins(flight_id, std::nullopt,
                                                  std::optional<std::string>{"success"});
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& c : checkins) arr.push_back(c.toJson());

            return jsonResponse(200, nlohmann::json{
                {"flightId",  flight_id},
                {"triggered", true},
                {"checkins",  arr}
            });

        } catch (const std::exception& e) {
            spdlog::error("POST /v1/checkins/run/{} error: {}", flight_id, e.what());
            return errorResponse(500, "internal_error", e.what());
        }
    });

    // ── GET /v1/checkins/{checkinId}/history ──────────────────────────────────
    // Must be registered BEFORE /v1/checkins/<string> (more specific path).
    CROW_ROUTE(app, "/v1/checkins/<string>/history")
    .methods(crow::HTTPMethod::GET)
    ([&service](const crow::request&, const std::string& checkin_id) {
        try {
            auto history = service.getCheckinHistory(checkin_id);
            if (!history.has_value())
                return errorResponse(404, "not_found", "Check-in not found");

            nlohmann::json arr = nlohmann::json::array();
            for (const auto& entry : *history) arr.push_back(entry);
            return jsonResponse(200, arr);

        } catch (const std::exception& e) {
            spdlog::error("GET /v1/checkins/{}/history error: {}", checkin_id, e.what());
            return errorResponse(500, "internal_error", e.what());
        }
    });

    // ── GET /v1/checkins/{checkinId} ──────────────────────────────────────────
    // Must be registered LAST — catches any single-segment path.
    CROW_ROUTE(app, "/v1/checkins/<string>")
    .methods(crow::HTTPMethod::GET)
    ([&service](const crow::request&, const std::string& checkin_id) {
        try {
            auto c = service.getCheckin(checkin_id);
            if (!c) return errorResponse(404, "not_found", "Check-in not found");
            return jsonResponse(200, c->toJson());

        } catch (const std::exception& e) {
            spdlog::error("GET /v1/checkins/{} error: {}", checkin_id, e.what());
            return errorResponse(500, "internal_error", e.what());
        }
    });
}

} // namespace checkin::handlers
