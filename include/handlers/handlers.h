#pragma once
#include "services/checkin_service.h"
#include "db/database.h"
#include <crow.h>

namespace checkin::handlers {

	// Health & meta
	void registerHealthRoute(crow::SimpleApp& app, Database& db);
	void registerVersionRoute(crow::SimpleApp& app);

	// Business routes
	void registerCheckinRoutes(crow::SimpleApp& app, CheckinService& service);

	// CORS preflight for all routes
	void registerCorsHandler(crow::SimpleApp& app);

} // namespace checkin::handlers
