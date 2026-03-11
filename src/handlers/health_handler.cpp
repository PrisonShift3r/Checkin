#include "handlers/handlers.h"
#include "utils/time_utils.h"
#include <crow.h>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

namespace checkin::handlers {

    void registerHealthRoute(crow::SimpleApp& app, Database& db) {
        CROW_ROUTE(app, "/health")
            .methods(crow::HTTPMethod::GET)
            ([]() {
            nlohmann::json body = {
                {"status",  "ok"},
                {"service", "checkin"},
                {"time",    utils::nowISO()}
            };
            crow::response res(200, body.dump());
            res.add_header("Content-Type", "application/json");
            return res;
                });
    }

    void registerVersionRoute(crow::SimpleApp& app) {
        CROW_ROUTE(app, "/version")
            .methods(crow::HTTPMethod::GET)
            ([]() {
            nlohmann::json body = {
                {"service", "checkin"},
                {"version", "1.0.0"},
                {"build",   __DATE__ " " __TIME__}
            };
            crow::response res(200, body.dump());
            res.add_header("Content-Type", "application/json");
            return res;
                });
    }

} // namespace checkin::handlers
